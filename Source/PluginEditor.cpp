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
    g.setColour(juce::Colours::black);
    g.setOpacity(0.35f);
    g.fillRect(getLocalBounds().toFloat());
}

void DbScale::buildBackgroundImage(int dbDivision, juce::Rectangle<int> meterBounds, int minDb, int maxDb)
{
    if (minDb > maxDb)
        std::swap(minDb, maxDb);

    auto bounds = getLocalBounds();
    if (bounds.isEmpty())
        return;

    juce::Graphics gbkgd(bkgd.rescaled(meterBounds.getWidth(), meterBounds.getHeight()));
    gbkgd.addTransform(juce::AffineTransform::scale(juce::Desktop::getInstance().getGlobalScaleFactor()));

    for (auto& tick : getTicks(dbDivision, meterBounds, minDb, maxDb))
    {
        gbkgd.setColour(juce::Colours::white);
        gbkgd.drawFittedText(juce::String(tick.db), bounds, juce::Justification::centred, (maxDb - minDb) / dbDivision);
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
        tick.db = juce::jmap<float>(db, minDb, maxDb, meterBounds.getBottom(), meterBounds.getY());
        tickVector.push_back(tick);
    }

    return tickVector;
}

void PFMCPP_Project10AudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    auto bounds = getLocalBounds();
    meter.setBounds(15, 15, 20, bounds.getHeight() - 30);
    dbScale.setBounds(meter.getBounds().removeFromLeft(meter.getWidth() + 2));
    dbScale.buildBackgroundImage(6, meter.getBounds(), NEGATIVE_INFINITY, MAX_DECIBELS);
}
