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
enum Orientation { Left, Right };
//==============================================================================
struct NewLNF : juce::LookAndFeel_V4
{
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle, juce::Slider&) override;
};
//==============================================================================
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
    float getThreshold() const;

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
    void paint(juce::Graphics& g) override;
    ///expects a decibel value
    void update(float valueDb);
    void setThreshold(float threshold);
private:
    float cachedValueDb;
    ValueHolder valueHolder;
};
//==============================================================================
struct Meter : juce::Component
{
    void paint(juce::Graphics& g) override;
    void update(float dbLevel);
    void setThreshold(float threshold);

private:
    float peakDb;
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
    void paint(juce::Graphics& g) override;
    void buildBackgroundImage(int dbDivision, juce::Rectangle<int> meterBounds, int minDb, int maxDb);
    static std::vector<Tick> getTicks(int dbDivision, juce::Rectangle<int> meterBounds, int minDb, int maxDb);

private:
    juce::Image bkgd;
};
//==============================================================================
struct MacroMeter : juce::Component
{
    MacroMeter(int orientation);
    ~MacroMeter();
    void resized() override;
    void update(float level);
    bool getOrientation() const;
    juce::Rectangle<int> getAvgMeterBounds() const;
    int getTextMeterHeight() const;
    void setThreshold(float threshold);

private:
    int orientation;
    TextMeter textMeter;
    Meter peakMeter, avgMeter;
    Averager<float> averager;
};
//==============================================================================
struct StereoMeter : juce::Component
{
    StereoMeter(juce::String labelName, juce::String labelText);
    ~StereoMeter();
    void update(float levelLeft, float levelRight);
    void resized() override;
    void setThreshold(float threshold);

    juce::Slider thresholdSlider{ juce::Slider::SliderStyle::LinearVertical,
                                  juce::Slider::TextEntryBoxPosition::NoTextBox };

private:
    MacroMeter leftMacroMeter{ Left }, rightMacroMeter{ Right };
    DbScale dbScale;
    juce::Label label;
    NewLNF newLNF;
};
//==============================================================================
struct Histogram : juce::Component
{
    Histogram(const juce::String& title_);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void update(float value);
    void setThreshold(float newThreshold);
    bool isOverThreshold() const;

private:
    ReadAllAfterWriteCircularBuffer<float> buffer{ NEGATIVE_INFINITY };
    juce::Path path;

    void displayPath(juce::Graphics& g, juce::Rectangle<float> bounds);

    static juce::Path buildPath(juce::Path& p,
                                ReadAllAfterWriteCircularBuffer<float>& buffer,
                                juce::Rectangle<float> bounds);
    const juce::String title;
    float threshold{ 0.0f };
};
//==============================================================================
struct Goniometer : juce::Component
{
    Goniometer(juce::AudioBuffer<float>& buffer);
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioBuffer<float>& buffer;
    juce::AudioBuffer<float> internalBuffer;
    juce::Path p;
    juce::Point<float> center;
    juce::Array<juce::String> chars { "+S", "L", "M", "R", "-S" };
    juce::Image bkgd;
    int radius{ 0 };
    float conversionCoefficient{ juce::Decibels::decibelsToGain(-3.0f) };

    void drawBackground();
};
//==============================================================================
struct CorrelationMeter : juce::Component
{
    CorrelationMeter(juce::AudioBuffer<float>& buf, double sampleRate);
    void update();
    void paint(juce::Graphics& g) override;
    void fillMeter(juce::Graphics& g, juce::Rectangle<float>& bounds, float value, float centerX);

private:
    juce::AudioBuffer<float>& buffer;
    using FilterType = juce::dsp::FIR::Filter<float>;
    std::array<FilterType, 3> filters;
    juce::Array<juce::String> chars{ "-1", "+1" };

    Averager<float> slowAverager{ 1024 * 3, 0 }, peakAverager{ 512, 0 };
};
//==============================================================================
struct StereoImageMeter : juce::Component
{
    StereoImageMeter(juce::AudioBuffer<float>& buffer_, double sampleRate);
    void resized() override;
    void update();

private:
    Goniometer goniometer;
    CorrelationMeter correlationMeter;
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
    juce::Image reference;
    StereoMeter rmsStereoMeter{ "RMS", "L RMS R" },
                peakStereoMeter{ "PEAK", "L PEAK R" };

    Histogram rmsHistogram{ "RMS" }, peakHistogram{ "PEAK" };

    StereoImageMeter stereoImageMeter{ buffer, audioProcessor.getSampleRate() };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PFMCPP_Project10AudioProcessorEditor)
};

