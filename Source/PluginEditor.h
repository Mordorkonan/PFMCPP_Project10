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
struct ValueHolderBase : juce::Timer
{
    ValueHolderBase();
    virtual ~ValueHolderBase();

    virtual void updateHeldValue(float v) = 0;
    virtual void timerCallback() override;
    virtual void timerCallbackImpl() = 0;
    void setThreshold(float th);
    void setHoldTime(int ms);
    float getCurrentValue() const;
    bool getIsOverThreshold() const;

    juce::int64 getPeakTime() const;
    juce::int64 getHoldTime() const;
    static juce::int64 getNow();

    static int frameRate;

protected:
    float threshold = 0.0f;
    float currentValue = NEGATIVE_INFINITY;
    juce::int64 peakTime = 0;   // 0 to prevent red textmeter at launch
    juce::int64 holdTime = 2000;
};
//==============================================================================
struct ValueHolder : ValueHolderBase
{
    ValueHolder();
    ~ValueHolder();
    void timerCallbackImpl() override;
    void updateHeldValue(float v) override;

    float getHeldValue() const;

private:
    float heldValue = NEGATIVE_INFINITY;
};
//==============================================================================
struct DecayingValueHolder : ValueHolderBase
{
    DecayingValueHolder();
    ~DecayingValueHolder();

    void timerCallbackImpl() override;
    void updateHeldValue(float v) override;

    void setDecayRate(float dbPerSec);

private:
    float decayRatePerFrame{ 0 };
    float decayRateMultiplier{ 1 };

    void resetDecayRateMultiplier();
};
//==============================================================================
struct TextMeter : juce::Component
{
    TextMeter();
    void paintTextMeter(juce::Graphics& g);
    ///expects a decibel value
    void update(float valueDb);
private:
    float cachedValueDb;
    ValueHolder valueHolder;
};
//==============================================================================
struct Meter : juce::Component
{
    void paintMeter(juce::Graphics& g);
    void update(float dbLevel);
private:
    float peakDb { NEGATIVE_INFINITY };
    DecayingValueHolder decayingValueHolder;
};
//==============================================================================
struct Tick
{
    float db{ 0.f };
    int y{ 0 };
};
//==============================================================================
struct DbScale : juce::Component
{
    ~DbScale() override = default;
    void paintScale(juce::Graphics& g);
    void buildBackgroundImage(int dbDivision, juce::Rectangle<int> meterBounds, int minDb, int maxDb);
    static std::vector<Tick> getTicks(int dbDivision, juce::Rectangle<int> meterBounds, int minDb, int maxDb);

private:
    juce::Image bkgd;
};
//==============================================================================
struct LabelWithBackground : juce::Component
{
    LabelWithBackground(juce::String labelName, juce::String labelText);
    void paintLabel(juce::Graphics& g);
private:
    juce::Label label;
};
//==============================================================================
struct MacroMeter : juce::Component
{
    explicit MacroMeter(bool useAverage = false);
    ~MacroMeter();
    void paintMacro(juce::Graphics& g);
    void resized() override;
    void update(float levelLeft, float levelRight);
    juce::Rectangle<int> getLeftMeterBounds() const;
    bool isAverageMeasure() const;

private:
    bool averageMeasure;
    TextMeter textMeterLeft, textMeterRight;
    Meter meterLeft, meterRight;
    Averager<float> averagerLeft, averagerRight;
};
//==============================================================================
struct StereoMeter : juce::Component
{
    explicit StereoMeter(juce::String labelName, juce::String labelText, bool useAverage = false);
    void paintStereoMeter(juce::Graphics& g);
    void update(float levelLeft, float levelRight);
    void resized() override;

private:
    MacroMeter macroMeter;
    DbScale dbScale;
    LabelWithBackground label;
};
//==============================================================================
class PFMCPP_Project10AudioProcessorEditor : public juce::AudioProcessorEditor,
    public juce::Timer
{
public:
    PFMCPP_Project10AudioProcessorEditor(PFMCPP_Project10AudioProcessor&);
    ~PFMCPP_Project10AudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PFMCPP_Project10AudioProcessor& audioProcessor;

    juce::AudioBuffer<float> buffer;
    juce::Image referenceImage;

    StereoMeter peakStereoMeter { "L PEAK R", "L PEAK R", false },
                avgStereoMeter { "L RMS R", "L RMS R", true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PFMCPP_Project10AudioProcessorEditor)
};
