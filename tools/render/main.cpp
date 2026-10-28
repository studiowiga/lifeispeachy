// Captures PNG de la fenetre reelle de Peachy (meme code que le plugin), pour l'apercu et la notice.
#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace juce;
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const String out = argc > 1 ? argv[1] : ".";
    PeachyAudioProcessor proc;
    std::printf ("modeles NAM charges : %s\n", proc.modelsLoaded() ? "oui" : "NON");
    proc.prepareToPlay (48000.0, 256);
    std::printf ("latence a 48 kHz : %d echantillons\n", proc.getLatencySamples());
    // test audio des presets : niveau de sortie de chaque preset avec un riff synthetique (sinus graves + attaques)
    for (double sr : { 44100.0, 48000.0 })
    {
        proc.prepareToPlay (sr, 512);
        for (int pi = 0; pi < PeachyAudioProcessor::getNumFactoryPresets(); ++pi)
        {
            proc.applyFactoryPreset (pi);
            for (int i = 0; i < 5; ++i) MessageManager::getInstance()->runDispatchLoopUntil (20);
            double sum = 0; long cnt = 0; double ph = 0;
            for (int blk = 0; blk < 200; ++blk)
            {
                AudioBuffer<float> b (2, 512); MidiBuffer m;
                for (int i = 0; i < 512; ++i)
                {
                    const float env = (float) std::exp (-((blk * 512 + i) % 12000) / 4000.0);
                    const float x = 0.25f * env * (float) std::sin (ph); ph += 2 * MathConstants<double>::pi * 82.4 / sr;
                    b.setSample (0, i, x); b.setSample (1, i, x);
                }
                proc.processBlock (b, m);
                if (blk > 40) for (int i = 0; i < 512; ++i) { sum += b.getSample (0, i) * b.getSample (0, i); ++cnt; }
            }
            std::printf ("sr %.0f preset %2d %-26s rms %6.1f dB\n", sr, pi + 1, PeachyAudioProcessor::getFactoryPreset (pi).name, 10 * std::log10 (sum / cnt + 1e-20));
        }
    }
    proc.prepareToPlay (48000.0, 256);
    std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
    auto* ped = static_cast<PeachyAudioProcessorEditor*> (ed.get());
    std::printf ("taille par defaut : %d x %d\n", ed->getWidth(), ed->getHeight());
    auto pump = [] (int n) { for (int i = 0; i < n; ++i) MessageManager::getInstance()->runDispatchLoopUntil (40); };
    auto shot = [&] (const String& name, int preset, bool pre, bool fuzz, bool status, int height)
    {
        ped->setScaledHeight (height);
        proc.applyFactoryPreset (preset);
        proc.setParamValue ("preampOn", pre ? 1.f : 0.f);
        proc.setParamValue ("fuzzOn", fuzz ? 1.f : 0.f);
        AudioBuffer<float> b (2, 256); MidiBuffer m;
        for (int i = 0; i < 10; ++i) { b.clear(); proc.processBlock (b, m); }
        pump (30);
        if (status) proc.setParamValue ("distortion", 8.2f); else pump (100);
        pump (12);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        File f (out + "/" + name + ".png"); f.deleteFile();
        FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
        std::printf ("%s (%d x %d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    };
    shot ("peachy_default", 0, true,  false, false, PeachyAudioProcessorEditor::defaultH);
    shot ("peachy_both",    7, true,  true,  false, PeachyAudioProcessorEditor::refH);
    shot ("peachy_status",  4, false, true,  true,  PeachyAudioProcessorEditor::refH);
    shot ("peachy_off",     0, false, false, false, PeachyAudioProcessorEditor::refH);
    {
        auto sendCC = [&] (int v)
        {
            AudioBuffer<float> b (2, 256); MidiBuffer m; m.addEvent (MidiMessage::controllerEvent (1, 23, v), 0);
            proc.processBlock (b, m);
            for (int k = 0; k < 20; ++k) { MidiBuffer e; b.clear(); proc.processBlock (b, e); }
            pump (3);
            return proc.raw ("distortion");
        };
        const float a = sendCC (2), c = sendCC (116), mid = sendCC (59);
        std::printf ("pedale 2..116 -> distortion %.2f / %.2f, mi-course %.2f\n", a, c, mid);
    }
    ed.reset();
    return 0;
}
