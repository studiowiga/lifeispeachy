#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Potards : chapeau noir et biseau chrome dans le fond (reflets fixes) ; seul le repere blanc peint tourne.
//==============================================================================
class PeachyKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle, juce::Slider&) override;
};

class PeachyKnob : public juce::Slider
{
public:
    float topRatio = 0.83f;              // rayon du disque noir du dessus / rayon du potard
    bool hitTest (int x, int y) override;
};

//==============================================================================
// Inverseur OCTAVE (levier nickel du kit Studio Wiga) : 3 images (haut = ON, intermediaire ~55 ms, bas = OFF)
//==============================================================================
class LeverSwitch : public juce::Component
{
public:
    explicit LeverSwitch (juce::RangedAudioParameter& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void tick (double nowMs);
    juce::Image strip;
    std::function<void()> onRightClick, onUserChange;
private:
    juce::ParameterAttachment attachment;
    bool on = false;
    int shownFrame = 2, targetFrame = 2;
    double transitionUntil = 0.0;
};

//==============================================================================
// Footswitch (reference Echoflex) : bascule a l'appui, bouton assombri tant qu'on appuie,
// appui maintenu (> 0,45 s) quand la section etait coupee = fonctionnement momentane.
//==============================================================================
class FootSwitchArea : public juce::Component
{
public:
    FootSwitchArea (juce::RangedAudioParameter& p, std::atomic<float>* holdMomentary);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool hitTest (int x, int y) override;
    std::function<void()> onRightClick, onUserChange;
    juce::Image imgUp, imgDown;            // footswitch noir : repos / enfonce
private:
    bool value = false, pressed = false, turnedOnByPress = false;
    double downTime = 0.0;
    std::atomic<float>* hold = nullptr;
    juce::ParameterAttachment attachment;
};

struct RightClickForwarder : public juce::MouseListener
{
    std::function<void()> callback;
    void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu() && callback) callback(); }
};

//==============================================================================
class PeachyAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit PeachyAudioProcessorEditor (PeachyAudioProcessor&);
    ~PeachyAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    // repere de reference (fond 2x = 2480 x 1856) ; tailles de fenetre comme Ring Think (format double)
    static constexpr int baseW = 1240, baseH = 928;
    static constexpr int defaultH = 495;             // 661 x 495 : ouverture par defaut = taille minimale
    static constexpr int refH = 743;                 // « 100 % » = 993 x 743 (comme Ring Think)
    static constexpr int maxH = 1123;

    void setScaledHeight (int h);
    void showStatus();

private:
    void timerCallback() override;
    void useSoftwareRenderer();
    void visibilityChanged() override;
    void askRepaint (juce::Rectangle<int> r);
    void showContextMenu();
    void showPresetMenu();
    void stepPreset (int delta);
    void rebuildCaches (float scale = 0.0f);
    float physicalScale() const;
    juce::Rectangle<int> scaled (juce::Rectangle<float> r) const;
    juce::String buildStatusLine() const;
    juce::String buildPresetLine() const;
    void drawLed (juce::Graphics&, int which);
    void drawStatus (juce::Graphics&);
    void drawPresetBar (juce::Graphics&);

    PeachyAudioProcessor& proc;
    PeachyKnobLookAndFeel lnf;
    RightClickForwarder rightClickFwd;

    juce::Image background, bgScaled, glow[2], glowS[2], ledOn;
    int cacheW = 0, cacheH = 0;
    float cacheScale = 0.0f;
    int resizeSettle = 0;
    void* lastPeer = nullptr;
    double lastPaintMs = 0.0, repaintAskedMs = 0.0, lastScaleCheckMs = 0.0, lastRendererCheckMs = 0.0;
    juce::Typeface::Ptr labelFace;

    PeachyKnob knobs[PeachyAudioProcessor::numKnobs];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> knobAtt[PeachyAudioProcessor::numKnobs];
    LeverSwitch octSwitch;
    FootSwitchArea preFs, fuzzFs;

    float ledLevel[2] { 0.0f, 0.0f }, drawnLed[2] { -1.0f, -1.0f };
    int lastChanged = -1;
    float statusAlpha = 0.0f, drawnStatus = -1.0f;
    double statusUntil = 0.0;
    juce::String lastLine1, lastPreset;
    float lastValues[16];
    int lastPresetCounter = 0, lastMidiCount = 0, midiFlash = 0;
    int hoverZone = 0, drawnHover = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PeachyAudioProcessorEditor)
};
