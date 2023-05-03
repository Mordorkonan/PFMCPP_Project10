/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
//==============================================================================
ValueHolderBase::ValueHolderBase()
{
    startTimerHz(frameRate);
}

ValueHolderBase::~ValueHolderBase()
{
    stopTimer();
}

void ValueHolderBase::timerCallback()
{
    if (getNow() - peakTime > holdTime)
    {
        timerCallbackImpl();
    }
}

float ValueHolderBase::getCurrentValue() const { return currentValue; }

bool ValueHolderBase::getIsOverThreshold() const { return currentValue > threshold; }

void ValueHolderBase::setHoldTime(int ms) { holdTime = ms; }

void ValueHolderBase::setThreshold(float th) { threshold = th; }

juce::int64 ValueHolderBase::getNow() { return juce::Time::currentTimeMillis(); }

juce::int64 ValueHolderBase::getPeakTime() const { return peakTime; }

juce::int64 ValueHolderBase::getHoldTime() const { return holdTime; }

int ValueHolderBase::frameRate = 60;
//==============================================================================
ValueHolder::ValueHolder() { holdTime = 500; }

ValueHolder::~ValueHolder() = default;

void ValueHolder::updateHeldValue(float v)
{
    currentValue = v;

    if (getIsOverThreshold())
    {
        peakTime = getNow();
        if (v > heldValue)
        {
            heldValue = v;
        }
    }    
}

void ValueHolder::timerCallbackImpl()
{
    if (!getIsOverThreshold())
    {
        heldValue = NEGATIVE_INFINITY;
    }
}

float ValueHolder::getHeldValue() const { return heldValue; }
//==============================================================================
DecayingValueHolder::DecayingValueHolder() : decayRateMultiplier(3)
{
    setDecayRate(3);
}

DecayingValueHolder::~DecayingValueHolder() = default;

void DecayingValueHolder::updateHeldValue(float v)
{
    if (v > currentValue)
    {
        peakTime = getNow();
        currentValue = v;
        resetDecayRateMultiplier();
    }
}

void DecayingValueHolder::timerCallbackImpl()
{
    currentValue = juce::jlimit<float>(NEGATIVE_INFINITY,
        MAX_DECIBELS,
        currentValue - decayRatePerFrame * decayRateMultiplier);

    decayRateMultiplier++;

    if (currentValue <= NEGATIVE_INFINITY)
    {
        resetDecayRateMultiplier();
    }
}

void DecayingValueHolder::setDecayRate(float dbPerSec) { decayRatePerFrame = dbPerSec / frameRate; }

void DecayingValueHolder::resetDecayRateMultiplier() { decayRateMultiplier = 1; }
//==============================================================================
TextMeter::TextMeter() : cachedValueDb(NEGATIVE_INFINITY)
{
    valueHolder.setThreshold(0);
    valueHolder.updateHeldValue(NEGATIVE_INFINITY);
}

void TextMeter::update(float valueDb)
{
    cachedValueDb = valueDb;
    valueHolder.updateHeldValue(cachedValueDb);
}

void TextMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    juce::Colour textColor{ juce::Colours::white };
    auto valueToDisplay = NEGATIVE_INFINITY;
    //valueHolder.setThreshold(JUCE_LIVE_CONSTANT(0));
    auto now = ValueHolder::getNow();

    if (valueHolder.getIsOverThreshold() ||
        (now - valueHolder.getPeakTime() < valueHolder.getHoldTime()) &&
        valueHolder.getPeakTime() > valueHolder.getHoldTime())     // for plugin launch
    {
        g.setColour(juce::Colours::red);
        g.fillRect(bounds);
        textColor = juce::Colours::black;
        valueToDisplay = valueHolder.getHeldValue();
    }
    else
    {
        g.setColour(juce::Colours::black);
        g.fillRect(bounds);
        textColor = juce::Colours::white;
        valueToDisplay = valueHolder.getCurrentValue();
    }

    g.setColour(textColor);
    g.setFont(12);

    g.drawFittedText((valueToDisplay > NEGATIVE_INFINITY) ? juce::String(valueToDisplay, 1) : juce::String("-inf"),
        bounds,
        juce::Justification::centred,
        1);
}

void Meter::paint(juce::Graphics& g)
{
    using namespace juce;
    auto bounds = getLocalBounds();

    g.setColour(Colours::darkgrey);
    g.drawRect(bounds);

    bounds = bounds.reduced(1);

    float remappedPeakDb = jmap<float>(peakDb, NEGATIVE_INFINITY, MAX_DECIBELS, bounds.getBottom(), bounds.getY());

    g.setColour(Colours::white);
    g.fillRect(bounds.withY(remappedPeakDb + 1).withBottom(bounds.getBottom() - 1));
    // i like this implementation more, especially the last string in this function

    //decayingValueHolder.setThreshold(JUCE_LIVE_CONSTANT(0));
    g.setColour(decayingValueHolder.getIsOverThreshold() ? Colours::red : Colours::orange);

    float remappedTick = jmap<float>(decayingValueHolder.getCurrentValue(), NEGATIVE_INFINITY, MAX_DECIBELS, bounds.getBottom(), bounds.getY());
    g.fillRect(bounds.withY(remappedTick).withHeight(2));
}

void Meter::update(float dbLevel)
{
    peakDb = dbLevel;
    decayingValueHolder.updateHeldValue(peakDb);
}
//==============================================================================
void DbScale::paint(juce::Graphics& g)
{
    g.drawImage(bkgd, getLocalBounds().toFloat());
}

void DbScale::buildBackgroundImage(int dbDivision, juce::Rectangle<int> meterBounds, int minDb, int maxDb)
{
    if (minDb > maxDb)
        std::swap(minDb, maxDb);

    auto bounds = getBounds();
    if (bounds.isEmpty())
        return;

    bkgd = juce::Image(juce::Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    juce::Graphics gbkgd(bkgd);
    gbkgd.addTransform(juce::AffineTransform::scale(juce::Desktop::getInstance().getGlobalScaleFactor()));
    gbkgd.setColour(juce::Colours::white);
    for (auto& tick : getTicks(dbDivision, meterBounds.withY(16), minDb, maxDb))
    {
        gbkgd.drawFittedText(juce::String(tick.db),
            0,
            tick.y - 17, //JUCE_LIVE_CONSTANT(20),    // 17 is label cell height accounting font size
            getWidth(),
            getHeight() / ((maxDb - minDb) / dbDivision),
            juce::Justification::centred, 1);
    }
}

std::vector<Tick> DbScale::getTicks(int dbDivision, juce::Rectangle<int> meterBounds, int minDb, int maxDb)
{
    if (minDb > maxDb)
        std::swap(minDb, maxDb);

    std::vector<Tick> tickVector;
    tickVector.resize((maxDb - minDb) / dbDivision);
    tickVector.clear();

    for (int db = minDb; db <= maxDb; db += dbDivision)
    {
        Tick tick;
        tick.db = db;
        tick.y = juce::jmap(db, minDb, maxDb, meterBounds.getBottom(), meterBounds.getY());
        tickVector.push_back(tick);
    }

    return tickVector;
}
//==============================================================================
MacroMeter::MacroMeter(int orientation) :
    averager(ValueHolderBase::frameRate, NEGATIVE_INFINITY),
    orientation(orientation)
{
    addAndMakeVisible(avgMeter);
    addAndMakeVisible(peakMeter);
    addAndMakeVisible(textMeter);
}

MacroMeter::~MacroMeter() { averager.clear(NEGATIVE_INFINITY); }

bool MacroMeter::getOrientation() const { return orientation; }

void MacroMeter::update(float level)
{
    averager.add(level);
    avgMeter.update(averager.getAvg());
    peakMeter.update(level);
    textMeter.update(level);
}

void MacroMeter::resized()
{
    auto bounds = getLocalBounds();

    textMeter.setBounds(bounds.removeFromTop(16));

    if (getOrientation() == Left)
    {
        avgMeter.setBounds(bounds.removeFromLeft(20));
        peakMeter.setBounds(bounds.removeFromRight(3));
    }
    else if (getOrientation() == Right)
    {
        avgMeter.setBounds(bounds.removeFromRight(20));
        peakMeter.setBounds(bounds.removeFromLeft(3));
    }
}

juce::Rectangle<int> MacroMeter::getAvgMeterBounds() const { return avgMeter.getLocalBounds(); }
//==============================================================================
LabelWithBackground::LabelWithBackground(juce::String text_) : text(text_) { }

void LabelWithBackground::paint(juce::Graphics& g)
{
    g.drawImage(label, getLocalBounds().toFloat());
}

void LabelWithBackground::drawLabel()
{
    label = juce::Image(juce::Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    juce::Graphics glabel(label);
    glabel.setColour(juce::Colours::black);
    glabel.fillRect(getLocalBounds());
    glabel.setColour(juce::Colours::darkgrey);
    glabel.drawRect(getLocalBounds(), 2);
    glabel.setFont(18);
    glabel.drawFittedText(text, getLocalBounds(), juce::Justification::centred, 1);
}
//==============================================================================
StereoMeter::StereoMeter(juce::String labelText) : label(labelText)
{
    addAndMakeVisible(leftMacroMeter);
    addAndMakeVisible(rightMacroMeter);
    addAndMakeVisible(dbScale);
    addAndMakeVisible(label);
}

void StereoMeter::update(float levelLeft, float levelRight)
{
    leftMacroMeter.update(levelLeft);
    rightMacroMeter.update(levelRight);
    repaint();
}

void StereoMeter::resized()
{
    auto bounds = getLocalBounds().reduced(5);

    label.setBounds(bounds.removeFromBottom(25));
    label.drawLabel();

    leftMacroMeter.setBounds(bounds.removeFromLeft(25));
    rightMacroMeter.setBounds(bounds.removeFromRight(25));
    dbScale.setBounds(bounds);
    dbScale.buildBackgroundImage(6, leftMacroMeter.getAvgMeterBounds(), NEGATIVE_INFINITY, MAX_DECIBELS);
}
//==============================================================================
Histogram::Histogram(const juce::String& title_) : title(title_) { }

void Histogram::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced(5);

    g.setColour(juce::Colours::black);
    g.fillRect(bounds);
    g.setColour(juce::Colours::darkgrey);
    g.drawText(title, bounds, juce::Justification::centredBottom);

    displayPath(g, bounds.toFloat().reduced(1));
}

void Histogram::resized() { buffer.resize(getWidth(), NEGATIVE_INFINITY); }

void Histogram::mouseDown(const juce::MouseEvent& e)
{
    buffer.clear(NEGATIVE_INFINITY);
    repaint();
}

void Histogram::update(float value)
{
    buffer.write(value);
    repaint();
}

void Histogram::displayPath(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    juce::Path fill = buildPath(path, buffer, bounds);
    if (!fill.isEmpty())
    {
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.fillPath(fill);
        g.setColour(juce::Colours::white);
        g.strokePath(path,
                     juce::PathStrokeType(1));
    }
}

juce::Path Histogram::buildPath(juce::Path& p,
                                ReadAllAfterWriteCircularBuffer<float>& buffer,
                                juce::Rectangle<float> bounds)
{
    p.clear();
    auto size = buffer.getSize();
    auto& data = buffer.getData();
    auto readIndex = buffer.getReadIndex();

    auto map = [&bounds](float db) -> float
        { return juce::jmap(db, NEGATIVE_INFINITY, MAX_DECIBELS, bounds.getBottom(), bounds.getY()); };

    auto increment = [&size](size_t& readIndex)
    {
        if (readIndex == size - 1) { readIndex = 0; }
        else { ++readIndex; }
    };

    p.startNewSubPath(bounds.getX(), map(data[readIndex]));
    increment(readIndex);

    for (int x = 1; x < bounds.getWidth(); ++x)
    {
        p.lineTo(bounds.getX() + x, map(data[readIndex]));
        increment(readIndex);
    }
    
    if (p.getBounds().isEmpty()) { p.clear(); return p; }

    auto fill = p;
    fill.lineTo(bounds.getBottomRight());
    fill.lineTo(bounds.getBottomLeft());
    fill.closeSubPath();
    return fill;
}
//==============================================================================
Goniometer::Goniometer(juce::AudioBuffer<float>& buffer) : buffer(buffer) { internalBuffer = juce::AudioBuffer<float>(2, 256); }

void Goniometer::paint(juce::Graphics& g)
{
    p.clear();
    if (buffer.getNumSamples() >= JUCE_LIVE_CONSTANT(400)) { internalBuffer.makeCopyOf(buffer); } // 256
    else { internalBuffer.applyGain(juce::Decibels::decibelsToGain(-2.0f)); }
    
    auto bounds = getLocalBounds().withTrimmedLeft((getWidth() - getHeight()) / 2).withTrimmedRight((getWidth() - getHeight()) / 2).toFloat();

    g.drawImage(bkgd, bounds);

    auto map = [&](float value, float min, float max) -> float
    {
        value = juce::jmap(value,
            -1.0f,
            1.0f,
            min,
            max);

        return value;
    };

    auto reducedBounds = bounds.reduced(25).toFloat();

    for (int i = 0; i < internalBuffer.getNumSamples(); i += 2)
    {
        auto left = internalBuffer.getSample(0, i);
        auto right = internalBuffer.getSample(1, i);
        auto mid = (left + right) * conversionCoefficient;
        auto side = (left - right) * conversionCoefficient;

        juce::Point<float> node{ map(side, reducedBounds.getRight(), reducedBounds.getX()),
                                 map(mid, reducedBounds.getBottom(), reducedBounds.getY()) };

        // Lissajous curve limitation criteria v2

        if (center.getDistanceFrom(node) >= radius)
        {
            if (i == 0) { p.startNewSubPath(center.getPointOnCircumference(radius, center.getAngleToPoint(node))); }
            else { p.lineTo(center.getPointOnCircumference(radius, center.getAngleToPoint(node))); }
        }
        else
        {        
            if (i == 0) { p.startNewSubPath(node); }
            else { p.lineTo(node); }
        }
    }
    g.setColour(juce::Colours::white);
    g.strokePath(p, juce::PathStrokeType(1));
}

void Goniometer::drawBackground()
{
    auto bounds = getLocalBounds().withWidth(getHeight()).toFloat();

    bkgd = juce::Image(juce::Image::PixelFormat::ARGB, bounds.getWidth(), bounds.getHeight(), true);
    juce::Graphics gbkgd(bkgd);
    gbkgd.addTransform(juce::AffineTransform::scale(juce::Desktop::getInstance().getGlobalScaleFactor()));

    bounds = bounds.reduced(25);

    gbkgd.setColour(juce::Colours::black);
    gbkgd.fillEllipse(bounds);

    gbkgd.setColour(juce::Colours::darkgrey);
    gbkgd.drawEllipse(bounds, 1);

    juce::Line<float> axis{ bounds.getX(), bounds.getCentreY(), bounds.getRight(), bounds.getCentreY() };

    for (int i = 0; i < 4; ++i)
    {
        axis.applyTransform(juce::AffineTransform::rotation(juce::MathConstants<float>::pi / 4,
            bounds.getCentreX(),
            bounds.getCentreY()));

        gbkgd.drawLine(axis, 1.0f);
    }

    axis.applyTransform(juce::AffineTransform::scale(1.1f, 1.1f, bounds.getCentreX(), bounds.getCentreY()));
    juce::Rectangle<float> charBounds{ 25, 25 };
    gbkgd.setColour(juce::Colours::white);

    for (int i = 1; i <= 5; ++i)
    {
        gbkgd.drawText(chars[i - 1],
            charBounds.withCentre(juce::Point<float>(axis.getEndX(), axis.getEndY())),
            juce::Justification::centred);

        axis.applyTransform(juce::AffineTransform::rotation(juce::MathConstants<float>::pi / 4,
            bounds.getCentreX(),
            bounds.getCentreY()));
    }
}

void Goniometer::resized()
{
    radius = getLocalBounds().reduced(25).getHeight() / 2;  // radius of goniometer background
    center = getLocalBounds().getCentre().toFloat();
    drawBackground();
}
//==============================================================================
CorrelationMeter::CorrelationMeter(juce::AudioBuffer<float>& buf, double sampleRate) : buffer(buf)
{
    addAndMakeVisible(peakMeter);
    addAndMakeVisible(slowMeter);

    for (auto& filter : filters)
    {
        filter = juce::dsp::FIR::Filter<float>(juce::dsp::FilterDesign<float>
                                                        ::designFIRLowpassWindowMethod(100.0f,
                                                                                       sampleRate,
                                                                                       1,
                                                                                       juce::dsp::FilterDesign<float>::WindowingMethod::rectangular));
        filter.reset();        
    }
}

void CorrelationMeter::paint(juce::Graphics& g)
{
    auto labelWidth = 25;
    auto labelBounds = getLocalBounds().withWidth(labelWidth).toFloat();

    g.setColour(juce::Colours::white);
    g.drawText(chars[0], labelBounds, juce::Justification::centred);
    g.drawText(chars[1], labelBounds.withX(getLocalBounds().getRight() - labelWidth), juce::Justification::centred);
}

void CorrelationMeter::update()
{
    auto getFilteredSample = [&](int filterIndex, int sample1, int sample2) -> float
    {
        auto sample = filters[filterIndex].processSample(sample1 * sample2);
        return sample == 0 ? 1 : sample;
    };
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        // Pearson fitting criterion
        float processedSample = filters[0].processSample(buffer.getSample(0, i) * buffer.getSample(1, i)) /
                                std::sqrt(filters[1].processSample(buffer.getSample(0, i) * buffer.getSample(0, i)) * 
                                          filters[2].processSample(buffer.getSample(1, i) * buffer.getSample(1, i)));

        if (std::isnan(processedSample))
        {
            slowAverager.add(0);
            peakAverager.add(0);
        }
        else
        {
            slowAverager.add(processedSample);
            peakAverager.add(processedSample);
        }
    }
    
    peakMeter.update(peakAverager.getAvg());
    slowMeter.update(slowAverager.getAvg());
}

void CorrelationMeter::resized()
{
    auto bounds = getLocalBounds().withX(getLocalBounds().getX() + 25).withRight(getLocalBounds().getRight() - 25);
    peakMeter.setBounds(bounds.withHeight(3));
    slowMeter.setBounds(bounds.withY(5).withHeight(20));
}
//==============================================================================
StereoImageMeter::StereoImageMeter(juce::AudioBuffer<float>& buffer_, double sampleRate) :
goniometer(buffer_),
correlationMeter(buffer_, sampleRate)
{
    addAndMakeVisible(goniometer);
    addAndMakeVisible(correlationMeter);
}

void StereoImageMeter::update()
{
    correlationMeter.update();
    goniometer.repaint();
}

void StereoImageMeter::resized()
{
    auto bounds = getLocalBounds();

    goniometer.setBounds(bounds.removeFromTop(300).withTrimmedLeft((getWidth() - getHeight()) / 2).withTrimmedRight((getWidth() - getHeight()) / 2));
    correlationMeter.setBounds(goniometer.getBounds().withY(goniometer.getBottom() - 10).withHeight(25));
}
//==============================================================================
PFMCPP_Project10AudioProcessorEditor::PFMCPP_Project10AudioProcessorEditor (PFMCPP_Project10AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    addAndMakeVisible(rmsStereoMeter);
    addAndMakeVisible(peakStereoMeter);

    addAndMakeVisible(rmsHistogram);
    addAndMakeVisible(peakHistogram);

    addAndMakeVisible(stereoImageMeter);

    startTimerHz(ValueHolderBase::frameRate);
    setSize (700, 570);
}

PFMCPP_Project10AudioProcessorEditor::~PFMCPP_Project10AudioProcessorEditor()
{
}

void PFMCPP_Project10AudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)

    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    //reference = juce::ImageCache::getFromMemory(BinaryData::Reference_png, BinaryData::Reference_pngSize);
    //g.drawImage(reference, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
}

void PFMCPP_Project10AudioProcessorEditor::timerCallback()
{
    if (audioProcessor.audioBufferFifo.pull(buffer))
    {
        while (audioProcessor.audioBufferFifo.pull(buffer)) { }

        auto magDbLeft = juce::Decibels::gainToDecibels(buffer.getMagnitude(0, 0, buffer.getNumSamples()), NEGATIVE_INFINITY);
        auto magDbRight = juce::Decibels::gainToDecibels(buffer.getMagnitude(1, 0, buffer.getNumSamples()), NEGATIVE_INFINITY);

        auto rmsDbLeft = juce::Decibels::gainToDecibels(buffer.getRMSLevel(0, 0, buffer.getNumSamples()), NEGATIVE_INFINITY);
        auto rmsDbRight = juce::Decibels::gainToDecibels(buffer.getRMSLevel(1, 0, buffer.getNumSamples()), NEGATIVE_INFINITY);

        magDbLeft = juce::jlimit(NEGATIVE_INFINITY, MAX_DECIBELS, magDbLeft);
        magDbRight = juce::jlimit(NEGATIVE_INFINITY, MAX_DECIBELS, magDbRight);

        rmsDbLeft = juce::jlimit(NEGATIVE_INFINITY, MAX_DECIBELS, rmsDbLeft);
        rmsDbRight = juce::jlimit(NEGATIVE_INFINITY, MAX_DECIBELS, rmsDbRight);

        rmsStereoMeter.update(rmsDbLeft, rmsDbRight);
        peakStereoMeter.update(magDbLeft, magDbRight);

        rmsHistogram.update((rmsDbLeft + rmsDbRight) / 2);
        peakHistogram.update((magDbLeft + magDbRight) / 2);

        stereoImageMeter.update();
    }
}

void PFMCPP_Project10AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    peakHistogram.setBounds(bounds.removeFromBottom(120));
    rmsHistogram.setBounds(bounds.removeFromBottom(120));

    rmsStereoMeter.setBounds(bounds.removeFromLeft(85));
    peakStereoMeter.setBounds(bounds.removeFromRight(85));

    stereoImageMeter.setBounds(bounds);
}
