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

void TextMeter::paintTextMeter(juce::Graphics& g, float offsetX, float offsetY)
{
    auto bounds = getLocalBounds().withX(offsetX).withY(offsetY);
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
PFMCPP_Project10AudioProcessorEditor::PFMCPP_Project10AudioProcessorEditor (PFMCPP_Project10AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.

    addAndMakeVisible(peakMacroMeter);
    addAndMakeVisible(avgMacroMeter);

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
}
//==============================================================================
void Meter::paintMeter(juce::Graphics& g, float offsetX, float offsetY)
{
    using namespace juce;
    auto bounds = getLocalBounds().toFloat().withX(offsetX).withY(offsetY);

    g.setColour(Colours::darkgrey);
    g.drawRect(bounds, 2.0f);

    bounds = bounds.reduced(2);

    float remappedPeakDb = jmap<float>(peakDb, NEGATIVE_INFINITY, MAX_DECIBELS, bounds.getBottom(), bounds.getY());

    g.setColour(Colours::white);
    g.fillRect(bounds.withY(remappedPeakDb).withBottom(bounds.getBottom()));
    // i like this implementation more, especially the last string in this function

    decayingValueHolder.setThreshold(JUCE_LIVE_CONSTANT(0));
    g.setColour(decayingValueHolder.getIsOverThreshold() ? Colours::red : Colours::orange);

    float remappedTick = jmap<float>(decayingValueHolder.getCurrentValue(), NEGATIVE_INFINITY, MAX_DECIBELS, bounds.getBottom(), bounds.getY());
    g.fillRect(bounds.withY(remappedTick).withHeight(4));
}

void Meter::update(float dbLevel)
{
    peakDb = dbLevel;
    decayingValueHolder.updateHeldValue(peakDb);
}

void DbScale::paintScale(juce::Graphics& g, float offsetX, float offsetY)
{
    g.drawImage(bkgd, getLocalBounds().toFloat().withX(offsetX).withY(offsetY));
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

juce::Rectangle<int> MacroMeter::getPeakMeterBounds() const { return meterLeft.getLocalBounds(); }

bool MacroMeter::isAverageMeasure() const { return averageMeasure; }

void MacroMeter::drawLabel(juce::Graphics& g)
{
    if (isAverageMeasure())
    {
        label.setText("L RMS R", juce::dontSendNotification);
    }
    else
    {
        label.setText("L PEAK R", juce::dontSendNotification);
    }
    g.setColour(juce::Colours::black);
    g.fillRect(label.getBounds().withY(getHeight() - 25));
    g.setColour(juce::Colours::darkgrey);
    g.drawRect(label.getBounds().withY(getHeight() - 25), 2);
    g.setFont(18);
    g.drawFittedText(label.getText(), label.getBounds().withY(getHeight() - 25), juce::Justification::centred, 1);
}

void MacroMeter::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    meterLeft.paintMeter(g, 0.0f, textMeterLeft.getHeight());
    meterRight.paintMeter(g, meterLeft.getWidth() + dbScale.getWidth(), textMeterRight.getHeight());
    textMeterLeft.paintTextMeter(g, 0.0f, 0.0f);
    textMeterRight.paintTextMeter(g, meterLeft.getWidth() + dbScale.getWidth(), 0.0f);
    dbScale.paintScale(g, meterLeft.getWidth(), 0.0f);
    drawLabel(g);
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
    auto bounds = getLocalBounds();
    int meterWidth = 25;

    meterLeft.setBounds(bounds.getX(), bounds.getY(), meterWidth, bounds.getHeight() - 41);
    meterRight.setBounds(meterLeft.getBounds());
    textMeterLeft.setBounds(bounds.getX(), bounds.getY() + 25, meterWidth, 16);
    textMeterRight.setBounds(textMeterLeft.getBounds());
    dbScale.setBounds(bounds.withWidth(meterWidth));
    dbScale.buildBackgroundImage(6, meterLeft.getBounds(), NEGATIVE_INFINITY, MAX_DECIBELS);
    label.setBounds(bounds.withHeight(25));
}

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
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    auto bounds = getLocalBounds();
    juce::Rectangle<int> macroBounds { 0, 0, 75, 335 };
    avgMacroMeter.setBounds(macroBounds.withX(10).withY(10));

    peakMacroMeter.setBounds(macroBounds.withX(getRight() - macroBounds.getWidth() - 10).withY(10));
}
