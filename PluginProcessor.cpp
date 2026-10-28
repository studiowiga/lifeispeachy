#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

//==============================================================================
// Presets d'usine (0 a 10 comme sur la pedale ; footswitchs et octave compris)
//==============================================================================
static const PeachyPreset factoryPresets[] =
{
    //  nom                       master gain  vol   dist  bass  mid   treb  tone  pres  oct pre fuzz
    { "Indigo Crunch",            5.0f, 6.5f, 5.0f, 7.5f, 6.0f, 4.0f, 6.5f, 5.0f, 6.0f, 0, 1, 0 },
    { "Seven String Chug",        5.0f, 7.8f, 5.0f, 7.5f, 5.5f, 3.2f, 6.2f, 5.0f, 6.8f, 0, 1, 0 },
    { "Scooped Rhythm",           5.2f, 8.5f, 5.0f, 7.5f, 7.0f, 2.0f, 7.0f, 5.0f, 6.0f, 0, 1, 0 },
    { "Woolly Fuzz",              5.0f, 6.0f, 5.0f, 7.5f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 0, 0, 1 },
    { "Fuzz Octave Sludge",       5.0f, 6.0f, 5.0f, 9.0f, 5.0f, 5.0f, 5.0f, 3.5f, 5.0f, 1, 0, 1 },
    { "Bright Fuzz Lead",         5.0f, 6.0f, 5.3f, 10.f, 5.0f, 5.0f, 5.0f, 8.0f, 5.0f, 0, 0, 1 },
    { "Fuzz Into Preamp",         4.6f, 5.0f, 4.0f, 7.0f, 5.5f, 4.5f, 6.0f, 4.5f, 5.5f, 0, 1, 1 },
    { "Wall of Sludge",           4.6f, 6.0f, 4.0f, 9.0f, 6.5f, 3.5f, 6.0f, 4.0f, 6.0f, 1, 1, 1 },
    { "Gritty Boost",             6.0f, 2.2f, 5.0f, 7.5f, 5.0f, 6.0f, 6.0f, 5.0f, 5.5f, 0, 1, 0 },
    { "Capture Reference G75 T5", 5.0f, 6.0f, 5.0f, 7.5f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 0, 0, 1 },   // capture d'origine, telle quelle
};

int PeachyAudioProcessor::getNumFactoryPresets() { return (int) std::size (factoryPresets); }
const PeachyPreset& PeachyAudioProcessor::getFactoryPreset (int i) { return factoryPresets[(size_t) jlimit (0, getNumFactoryPresets() - 1, i)]; }

const char* PeachyAudioProcessor::knobId (int i)
{
    static const char* ids[numKnobs] = { "master", "gain", "volume", "distortion", "bass", "mid", "treble", "tone", "presence" };
    return ids[jlimit (0, numKnobs - 1, i)];
}

const char* PeachyAudioProcessor::queuedId (int i)
{
    static const char* ids[numQueued] = { "master", "gain", "volume", "distortion", "bass", "mid", "treble", "tone", "presence",
                                          "octave", "preampOn", "fuzzOn" };
    return ids[jlimit (0, numQueued - 1, i)];
}

//==============================================================================
PeachyAudioProcessor::PeachyAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  AudioChannelSet::stereo(), true)
                        .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PEACHY", createLayout())
{
    for (int i = 0; i < numRaw; ++i) rawPtr[i] = apvts.getRawParameterValue (rawIds[i]);
    for (int i = 0; i < numQueued; ++i) { pendingValue[i] = 0.0f; pendingSet[i] = false; }

    // captures NAM du fuzz (DISTORTION 50 / 65 / 75 / 90 / 100 %, TONE 5), integrees au plugin
    const char* data[5] = { BinaryData::fuzz_G50_nam, BinaryData::fuzz_G65_nam, BinaryData::fuzz_G75_nam,
                            BinaryData::fuzz_G90_nam, BinaryData::fuzz_G100_nam };
    const int sizes[5] = { BinaryData::fuzz_G50_namSize, BinaryData::fuzz_G65_namSize, BinaryData::fuzz_G75_namSize,
                           BinaryData::fuzz_G90_namSize, BinaryData::fuzz_G100_namSize };
    for (int i = 0; i < 5; ++i) engine.loadModel (i, data[i], (size_t) sizes[i]);

    setLatencySamples (32);
    startTimerHz (60);
}

AudioProcessorValueTreeState::ParameterLayout PeachyAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    const auto& d = factoryPresets[0];
    auto knob = [] (const String& id, const String& name, float def)
    {
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 10.0f)) + " %"; }));
    };
    p.push_back (knob ("master",     "Master (preamp level)", d.master));
    p.push_back (knob ("gain",       "Gain (preamp)",         d.gain));
    p.push_back (knob ("volume",     "Volume (fuzz level)",   d.volume));
    p.push_back (knob ("distortion", "Distortion (fuzz)",     d.distortion));
    p.push_back (knob ("bass",       "Bass",                  d.bass));
    p.push_back (knob ("mid",        "Mid",                   d.mid));
    p.push_back (knob ("treble",     "Treble",                d.treble));
    p.push_back (knob ("tone",       "Tone (fuzz)",           d.tone));
    p.push_back (knob ("presence",   "Presence",              d.presence));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "octave", 1 },   "Octave",            d.octave != 0));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "preampOn", 1 }, "Preamp Footswitch", d.preampOn != 0));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "fuzzOn", 1 },   "Fuzz Footswitch",   d.fuzzOn != 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "gate", 1 }, "Noise Gate", gateNames(), 1));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "holdMomentary", 1 }, "Hold = Momentary", true));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "midiFsMode", 1 }, "MIDI Footswitch Mode",
                                                         StringArray { "Latching (127 / 0)", "Momentary (toggle)" }, 0));
    return { p.begin(), p.end() };
}

bool PeachyAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    return in == out;
}

void PeachyAudioProcessor::setParamValue (const String& id, float plain)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        prm->endChangeGesture();
    }
}

// Anti-gel : jamais d'appel a l'hote depuis le thread audio ; le MIDI met la valeur en file,
// ce timer (thread de messages, 60 Hz) la transmet au DAW et a la fenetre.
void PeachyAudioProcessor::setParamFromAudio (int qi, float plain)
{
    pendingValue[qi].store (plain, std::memory_order_relaxed);
    pendingSet[qi].store (true, std::memory_order_release);
}

void PeachyAudioProcessor::timerCallback()
{
    for (int i = 0; i < numQueued; ++i)
        if (pendingSet[i].exchange (false, std::memory_order_acquire))
            if (auto* prm = apvts.getParameter (queuedId (i)))
                prm->setValueNotifyingHost (prm->convertTo0to1 (pendingValue[i].load (std::memory_order_relaxed)));
}

//==============================================================================
void PeachyAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sr, samplesPerBlock);
    for (auto& r : ramps) { r.active = false; r.left = 0; }
    setLatencySamples (engine.latency());
}

//==============================================================================
// MIDI
//==============================================================================
float PeachyAudioProcessor::mapCC (int k, float raw01)
{
    auto& r = ramps[k];
    if (raw01 < r.lo) r.lo = raw01;
    if (raw01 > r.hi) r.hi = raw01;
    float v = raw01;
    if (r.hi - r.lo > 0.4f) v = (raw01 - r.lo) / (r.hi - r.lo);
    constexpr float dz = 0.02f;
    return jlimit (0.0f, 1.0f, (v - dz) / (1.0f - 2.0f * dz));
}

void PeachyAudioProcessor::handleMidi (MidiBuffer& midi)
{
    const bool latching = rawAt (14) < 0.5f;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const juce::int64 when = sampleClock + meta.samplePosition;
        if (m.isController())
        {
            const int cc = m.getControllerNumber(), v = m.getControllerValue();
            bool known = true;
            auto knobFromCC = [&] (int k, float raw01)
            {
                auto& r = ramps[k];
                const float target = mapCC (k, raw01) * 10.0f;
                if (! r.active) r.value = rawAt (k);
                const double interval = (double) (when - r.lastTime);
                r.left = (int) jlimit (0.002 * sr, 0.040 * sr, interval);
                r.lastTime = when;
                r.target = target;
                r.step = (r.target - r.value) / (float) r.left;
                r.active = true;
                setParamFromAudio (k, target);
                currentPreset = -1;
            };
            if (cc >= ccFirstKnob && cc < ccFirstKnob + numKnobs)
            {
                const int k = cc - ccFirstKnob;
                ramps[k].msb = v;
                knobFromCC (k, (float) v / 127.0f);
            }
            else if (cc >= ccFirstKnob + 32 && cc < ccFirstKnob + 32 + numKnobs)
            {
                const int k = cc - 32 - ccFirstKnob;
                if (ramps[k].msb >= 0) knobFromCC (k, (float) (ramps[k].msb * 128 + v) / 16383.0f);
            }
            else if (cc == ccOctave) { setParamFromAudio (9, v >= 64 ? 1.0f : 0.0f); currentPreset = -1; }
            else if (cc == ccPreampFs || cc == ccFuzzFs)
            {
                const int qi = cc == ccPreampFs ? 10 : 11;
                int& prev = cc == ccPreampFs ? prevPreCC : prevFuzzCC;
                const float curV = pendingSet[qi].load() ? pendingValue[qi].load() : rawAt (qi);
                if (latching)                  setParamFromAudio (qi, v >= 64 ? 1.0f : 0.0f);
                else if (v >= 64 && prev < 64) setParamFromAudio (qi, curV > 0.5f ? 0.0f : 1.0f);
                prev = v;
            }
            else known = false;
            if (known) { lastMidiCC = cc; lastMidiValue = v; ++midiCounter; }
        }
        else if (m.isProgramChange())
        {
            const int pc = m.getProgramChangeNumber();
            if (pc < getNumFactoryPresets()) { pendingPreset = pc; triggerAsyncUpdate(); }
            lastMidiProgram = pc; lastMidiCC = -2; ++midiCounter;
        }
    }
    midi.clear();
}

void PeachyAudioProcessor::handleAsyncUpdate()
{
    const int p = pendingPreset.exchange (-1);
    if (p >= 0) applyFactoryPreset (p);
}

//==============================================================================
void PeachyAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    handleMidi (midi);
    if (n == 0) return;
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);

    peachy::Params p;
    float* knobs[numKnobs] = { &p.master, &p.gain, &p.volume, &p.distortion, &p.bass, &p.mid, &p.treble, &p.tone, &p.presence };
    for (int k = 0; k < numKnobs; ++k)
    {
        auto& r = ramps[k];
        if (r.active)
        {
            const int adv = jmin (r.left, n);
            r.value += r.step * (float) adv; r.left -= adv;
            if (r.left <= 0) { r.value = r.target; r.active = false; }
            *knobs[k] = r.value;
        }
        else *knobs[k] = rawAt (k);
    }
    p.octave   = rawAt (9) > 0.5f;
    p.preampOn = rawAt (10) > 0.5f;
    p.fuzzOn   = rawAt (11) > 0.5f;
    p.gate     = jlimit (0, 3, roundToInt (rawAt (12)));

    const int nCh = jmin (2, buffer.getNumChannels());
    float* io[2] = { buffer.getWritePointer (0), nCh > 1 ? buffer.getWritePointer (1) : nullptr };
    engine.process (io, nCh, n, p);

    float pk = 0.0f;
    for (int c = 0; c < nCh; ++c) pk = jmax (pk, buffer.getMagnitude (c, 0, n));
    outLevel = pk;
    sampleClock += n;
}

//==============================================================================
// Presets
//==============================================================================
int PeachyAudioProcessor::getCurrentProgram()
{
    const int c = currentPreset.load();
    return (c >= 0 && c < getNumFactoryPresets()) ? c : 0;
}

void PeachyAudioProcessor::applyFactoryPreset (int index)
{
    const auto& p = getFactoryPreset (index);
    setParamValue ("master", p.master); setParamValue ("gain", p.gain); setParamValue ("volume", p.volume);
    setParamValue ("distortion", p.distortion); setParamValue ("bass", p.bass); setParamValue ("mid", p.mid);
    setParamValue ("treble", p.treble); setParamValue ("tone", p.tone); setParamValue ("presence", p.presence);
    setParamValue ("octave", (float) p.octave); setParamValue ("preampOn", (float) p.preampOn); setParamValue ("fuzzOn", (float) p.fuzzOn);
    currentPreset = jlimit (0, getNumFactoryPresets() - 1, index);
    ++presetCounter;
}

File PeachyAudioProcessor::getUserPresetFile()
{
    return File::getSpecialLocation (File::userApplicationDataDirectory)
             .getChildFile ("Studio Wiga").getChildFile ("Peachy").getChildFile ("UserPresets.xml");
}

static const char* presetIds[] = { "master", "gain", "volume", "distortion", "bass", "mid", "treble", "tone", "presence", "octave", "preampOn", "fuzzOn" };

bool PeachyAudioProcessor::saveUserPreset (int slot)
{
    if (slot < 1 || slot > 9) return false;
    auto file = getUserPresetFile();
    file.getParentDirectory().createDirectory();
    std::unique_ptr<XmlElement> root;
    if (file.existsAsFile()) root = XmlDocument::parse (file);
    if (root == nullptr || ! root->hasTagName ("UserPresets")) root = std::make_unique<XmlElement> ("UserPresets");
    for (auto* e = root->getFirstChildElement(); e != nullptr; e = e->getNextElement())
        if (e->getIntAttribute ("slot") == slot) { root->removeChildElement (e, true); break; }
    auto* e = root->createNewChildElement ("Preset");
    e->setAttribute ("slot", slot);
    for (auto* id : presetIds) e->setAttribute (id, (double) raw (id));
    const bool ok = root->writeTo (file);
    if (ok) { currentPreset = 100 + slot; ++presetCounter; }
    return ok;
}

bool PeachyAudioProcessor::loadUserPreset (int slot)
{
    const auto file = getUserPresetFile();
    if (! file.existsAsFile()) return false;
    auto root = XmlDocument::parse (file);
    if (root == nullptr) return false;
    for (auto* e = root->getFirstChildElement(); e != nullptr; e = e->getNextElement())
        if (e->getIntAttribute ("slot") == slot)
        {
            for (auto* id : presetIds)
                if (e->hasAttribute (id)) setParamValue (id, (float) e->getDoubleAttribute (id));
            currentPreset = 100 + slot; ++presetCounter;
            return true;
        }
    return false;
}

bool PeachyAudioProcessor::hasUserPreset (int slot) const
{
    const auto file = getUserPresetFile();
    if (! file.existsAsFile()) return false;
    if (auto root = XmlDocument::parse (file))
        for (auto* e = root->getFirstChildElement(); e != nullptr; e = e->getNextElement())
            if (e->getIntAttribute ("slot") == slot) return true;
    return false;
}

//==============================================================================
void PeachyAudioProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("currentPreset", currentPreset.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void PeachyAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = ValueTree::fromXml (*xml);
            currentPreset = (int) state.getProperty ("currentPreset", 0);
            apvts.replaceState (state);
            ++presetCounter;
        }
}

AudioProcessorEditor* PeachyAudioProcessor::createEditor() { return new PeachyAudioProcessorEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PeachyAudioProcessor(); }
