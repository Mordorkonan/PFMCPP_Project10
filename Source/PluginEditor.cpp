/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
//==============================================================================
void NewLNF::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPos, float minSliderPos, float maxSliderPos,
                              const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    g.setColour(juce::Colours::red);
    g.drawRect(juce::Rectangle<float>{ static_cast<float>(x), sliderPos - 1.0f, static_cast<float>(width), 2.0f });
}
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

float ValueHolderBase::getThreshold() const { return threshold; }

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
        if (holdTime == 0)
        {
            heldValue = v;
        }
        else
        {
            peakTime = getNow();
            if (v > heldValue)
            {
                heldValue = v;
            }
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
DecayingValueHolder::DecayingValueHolder() : decayRateMultiplier(1/*3*/)
{
    setDecayRate(3);
}

DecayingValueHolder::~DecayingValueHolder() = default;

void DecayingValueHolder::updateHeldValue(float v)
{
    if (v > currentValue || v == NEGATIVE_INFINITY)
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

    decayRateMultiplier += 0.05f;

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

void TextMeter::setThreshold(float threshold) { valueHolder.setThreshold(threshold); }

void TextMeter::setHoldDuration(int newDuration) { valueHolder.setHoldTime(newDuration); }

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


    // +++ FIX THIS CONDITION +++
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
//==============================================================================
void Meter::setThreshold(float threshold) { decayingValueHolder.setThreshold(threshold); }

void Meter::toggleTicks(bool toggleState) { showTicks = toggleState; }

void Meter::setDecayRate(float dbPerSec) { decayingValueHolder.setDecayRate(dbPerSec); }

void Meter::setHoldDuration(int newDuration) { decayingValueHolder.setHoldTime(newDuration); }

void Meter::resetHeldValue() { decayingValueHolder.updateHeldValue(NEGATIVE_INFINITY); }

void Meter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.setColour(juce::Colours::darkgrey);
    g.drawRect(bounds);

    bounds = bounds.reduced(1);

    auto remap = [&](float value) -> float
    {
        return juce::jmap<float>(value,
            NEGATIVE_INFINITY,
            MAX_DECIBELS,
            bounds.getBottom(),
            bounds.getY());
    };

    g.setColour(juce::Colours::white);
    // jmax because higher threshold value -> lesser Y value
    g.fillRect(bounds.withY(juce::jmax(remap(peakDb), remap(decayingValueHolder.getThreshold()))).withBottom(bounds.getBottom()));  

    if (decayingValueHolder.getIsOverThreshold())
    {
        g.setColour(juce::Colours::orange);
        g.fillRect(bounds.withY(remap(peakDb)).withBottom(remap(decayingValueHolder.getThreshold())));
    }

    if (showTicks)
    {
        g.setColour(decayingValueHolder.getIsOverThreshold() ? juce::Colours::red : juce::Colours::lime);
        g.fillRect(bounds.withY(remap(decayingValueHolder.getCurrentValue())).withHeight(2));
    }
}

void Meter::update(float Level)
{
    peakDb = Level;
    decayingValueHolder.updateHeldValue(peakDb);
    repaint();
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

void MacroMeter::showMeters(const juce::String& meter)
{
    if (meter == "AVG")
    {
        avgMeter.setVisible(true);
        peakMeter.setVisible(false);
    }
    else if (meter == "PEAK")
    {
        avgMeter.setVisible(false);
        peakMeter.setVisible(true);
    }
    else
    {
        avgMeter.setVisible(true);
        peakMeter.setVisible(true);
    }
}

void MacroMeter::toggleTicks(bool toggleState)
{
    avgMeter.toggleTicks(toggleState);
    peakMeter.toggleTicks(toggleState);
}

void MacroMeter::setThreshold(float threshold)
{
    textMeter.setThreshold(threshold);
    peakMeter.setThreshold(threshold);
    avgMeter.setThreshold(threshold);
}

void MacroMeter::setHoldDuration(int newDuration)
{
    avgMeter.setHoldDuration(newDuration);
    peakMeter.setHoldDuration(newDuration);
    textMeter.setHoldDuration(newDuration);
}

void MacroMeter::setAvgDuration(float avgDuration) { averager.resize(avgDuration, NEGATIVE_INFINITY); }

void MacroMeter::resetHeldValue()
{
    avgMeter.resetHeldValue();
    peakMeter.resetHeldValue();
}

void MacroMeter::setDecayRate(float dbPerSec)
{
    avgMeter.setDecayRate(dbPerSec);
    peakMeter.setDecayRate(dbPerSec);
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

int MacroMeter::getTextMeterHeight() const { return textMeter.getHeight(); }
//==============================================================================
StereoMeter::StereoMeter(juce::String labelName, juce::String labelText) : label(labelName, labelText)
{
    addAndMakeVisible(leftMacroMeter);
    addAndMakeVisible(rightMacroMeter);
    addAndMakeVisible(dbScale);
    addAndMakeVisible(label);

    addAndMakeVisible(thresholdSlider);
    thresholdSlider.setRange(NEGATIVE_INFINITY, MAX_DECIBELS);

    label.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    label.setColour(juce::Label::outlineColourId, juce::Colours::darkgrey);
    label.setColour(juce::Label::textColourId, juce::Colours::darkgrey);
    label.setFont(18);
}

// empty ref in threshold slider cause newLNF is deleted earlier
StereoMeter::~StereoMeter() { thresholdSlider.setLookAndFeel(nullptr); }

void StereoMeter::showMeters(const juce::String& meter)
{
    leftMacroMeter.showMeters(meter);
    rightMacroMeter.showMeters(meter);
}

void StereoMeter::toggleTicks(bool toggleState)
{
    leftMacroMeter.toggleTicks(toggleState);
    rightMacroMeter.toggleTicks(toggleState);
}

void StereoMeter::setThreshold(float threshold)
{
    leftMacroMeter.setThreshold(threshold);
    rightMacroMeter.setThreshold(threshold);
}

void StereoMeter::setHoldDuration(int newDuration)
{
    leftMacroMeter.setHoldDuration(newDuration);
    rightMacroMeter.setHoldDuration(newDuration);
}

void StereoMeter::resetHeldValue()
{
    leftMacroMeter.resetHeldValue();
    rightMacroMeter.resetHeldValue();
}

void StereoMeter::setDecayRate(float dbPerSec)
{
    leftMacroMeter.setDecayRate(dbPerSec);
    rightMacroMeter.setDecayRate(dbPerSec);
}

void StereoMeter::setAverageDuration(float avgDuration)
{
    leftMacroMeter.setAvgDuration(avgDuration);
    rightMacroMeter.setAvgDuration(avgDuration);
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

    leftMacroMeter.setBounds(bounds.removeFromLeft(25));
    rightMacroMeter.setBounds(bounds.removeFromRight(25));
    dbScale.setBounds(bounds);
    dbScale.buildBackgroundImage(6, leftMacroMeter.getAvgMeterBounds(), NEGATIVE_INFINITY, MAX_DECIBELS);
    thresholdSlider.setBounds(bounds.removeFromBottom(bounds.getHeight() - leftMacroMeter.getTextMeterHeight()).expanded(0, 12));
}
//==============================================================================
Histogram::Histogram(const juce::String& title_) : title(title_) { }

void Histogram::setThreshold(float newThreshold) { threshold = newThreshold; }

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
        juce::ColourGradient gradient;
        float remappedThreshold = juce::jmap(threshold, NEGATIVE_INFINITY, MAX_DECIBELS, 0.01f, 1.0f);

        gradient.addColour(0.0, juce::Colours::white.withAlpha(0.15f));
        gradient.addColour(remappedThreshold - 0.01, juce::Colours::white.withAlpha(0.15f));
        gradient.addColour(remappedThreshold, juce::Colours::red.withAlpha(0.45f));
        gradient.addColour(1.0f, juce::Colours::red.withAlpha(0.45f));
        
        gradient.point1 = bounds.getBottomLeft();
        gradient.point2 = bounds.getTopLeft();

        g.setGradientFill(gradient);
        g.fillPath(fill);
        g.setColour(juce::Colours::white);
        g.strokePath(path, juce::PathStrokeType(1));
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
HistogramContainer::HistogramContainer()
{
    addAndMakeVisible(rmsHistogram);
    addAndMakeVisible(peakHistogram);
}

void HistogramContainer::setFlex(juce::FlexBox::Direction directionType, juce::Rectangle<int> bounds)
{
    juce::FlexBox layout;
    juce::Array<juce::FlexItem> items;

    layout.flexDirection = directionType;
    layout.flexWrap = juce::FlexBox::Wrap::noWrap;
    layout.alignContent = juce::FlexBox::AlignContent::spaceAround;
    layout.alignItems = juce::FlexBox::AlignItems::stretch;
    layout.justifyContent = juce::FlexBox::JustifyContent::spaceAround;

    items.add(juce::FlexItem(rmsHistogram).withFlex(0.25f));
    items.add(juce::FlexItem(peakHistogram).withFlex(0.25f));

    layout.items = items;
    layout.performLayout(bounds);
}

void HistogramContainer::resized() { setFlex(juce::FlexBox::Direction::column, getLocalBounds()); }
//==============================================================================
Goniometer::Goniometer(juce::AudioBuffer<float>& buffer) : buffer(buffer) { internalBuffer = juce::AudioBuffer<float>(2, 256); }

void Goniometer::setScale(float& coefficient) { scaleCoefficient = coefficient; }

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
        auto mid = (left + right) * conversionCoefficient * scaleCoefficient;
        auto side = (left - right) * conversionCoefficient * scaleCoefficient;

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
    for (auto& filter : filters)
    {
        filter = juce::dsp::FIR::Filter<float>(juce::dsp::FilterDesign<float>
                                                        ::designFIRLowpassWindowMethod(100.0f,
                                                                                       sampleRate,
                                                                                       3,
                                                                                       juce::dsp::FilterDesign<float>::WindowingMethod::rectangular));
        filter.reset();        
    }
}

void CorrelationMeter::paint(juce::Graphics& g)
{
    auto labelWidth = 25;
    auto labelBounds = getLocalBounds().withWidth(labelWidth).toFloat();
    auto meterBounds = getLocalBounds().toFloat().withTrimmedLeft(labelWidth).withTrimmedRight(labelWidth);

    g.setColour(juce::Colours::darkgrey);
    g.drawRect(meterBounds.withHeight(3));
    g.drawRect(meterBounds.withHeight(20).translated(0, 5));
    g.setColour(juce::Colours::white);
    g.drawText(chars[0], labelBounds, juce::Justification::centred);
    g.drawText(chars[1], labelBounds.withX(getLocalBounds().getRight() - labelWidth), juce::Justification::centred);

    auto centerX = meterBounds.toFloat().getCentreX();
    auto remap = [&](float value) -> float
    {
        return juce::jmap<float>(value, -1, 1, meterBounds.getX(), meterBounds.getRight());
    };

    fillMeter(g, meterBounds.withHeight(3), remap(peakAverager.getAvg()), centerX);
    fillMeter(g, meterBounds.withHeight(20).translated(0, 5), remap(slowAverager.getAvg()), centerX);
}

void CorrelationMeter::fillMeter(juce::Graphics & g, juce::Rectangle<float>& bounds, float edgeX1, float edgeX2)
{
    if (edgeX1 < edgeX2) { std::swap(edgeX1, edgeX2); }
    bounds = bounds.withX(edgeX2).withRight(edgeX1);
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.fillRect(bounds);
    g.setColour(juce::Colours::white);
    g.drawRect(bounds);
}

void CorrelationMeter::update()
{
    auto bufferData = buffer.getArrayOfReadPointers();
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        // Pearson fitting criterion
        float sqrt = std::sqrt(filters[1].processSample(bufferData[0][i] * bufferData[0][i]) *
            filters[2].processSample(bufferData[1][i] * bufferData[1][i]));

        if (std::isnan(sqrt) || std::isinf(sqrt) || sqrt == 0)
        {
            slowAverager.add(0);
            peakAverager.add(0);
        }
        else
        {
            float processedSample = filters[0].processSample(bufferData[0][i] * bufferData[1][i]) / sqrt;

            slowAverager.add(processedSample);
            peakAverager.add(processedSample);
        }
    }
    repaint();
}
//==============================================================================
StereoImageMeter::StereoImageMeter(juce::AudioBuffer<float>& buffer_, double sampleRate) :
goniometer(buffer_),
correlationMeter(buffer_, sampleRate)
{
    addAndMakeVisible(goniometer);
    addAndMakeVisible(correlationMeter);
}

void StereoImageMeter::setGoniometerScale(float coefficient)
{
    goniometer.setScale(coefficient);
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

    addAndMakeVisible(histogramContainer);

    addAndMakeVisible(stereoImageMeter);

    addAndMakeVisible(meterView);
    addAndMakeVisible(holdDuration);
    addAndMakeVisible(decayRate);
    addAndMakeVisible(avgDuration);
    addAndMakeVisible(histogramView);

    addAndMakeVisible(resetHold);
    addAndMakeVisible(enableHold);

    addAndMakeVisible(goniometerScale);

    rmsStereoMeter.thresholdSlider.setLookAndFeel(&newLNF);
    peakStereoMeter.thresholdSlider.setLookAndFeel(&newLNF);

    rmsStereoMeter.thresholdSlider.onValueChange = [this]()
    {
        auto newThreshold = rmsStereoMeter.thresholdSlider.getValue();
        rmsStereoMeter.setThreshold(newThreshold);
        histogramContainer.rmsHistogram.setThreshold(newThreshold);
    };

    peakStereoMeter.thresholdSlider.onValueChange = [this]()
    {
        auto newThreshold = peakStereoMeter.thresholdSlider.getValue();
        peakStereoMeter.setThreshold(newThreshold);
        histogramContainer.peakHistogram.setThreshold(newThreshold);
    };

    juce::StringArray meterLines{ "AVG", "PEAK", "BOTH" };
    meterView.addItemList(meterLines, 1);
    meterView.setSelectedItemIndex(2);
    meterView.onChange = [this]()
    {
        rmsStereoMeter.showMeters(meterView.getText());
        peakStereoMeter.showMeters(meterView.getText());
    };

    juce::StringArray durationKeys{ "0.0s", "0.5s", "2.0s", "4.0s", "6.0s", "inf" };
    holdDuration.addItemList(durationKeys, 1);
    holdDuration.setSelectedItemIndex(1);
    holdDuration.onChange = [this]()
    {
        juce::Array<int> durationMs{ 0, 500, 2000, 4000, 6000, std::numeric_limits<int>::max() };
        int newDuration{ durationMs[holdDuration.getSelectedItemIndex()] };

        if (newDuration == std::numeric_limits<int>::max()) { resetHold.setVisible(true); }
        else { resetHold.setVisible(false); }
        rmsStereoMeter.setHoldDuration(newDuration);
        peakStereoMeter.setHoldDuration(newDuration);
    };

    juce::StringArray decayKeys{ "-3.0dB/s", "-6.0dB/s", "-12.0dB/s", "-24.0dB/s", "-36.0dB/s" };
    decayRate.addItemList(decayKeys, 1);
    decayRate.setSelectedItemIndex(1);
    decayRate.onChange = [this]()
    {
        juce::Array<float> decayRates{ 3.0f, 6.0f, 12.0f, 24.0f, 36.0f };
        float dbPerSec = decayRates[decayRate.getSelectedItemIndex()];
        rmsStereoMeter.setDecayRate(dbPerSec);
        peakStereoMeter.setDecayRate(dbPerSec);
    };

    juce::StringArray avgDurationKeys{ "100ms", "250ms", "500ms", "1000ms", "2000ms" };
    avgDuration.addItemList(avgDurationKeys, 1);
    avgDuration.setSelectedItemIndex(2);
    avgDuration.onChange = [this]()
    {
        juce::Array<float> avgDurations{ 0.10f, 0.25f, 0.50f, 1.0f, 2.0f };
        float newDuration{ avgDurations[avgDuration.getSelectedItemIndex()] * ValueHolderBase::frameRate };
        
        rmsStereoMeter.setAverageDuration(newDuration);
        peakStereoMeter.setAverageDuration(newDuration);
    };

    juce::StringArray histogramKeys{ "Stacked", "Side-by-Side" };
    histogramView.addItemList(histogramKeys, 1);
    histogramView.setSelectedItemIndex(0);
    histogramView.onChange = [this]()
    {
        if (histogramView.getSelectedItemIndex() == 0) { histogramContainer.setFlex(juce::FlexBox::Direction::column, histogramContainer.getLocalBounds()); }
        else { histogramContainer.setFlex(juce::FlexBox::Direction::row, histogramContainer.getLocalBounds()); }
    };

    resetHold.setVisible(false);
    resetHold.onClick = [this]()
    {
        rmsStereoMeter.resetHeldValue();
        peakStereoMeter.resetHeldValue();
    };

    enableHold.setToggleState(true, juce::NotificationType::sendNotification);
    enableHold.onStateChange = [this]()
    {
        rmsStereoMeter.toggleTicks(enableHold.getToggleState());
        peakStereoMeter.toggleTicks(enableHold.getToggleState());
    };
    goniometerScale.setRange(0.5f, 2.0f);
    goniometerScale.setValue(1.0f);
    goniometerScale.onValueChange = [this]()
    {
        stereoImageMeter.setGoniometerScale(goniometerScale.getValue());
    };

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

    g.setColour(juce::Colours::red);
    g.drawRect(histogramContainer.getBounds());

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

        histogramContainer.rmsHistogram.update((rmsDbLeft + rmsDbRight) / 2);
        histogramContainer.peakHistogram.update((magDbLeft + magDbRight) / 2);

        stereoImageMeter.update();
    }
}

void PFMCPP_Project10AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    histogramContainer.setBounds(bounds.removeFromBottom(240));

    rmsStereoMeter.setBounds(bounds.removeFromLeft(85));
    peakStereoMeter.setBounds(bounds.removeFromRight(85));

    stereoImageMeter.setBounds(bounds);

    meterView.setBounds(100, 10, 120, 25);
    holdDuration.setBounds(meterView.getBounds().translated(0, 30));
    resetHold.setBounds(holdDuration.getBounds().translated(0, 30));
    enableHold.setBounds(resetHold.getBounds().translated(0, 30));
    decayRate.setBounds(enableHold.getBounds().translated(0, 30));
    avgDuration.setBounds(decayRate.getBounds().translated(0, 30));
    histogramView.setBounds(avgDuration.getBounds().translated(0, 30));
    goniometerScale.setBounds(500, 10, 100, 100);
}
