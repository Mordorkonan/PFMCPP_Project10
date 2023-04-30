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

void TextMeter::paintTextMeter(juce::Graphics& g)
{
    auto bounds = getBounds();
    juce::Colour textColor { juce::Colours::white };
    auto valueToDisplay = NEGATIVE_INFINITY;
    valueHolder.setThreshold(JUCE_LIVE_CONSTANT(0));
    auto now = ValueHolder::getNow();

    if  (valueHolder.getIsOverThreshold() || 
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
//==============================================================================
void Meter::paintMeter(juce::Graphics& g)
{
    using namespace juce;
    auto bounds = getBounds();

    g.setColour(Colours::darkgrey);
    g.drawRect(bounds, 2.0f);

    //bounds = bounds.reduced(2);

    float remappedPeakDb = jmap<float>(peakDb, NEGATIVE_INFINITY, MAX_DECIBELS, bounds.getBottom(), bounds.getY());

    g.setColour(Colours::white);
    g.fillRect(bounds.withY(remappedPeakDb).withBottom(bounds.getBottom()));
    // i like this implementation more, especially the last string in this function

    decayingValueHolder.setThreshold(JUCE_LIVE_CONSTANT(0));
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
void DbScale::paintScale(juce::Graphics& g)
{
    g.drawImage(bkgd, getBounds().toFloat());
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
{ }

MacroMeter::~MacroMeter() { averager.clear(NEGATIVE_INFINITY); }

bool MacroMeter::getOrientation() const { return orientation; }

void MacroMeter::paintMacro(juce::Graphics& g)
{
    textMeter.paintTextMeter(g);
    avgMeter.paintMeter(g);
    peakMeter.paintMeter(g);
}

void MacroMeter::update(float level)
{
    averager.add(level);
    avgMeter.update(averager.getAvg());
    peakMeter.update(level);
    textMeter.update(level);
}

void MacroMeter::resized()
{
    auto bounds = getBounds();
    int avgWidth = 20;
    int peakWidth = 2;

    textMeter.setBounds(bounds.withHeight(16));
    avgMeter.setBounds(bounds.withY(textMeter.getBottom()).withWidth(avgWidth).withBottom(bounds.getBottom()));
    peakMeter.setBounds(avgMeter.getBounds().withWidth(peakWidth));

    if (getOrientation() == Left)
    {
        avgMeter.setBounds(bounds.withY(textMeter.getBottom()).withWidth(avgWidth).withBottom(bounds.getBottom()));
        peakMeter.setBounds(avgMeter.getBounds().withX(avgMeter.getRight() + 3).withWidth(peakWidth));
    }
    else if (getOrientation() == Right)
    {
        peakMeter.setBounds(bounds.withY(textMeter.getBottom()).withWidth(peakWidth).withBottom(bounds.getBottom()));
        avgMeter.setBounds(peakMeter.getBounds().withX(peakMeter.getRight() + 3).withWidth(avgWidth));
    }
}

juce::Rectangle<int> MacroMeter::getAvgMeterBounds() const { return avgMeter.getLocalBounds(); }
//==============================================================================
LabelWithBackground::LabelWithBackground(juce::String labelName, juce::String labelText) :
    label(labelName, labelText)
{ }
void LabelWithBackground::paintLabel(juce::Graphics& g)
{
    g.setColour(juce::Colours::black);
    g.fillRect(getBounds());
    g.setColour(juce::Colours::darkgrey);
    g.drawRect(getBounds(), 2);
    g.setFont(18);
    g.drawFittedText(label.getText(), getBounds(), juce::Justification::centred, 1);
}
//==============================================================================
StereoMeter::StereoMeter(juce::String labelName, juce::String labelText) : label(labelName, labelText) { }

void StereoMeter::update(float levelLeft, float levelRight)
{
    leftMacroMeter.update(levelLeft);
    rightMacroMeter.update(levelRight);
    repaint();
}

void StereoMeter::paintStereoMeter(juce::Graphics& g)
{
    leftMacroMeter.paintMacro(g);
    rightMacroMeter.paintMacro(g);
    dbScale.paintScale(g);
    label.paintLabel(g);
}

void StereoMeter::resized()
{
    auto bounds = getBounds();
    auto macroBounds = bounds.withWidth(25).withHeight(310);

    leftMacroMeter.setBounds(macroBounds);
    rightMacroMeter.setBounds(macroBounds.withX(macroBounds.getX() + leftMacroMeter.getWidth() * 2));
    dbScale.setBounds(macroBounds.withX(leftMacroMeter.getRight()));
    dbScale.buildBackgroundImage(6, leftMacroMeter.getAvgMeterBounds(), NEGATIVE_INFINITY, MAX_DECIBELS);
    label.setBounds(bounds.withY(dbScale.getBottom()).withHeight(25));
}
//==============================================================================
Histogram::Histogram(const juce::String& title_) : title(title_) { }

void Histogram::paintHisto(juce::Graphics& g)
{
    g.setColour(juce::Colours::black);
    g.fillRect(getBounds());
    g.setColour(juce::Colours::darkgrey);
    g.drawText(title, getBounds(), juce::Justification::centredBottom);

    displayPath(g, getBounds().toFloat().reduced(1));
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

void Goniometer::resized()
{
    w = getWidth();
    h = getHeight();
    center = getLocalBounds().getCentre().toFloat();
    drawBackground();
}

void Goniometer::paintGoniometer(juce::Graphics& g)
{
    p.clear();
    if (buffer.getNumSamples() >= JUCE_LIVE_CONSTANT(400)) { internalBuffer.makeCopyOf(buffer); } // 256
    else { internalBuffer.applyGain(juce::Decibels::decibelsToGain(-2.0f)); }
    
    g.drawImage(bkgd, getBounds().toFloat());
    
    juce::Line<float> limitDistance{ getBounds().toFloat().getCentre(), getBounds().toFloat().getCentre() };

    auto map = [&](float value, float min, float max) -> float
    {
        auto amplitude = juce::Decibels::decibelsToGain(0.0f);
        value = juce::jmap(value,
                           -amplitude,
                           amplitude,
                           //NEGATIVE_INFINITY,
                           //MAX_DECIBELS,
                           min,
                           max);

        return value;
    };

    for (int i = 0; i < internalBuffer.getNumSamples(); i += 2)
    {
        auto left = internalBuffer.getSample(0, i);
        auto right = internalBuffer.getSample(1, i);
        auto mid = (left + right) * juce::Decibels::decibelsToGain(-3.0f);
        auto side = (left - right) * juce::Decibels::decibelsToGain(-3.0f);
        //auto mid = juce::Decibels::gainToDecibels((left + right) * juce::Decibels::decibelsToGain(-3.0f));
        //auto side = juce::Decibels::gainToDecibels((left - right) * juce::Decibels::decibelsToGain(-3.0f));
        auto reducedBounds = getBounds().reduced(25).toFloat();

        juce::Point<float> node{ map(side, reducedBounds.getRight(), reducedBounds.getX()),
                                 map(mid, reducedBounds.getBottom(), reducedBounds.getY()) };


        // Lissajous curve limitation criteria
        limitDistance.setEnd(node);

        float a = limitDistance.getLength();
        float b = reducedBounds.getHeight() / 2;

        if (limitDistance.getLength() <= reducedBounds.getHeight() / 2)
        {
            if (splitPath)
            {
                splitPath = false;
                p.startNewSubPath(node);
            }
            else
            {
                if (i == 0) { p.startNewSubPath(node); }
                else { p.lineTo(node); }
            }
        }
        else
        {
            splitPath = true;
        }
    }

    g.setColour(juce::Colours::white);
    g.strokePath(p, juce::PathStrokeType(1));
}

void Goniometer::drawBackground()
{
    auto bounds = getLocalBounds().reduced(25).toFloat();    

    bkgd = juce::Image(juce::Image::PixelFormat::ARGB, getWidth(), getHeight(), true);
    juce::Graphics gbkgd(bkgd);
    gbkgd.addTransform(juce::AffineTransform::scale(juce::Desktop::getInstance().getGlobalScaleFactor()));
    gbkgd.setColour(juce::Colours::black);
    gbkgd.fillEllipse(bounds);

    gbkgd.setColour(juce::Colours::darkgrey);
    gbkgd.drawEllipse(bounds, 1);

    juce::Line<float> axis{ bounds.getX(), center.getY(), bounds.getRight(), center.getY() };

    for (int i = 0; i < 4; ++i)
    {
        axis.applyTransform(juce::AffineTransform::rotation(juce::MathConstants<float>::pi / 4,
            center.getX(),
            center.getY()));
        gbkgd.drawLine(axis, 1.0f);
    }

    axis.applyTransform(juce::AffineTransform::scale(1.1f, 1.1f, center.getX(), center.getY()));
    juce::Rectangle<float> charBounds{ 25, 25 };
    gbkgd.setColour(juce::Colours::white);

    for (int i = 1; i <= 5; ++i)
    {
        gbkgd.drawText(chars[i - 1],
            charBounds.withCentre(juce::Point<float>(axis.getEndX(), axis.getEndY())),
            juce::Justification::centred);

        axis.applyTransform(juce::AffineTransform::rotation(juce::MathConstants<float>::pi / 4,
            center.getX(),
            center.getY()));
    }
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

    addAndMakeVisible(goniometer);

    startTimerHz(ValueHolderBase::frameRate);
    setSize (700, 572);
}

PFMCPP_Project10AudioProcessorEditor::~PFMCPP_Project10AudioProcessorEditor()
{
}

void PFMCPP_Project10AudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)

    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    rmsStereoMeter.paintStereoMeter(g);
    peakStereoMeter.paintStereoMeter(g);

    rmsHistogram.paintHisto(g);
    peakHistogram.paintHisto(g);

    goniometer.paintGoniometer(g);

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

        goniometer.repaint();
    }
}

void PFMCPP_Project10AudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    juce::Rectangle<int> stereoMeterBounds{ 10, 10, 75, 325 };
    rmsStereoMeter.setBounds(stereoMeterBounds);
    peakStereoMeter.setBounds(stereoMeterBounds.withX(getRight() - stereoMeterBounds.getWidth() - 10));

    stereoMeterBounds = stereoMeterBounds.withRight(peakStereoMeter.getRight())
                                         .withHeight((getHeight() - rmsStereoMeter.getBottom() - 10 * 3) / 2);
    
    rmsHistogram.setBounds(stereoMeterBounds.withY(rmsStereoMeter.getBottom() + 10));
    peakHistogram.setBounds(rmsHistogram.getBounds().withY(rmsHistogram.getBottom() + 10));

    int gonioSize{ 300 };
    goniometer.setBounds(getWidth() / 2 - gonioSize / 2, gonioSize / 12, gonioSize, gonioSize);
}
// FREE FUNCTIONS ==============================================================
