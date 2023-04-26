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

//int ValueHolderBase::getFrameRate() const { return frameRate; }

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
void TextMeter::paintTextMeter(juce::Graphics& g)
{
    auto bounds = getBounds();
    juce::Colour textColor { juce::Colours::white };
    auto valueToDisplay = NEGATIVE_INFINITY;
    valueHolder.setThreshold(JUCE_LIVE_CONSTANT(0));
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
//==============================================================================
PFMCPP_Project10AudioProcessorEditor::PFMCPP_Project10AudioProcessorEditor (PFMCPP_Project10AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.

    addAndMakeVisible(peakMacroMeter);
    addAndMakeVisible(avgMacroMeter);
    addAndMakeVisible(peakLabel);
    addAndMakeVisible(avgLabel);
    addAndMakeVisible(peakScale);
    addAndMakeVisible(avgScale);

    startTimerHz(ValueHolderBase::frameRate);
    setSize (700, 572);
}

PFMCPP_Project10AudioProcessorEditor::~PFMCPP_Project10AudioProcessorEditor()
{
}

//==============================================================================
void PFMCPP_Project10AudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    referenceImage = juce::ImageCache::getFromMemory(BinaryData::Reference_png, BinaryData::Reference_pngSize);
    g.drawImageWithin(referenceImage, 0, 0, getWidth(), getHeight(), juce::RectanglePlacement::stretchToFit);

    avgMacroMeter.paintMacro(g);
    peakMacroMeter.paintMacro(g);
    avgLabel.paintLabel(g);
    peakLabel.paintLabel(g);
    avgScale.paintScale(g);
    peakScale.paintScale(g);
}
//==============================================================================
void Meter::paintMeter(juce::Graphics& g)
{
    auto bounds = getBounds();

    g.setColour(juce::Colours::darkgrey);
    g.drawRect(bounds, 2);

    bounds.reduced(2);

    float remappedPeakDb = juce::jmap<float>(peakDb, NEGATIVE_INFINITY, MAX_DECIBELS, bounds.getBottom(), bounds.getY());

    g.setColour(juce::Colours::white);
    g.fillRect(bounds.withY(remappedPeakDb).withBottom(bounds.getBottom()));

    decayingValueHolder.setThreshold(JUCE_LIVE_CONSTANT(0));
    g.setColour(decayingValueHolder.getIsOverThreshold() ? juce::Colours::red : juce::Colours::orange);

    float remappedTick = juce::jmap<float>(decayingValueHolder.getCurrentValue(), NEGATIVE_INFINITY, MAX_DECIBELS, bounds.getBottom(), bounds.getY());
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

    auto bounds = getLocalBounds();
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
MacroMeter::MacroMeter(bool useAverage) : 
averagerLeft(ValueHolderBase::frameRate, NEGATIVE_INFINITY),
averagerRight(ValueHolderBase::frameRate, NEGATIVE_INFINITY),
averageMeasure(useAverage)
{ }

MacroMeter::~MacroMeter()
{
    averagerLeft.clear(NEGATIVE_INFINITY);
    averagerRight.clear(NEGATIVE_INFINITY);
}

juce::Rectangle<int> MacroMeter::getLeftMeterBounds() const { return meterLeft.getLocalBounds(); }

bool MacroMeter::isAverageMeasure() const { return averageMeasure; }

void MacroMeter::paintMacro(juce::Graphics& g)
{
    g.setColour(juce::Colours::black);
    g.drawRect(getBounds(), 2);

    textMeterLeft.paintTextMeter(g);
    textMeterRight.paintTextMeter(g);

    meterLeft.paintMeter(g);
    meterRight.paintMeter(g);
}

void MacroMeter::update(float levelLeft, float levelRight)
{
    if (isAverageMeasure())
    {    
        averagerLeft.add(levelLeft);
        averagerRight.add(levelRight);
        meterLeft.update(averagerLeft.getAvg());
        meterRight.update(averagerRight.getAvg());
        textMeterLeft.update(averagerLeft.getAvg());
        textMeterRight.update(averagerRight.getAvg());
    }
    else
    {
        meterLeft.update(levelLeft);
        meterRight.update(levelRight);
        textMeterLeft.update(levelLeft);
        textMeterRight.update(levelRight);
    }
    repaint();
}

void MacroMeter::resized()
{
    auto bounds = getBounds();
    int meterWidth = 25;

    textMeterLeft.setBounds(bounds.withWidth(meterWidth).withHeight(16));
    textMeterRight.setBounds(textMeterLeft.getBounds().withX(textMeterLeft.getRight() + meterWidth));

    bounds = bounds.withY(textMeterLeft.getBottom()).withBottom(getBottom()).withWidth(meterWidth);

    meterLeft.setBounds(bounds);
    meterRight.setBounds(bounds.withX(textMeterRight.getX()));
}
//==============================================================================
void PFMCPP_Project10AudioProcessorEditor::timerCallback()
{
    if (audioProcessor.audioBufferFifo.pull(buffer))
    {
        while (audioProcessor.audioBufferFifo.pull(buffer))
        {

        }
        auto magDbLeft = juce::Decibels::gainToDecibels(buffer.getMagnitude(0, 0, buffer.getNumSamples()), NEGATIVE_INFINITY);
        auto magDbRight = juce::Decibels::gainToDecibels(buffer.getMagnitude(1, 0, buffer.getNumSamples()), NEGATIVE_INFINITY);
        peakMacroMeter.update(magDbLeft, magDbRight);
        avgMacroMeter.update(magDbLeft, magDbRight);
    }
}

void PFMCPP_Project10AudioProcessorEditor::resized()
{
    juce::Rectangle<int> macroBounds { 10, 10, 75, 310 };

    avgMacroMeter.setBounds(macroBounds);
    peakMacroMeter.setBounds(macroBounds.withX(getRight() - macroBounds.getWidth() - 10).withY(10));

    avgScale.setBounds(avgMacroMeter.getBounds().withX(avgMacroMeter.getX() + 25).withWidth(25));
    peakScale.setBounds(peakMacroMeter.getBounds().withX(peakMacroMeter.getX() + 25).withWidth(25));

    avgScale.buildBackgroundImage(6, avgMacroMeter.getLeftMeterBounds(), NEGATIVE_INFINITY, MAX_DECIBELS);
    peakScale.buildBackgroundImage(6, peakMacroMeter.getLeftMeterBounds(), NEGATIVE_INFINITY, MAX_DECIBELS);

    avgLabel.setBounds(macroBounds.withY(avgMacroMeter.getBottom()).withHeight(25));
    peakLabel.setBounds(macroBounds.withX(peakMacroMeter.getX()).withY(peakMacroMeter.getBottom()).withHeight(25));
}
