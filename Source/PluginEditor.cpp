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
    
    // Testing placement
    //auto bounds = getLocalBounds().toFloat().reduced(10);
    //g.setColour(juce::Colours::darkgrey);
    //g.drawRect(bounds, 5.0f);
    //float remappedPeakDb = juce::jmap<float>(10.0f, NEGATIVE_INFINITY, MAX_DECIBELS, bounds.getHeight(), 0.0f);
    //g.setColour(juce::Colours::white);
    //g.fillRect(15.0f, remappedPeakDb, bounds.reduced(5).getWidth(), bounds.reduced(5).getHeight()); // 15 = reduced(10) + 5.0f as line thickness
}

void Meter::paint(juce::Graphics& g)
{
    using namespace juce;
    auto bounds = getLocalBounds().toFloat();
    float outline = 2.0f;
    g.setColour(Colours::darkgrey);
    g.drawRect(bounds, outline);
    float remappedPeakDb = jmap<float>(peakDb, NEGATIVE_INFINITY, MAX_DECIBELS, bounds.getHeight() - outline, bounds.getY() + outline);
    g.setColour(Colours::white);
    Rectangle<float> rect { bounds.getX() + 2, remappedPeakDb, bounds.reduced(2).getWidth(), bounds.getHeight() };
    g.fillRect(rect);
}

void Meter::update(float dbLevel)
{
    peakDb = dbLevel;
    repaint();
}

void PFMCPP_Project10AudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    auto bounds = getLocalBounds();
    meter.setBounds(15, 15, 20, bounds.getHeight() - 30);
}
