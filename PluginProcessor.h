#pragma once
#include <cstring>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include "PeachyDSP.h"

//==============================================================================
// PEACHY (Studio Wiga) - preampli + fuzz au format pedale double. Voir README.md et PeachyDSP.h.
//==============================================================================

struct PeachyPreset
{
    const char* name;
    float master, gain, volume, distortion, bass, mid, treble, tone, presence;   // 0..10 comme sur la pedale
    int   octave, preampOn, fuzzOn;
};

class PeachyAudioProcessor : public juce::AudioProcessor,
                             private juce::AsyncUpdater,
                             private juce::Timer
{
public:
    PeachyAudioProcessor();
    ~PeachyAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Peachy"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    static int getNumFactoryPresets();
    static const PeachyPreset& getFactoryPreset (int index);
    int getNumPrograms() override { return getNumFactoryPresets(); }
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override { applyFactoryPreset (index); }
    const juce::String getProgramName (int index) override { return getFactoryPreset (index).name; }
    void changeProgramName (int, const juce::String&) override {}
    void applyFactoryPreset (int index);

    bool saveUserPreset (int slot);
    bool loadUserPreset (int slot);
    bool hasUserPreset (int slot) const;
    static juce::File getUserPresetFile();

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setParamValue (const juce::String& id, float plain);

    // anti-gel : pointeurs des parametres mis en cache (aucune recherche par chaine dans processBlock)
    static constexpr int numRaw = 15;
    static constexpr const char* rawIds[numRaw] = { "master", "gain", "volume", "distortion", "bass", "mid", "treble", "tone",
                                                     "presence", "octave", "preampOn", "fuzzOn", "gate", "holdMomentary", "midiFsMode" };
    std::atomic<float>* rawPtr[numRaw] {};
    float raw (const char* id) const
    {
        for (int i = 0; i < numRaw; ++i)
            if (std::strcmp (id, rawIds[i]) == 0) return rawPtr[i]->load (std::memory_order_relaxed);
        return apvts.getRawParameterValue (id)->load();
    }
    float rawAt (int i) const { return rawPtr[i]->load (std::memory_order_relaxed); }

    static juce::StringArray gateNames() { return { "Off", "Light", "Medium", "Strong" }; }

    // informations pour la fenetre
    std::atomic<int>   currentPreset { 0 }, presetCounter { 0 };
    std::atomic<int>   lastMidiCC { -1 }, lastMidiValue { 0 }, lastMidiProgram { -1 }, midiCounter { 0 };
    std::atomic<float> outLevel { 0.0f };
    bool modelsLoaded() const { return engine.modelsReady(); }

    // 8 potards (ordre du Layout), puis PRESENCE
    static constexpr int numKnobs = 9;
    static const char* knobId (int i);
    enum MidiMap
    {
        ccFirstKnob = 20,      // CC 20 a 28 : master, gain, volume, distortion, bass, mid, treble, tone, presence
        ccOctave = 29,         // >= 64 : octave
        ccPreampFs = 30,       // footswitch gauche (PREAMP)
        ccFuzzFs = 31          // footswitch droit (FUZZ)
    };

private:
    void handleMidi (juce::MidiBuffer& midi);
    void setParamFromAudio (int queueIndex, float plain);
    void timerCallback() override;
    static constexpr int numQueued = 12;            // 9 potards + octave + preampOn + fuzzOn
    static const char* queuedId (int i);
    std::atomic<float> pendingValue[numQueued];
    std::atomic<bool>  pendingSet[numQueued];
    void handleAsyncUpdate() override;
    float mapCC (int knob, float raw01);

    peachy::Engine engine;
    double sr = 48000.0;

    struct ExprRamp
    {
        bool  active = false;
        float value = 0.0f, target = 0.0f, step = 0.0f;
        int   left = 0;
        juce::int64 lastTime = 0;
        int   msb = -1;
        float lo = 1.0f, hi = 0.0f;
    } ramps[numKnobs];
    juce::int64 sampleClock = 0;
    int prevPreCC = 0, prevFuzzCC = 0;
    std::atomic<int> pendingPreset { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PeachyAudioProcessor)
};
