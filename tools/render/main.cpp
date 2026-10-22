#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace juce;
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const String out = argc > 1 ? argv[1] : ".";
    WharryProcessor proc;
    proc.prepareToPlay (48000.0, 256);
    std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
    std::printf ("taille par défaut : %d x %d\n", ed->getWidth(), ed->getHeight());
    auto shot = [&] (const String& name, float pedal, int prog, bool chords, bool active)
    {
        proc.setParamValue ("program", (float) prog); proc.setParamValue ("chords", chords ? 1.f : 0.f); proc.setParamValue ("active", active ? 1.f : 0.f);
        proc.setParamValue ("pedal", pedal);
        // fait tourner le moteur pour que la position lissée suive
        AudioBuffer<float> b (2, 256); MidiBuffer m;
        for (int i = 0; i < 200; ++i) { b.clear(); proc.processBlock (b, m); }
        for (int i = 0; i < 3; ++i) MessageManager::getInstance()->runDispatchLoopUntil (60);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        File f (out + "/" + name + ".png"); f.deleteFile();
        FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
        std::printf ("%s (%d x %d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    };
    shot ("wharry_haute", 0.0f, 1, false, true);
    shot ("wharry_milieu", 0.5f, 1, false, true);
    shot ("wharry_basse", 1.0f, 15, true, true);
    static_cast<WharryEditor*> (ed.get())->setScaleIndex (2);
    shot ("wharry_100pc_basse", 1.0f, 9, false, false);
    ed.reset();
    return 0;
}
