/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PFMCPP_Project10AudioProcessorEditor::PFMCPP_Project10AudioProcessorEditor (PFMCPP_Project10AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    addAndMakeVisible(meter);
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
}

void Meter::update(float dbLevel)
{
    peakDb = dbLevel;
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

    //repaint();
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
    }
}

void PFMCPP_Project10AudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    auto bounds = getLocalBounds();
    //meter.setBounds(15, 45, 20, bounds.getHeight() - 30);
    meter.setBounds(15,
                    JUCE_LIVE_CONSTANT(15),
                    20,
                    JUCE_LIVE_CONSTANT(getHeight() - 30));
    dbScale.setBounds(meter.getRight(), 0, meter.getWidth() + 10, getHeight());
    dbScale.buildBackgroundImage(6, meter.getBounds(), NEGATIVE_INFINITY, MAX_DECIBELS);
}
