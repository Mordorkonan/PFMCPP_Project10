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
    repaint();
}

void TextMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    juce::Colour textColor { juce::Colours::white };
    auto valueToDisplay = NEGATIVE_INFINITY;
    valueHolder.setThreshold(JUCE_LIVE_CONSTANT(0));
    auto now = ValueHolder::getNow();

    if  (valueHolder.getIsOverThreshold() || 
        (now - valueHolder.getPeakTime() < valueHolder.getHoldTime()) &&
            valueHolder.getPeakTime() > valueHolder.getHoldTime())     // for plugin launch
    {   
        g.setColour(juce::Colours::red);
        g.fillAll();
        textColor = juce::Colours::black;
        valueToDisplay = valueHolder.getHeldValue();
    }
    else
    {
        g.setColour(juce::Colours::black);
        g.fillAll();
        textColor = juce::Colours::white;
        valueToDisplay = valueHolder.getCurrentValue();
    }

    g.setColour(textColor);
    g.setFont(12);

    g.drawFittedText((valueToDisplay > NEGATIVE_INFINITY) ? juce::String(valueToDisplay, 1) : juce::String("-inf"), 
                     getLocalBounds(), 
                     juce::Justification::centred, 
                     1);
}
//==============================================================================
PFMCPP_Project10AudioProcessorEditor::PFMCPP_Project10AudioProcessorEditor (PFMCPP_Project10AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    addAndMakeVisible(meter);
    addAndMakeVisible(textMeter);
    addAndMakeVisible(dbScale);

    startTimerHz(60);
    setSize (400, 300);
}

PFMCPP_Project10AudioProcessorEditor::~PFMCPP_Project10AudioProcessorEditor()
{
}

//==============================================================================
void PFMCPP_Project10AudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}
//==============================================================================
void Meter::paint(juce::Graphics& g)
{
    using namespace juce;
    auto bounds = getLocalBounds().toFloat();

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
    repaint();
}

void DbScale::paint(juce::Graphics& g)
{
    g.drawImage(bkgd, getLocalBounds().toFloat());
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
    for (auto& tick : getTicks(dbDivision, meterBounds, minDb, maxDb))
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

void PFMCPP_Project10AudioProcessorEditor::timerCallback()
{
    if (audioProcessor.audioBufferFifo.pull(buffer))
    {
        while (audioProcessor.audioBufferFifo.pull(buffer))
        {

        }
        auto magDb = juce::Decibels::gainToDecibels(buffer.getMagnitude(0, 0, buffer.getNumSamples()), NEGATIVE_INFINITY);
        meter.update(magDb);
        textMeter.update(magDb);
    }
}

void PFMCPP_Project10AudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    auto bounds = getLocalBounds();
    //meter.setBounds(15, 45, 20, bounds.getHeight() - 30);
    meter.setBounds(15,
                    JUCE_LIVE_CONSTANT(25),
                    25,
                    JUCE_LIVE_CONSTANT(getHeight() - 50));
    textMeter.setBounds(meter.getX(), meter.getY() - 20, meter.getWidth(), 16);
    dbScale.setBounds(meter.getRight(), 0, meter.getWidth() + 10, getHeight());
    dbScale.buildBackgroundImage(6, meter.getBounds(), NEGATIVE_INFINITY, MAX_DECIBELS);
}
