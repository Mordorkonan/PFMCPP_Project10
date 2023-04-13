/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

#define NEGATIVE_INFINITY -66.0f
#define MAX_DECIBELS 12.0f
//==============================================================================
/**
*/
struct Meter : juce::Component
{
    void paint(juce::Graphics&) override;
    void update(float dbLevel);
private:
    float peakDb { NEGATIVE_INFINITY };
};

struct Tick
{
    float db{ 0.f };
    int y{ 0 };
};

struct DbScale : juce::Component
{
    ~DbScale() override = default;
    void paint(juce::Graphics& g) override;
    //void resized() override;
    void buildBackgroundImage(int dbDivision, juce::Rectangle<int> meterBounds, int minDb, int maxDb);
    static std::vector<Tick> getTicks(int dbDivision, juce::Rectangle<int> meterBounds, int minDb, int maxDb);

private:
    juce::Image bkgd;
};
//==============================================================================
struct ValueHolder : juce::Timer
{
    ValueHolder();
    ~ValueHolder();
    void timerCallback() override;
    void setThreshold(float th);
    void updateHeldValue(float v);
    void setHoldTime(int ms);
    float getCurrentValue() const;
    float getHeldValue() const;
    bool getIsOverThreshold() const;
    void updateIsOverThreshold();
private:
    float threshold = 0;
    float currentValue = NEGATIVE_INFINITY;
    float heldValue = NEGATIVE_INFINITY;
    juce::int64 timeOfPeak;
    int durationToHoldForMs{ 500 };
    bool isOverThreshold{ false };
};
//==============================================================================
struct DecayingValueHolder : juce::Timer
{
    DecayingValueHolder();
    ~DecayingValueHolder();

    void updateHeldValue(float input);

    float getCurrentValue() const;
    bool isOverThreshold() const;

    void setHoldTime(int ms);
    void setDecayRate(float dbPerSec);

    void timerCallback() override;
private:
    float currentValue{ NEGATIVE_INFINITY };
    int frameRate{ 60 };
    juce::int64 peakTime = getNow();
    float threshold = 0.f;
    juce::int64 holdTime = 2000; //2 seconds
    float decayRatePerFrame{ 0 };
    float decayRateMultiplier{ 1 };

    static juce::int64 getNow();
    void resetDecayRateMultiplier();
};
//==============================================================================
struct TextMeter : juce::Component
{
    TextMeter();
    void paint(juce::Graphics& g) override;
    ///expects a decibel value
    void update(float valueDb);
private:
    float cachedValueDb;
    ValueHolder valueHolder;
};
//==============================================================================
class PFMCPP_Project10AudioProcessorEditor  : public juce::AudioProcessorEditor,
                                              public juce::Timer
{
public:
    PFMCPP_Project10AudioProcessorEditor (PFMCPP_Project10AudioProcessor&);
    ~PFMCPP_Project10AudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PFMCPP_Project10AudioProcessor& audioProcessor;

    juce::AudioBuffer<float> buffer;
    Meter meter;
    TextMeter textMeter;
    DbScale dbScale;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PFMCPP_Project10AudioProcessorEditor)
};
