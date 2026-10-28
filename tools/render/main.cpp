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
