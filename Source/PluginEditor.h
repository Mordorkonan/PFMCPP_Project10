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
enum Orientation { Left = 0, Right };

struct MacroMeter : juce::Component
{
    MacroMeter(int orientation);
    ~MacroMeter();
    void paintMacro(juce::Graphics& g);
    void resized() override;
    void update(float level);
    bool getOrientation() const;
    juce::Rectangle<int> getAvgMeterBounds() const;

private:
    int orientation;
    TextMeter textMeter;
    Meter peakMeter, avgMeter;
    Averager<float> averager;
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
struct StereoMeter : juce::Component
{
    StereoMeter(juce::String labelName, juce::String labelText);
    void paintStereoMeter(juce::Graphics& g);
    void update(float levelLeft, float levelRight);
    void resized() override;

private:
    MacroMeter leftMacroMeter{ Left }, rightMacroMeter{ Right };
    DbScale dbScale;
    LabelWithBackground label;
};
//==============================================================================
struct Histogram : juce::Component
{
    Histogram(const juce::String& title_);

    void paintHisto(juce::Graphics& g);
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void update(float value);
private:
    ReadAllAfterWriteCircularBuffer<float> buffer{ NEGATIVE_INFINITY };
    juce::Path path;

    void displayPath(juce::Graphics& g, juce::Rectangle<float> bounds);

    static juce::Path buildPath(juce::Path& p,
                                ReadAllAfterWriteCircularBuffer<float>& buffer,
                                juce::Rectangle<float> bounds);
    const juce::String title;
};
//==============================================================================
struct Goniometer : juce::Component
{
    Goniometer(juce::AudioBuffer<float>& buffer);
    void paintGoniometer(juce::Graphics& g);
    void resized() override;

private:
    juce::AudioBuffer<float>& buffer;
    juce::AudioBuffer<float> internalBuffer;
    juce::Path p;
    int w{ 0 }, h{ 0 };
    juce::Point<float> center;
    juce::Array<juce::String> chars { "+S", "L", "M", "R", "-S" };
    juce::Image bkgd;

    void drawBackground();
};
//==============================================================================
class PFMCPP_Project10AudioProcessorEditor  : public juce::AudioProcessorEditor,
                                              public juce::Timer
{
public:
    PFMCPP_Project10AudioProcessorEditor (PFMCPP_Project10AudioProcessor&);
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
    juce::Image reference;
    StereoMeter rmsStereoMeter{ "L RMS R", "L RMS R" },
                peakStereoMeter{ "L PEAK R", "L PEAK R" };

    Histogram rmsHistogram{ "RMS" }, peakHistogram{ "PEAK" };

    Goniometer goniometer { buffer };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PFMCPP_Project10AudioProcessorEditor)
};

