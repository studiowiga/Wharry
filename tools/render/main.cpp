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
    shot ("wharry_quart", 0.25f, 1, false, true);
    shot ("wharry_basse", 1.0f, 15, true, true);
    shot ("wharry_bypass", 0.3f, 3, false, false);
    static_cast<WharryEditor*> (ed.get())->setScaleIndex (2);
    shot ("wharry_100pc_haute", 0.0f, 1, false, true);
    shot ("wharry_100pc_milieu", 0.55f, 15, true, true);
    shot ("wharry_100pc_basse", 1.0f, 1, false, true);
    shot ("wharry_100pc_bypass", 1.0f, 9, false, false);
    proc.loadFactory (4);
    {
        proc.setParamValue ("pedal", 0.35f);
        AudioBuffer<float> b (2, 256); MidiBuffer m;
        for (int i = 0; i < 200; ++i) { b.clear(); proc.processBlock (b, m); }
        for (int i = 0; i < 3; ++i) MessageManager::getInstance()->runDispatchLoopUntil (60);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        File f (out + "/wharry_100pc_preset.png"); f.deleteFile();
        FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
    }
    // mesure : temps de dessin de la fenêtre quand le plateau balaie toute sa course (1er passage = calcul des positions)
    MessageManager::getInstance()->runDispatchLoopUntil (3000);   // laisse le timer précalculer au repos
    for (int pass = 0; pass < 2; ++pass)
    {
        double worst = 0, total = 0;
        for (int i = 0; i <= 40; ++i)
        {
            proc.setParamValue ("pedal", (float) i / 40.0f);
            AudioBuffer<float> b (2, 256); MidiBuffer m;
            for (int k = 0; k < 60; ++k) { b.clear(); proc.processBlock (b, m); }
            MessageManager::getInstance()->runDispatchLoopUntil (40);
            const double t0 = Time::getMillisecondCounterHiRes();
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            const double dt = Time::getMillisecondCounterHiRes() - t0;
            worst = jmax (worst, dt); total += dt;
        }
        std::printf ("mesure passage %d : dessin complet moyen %.1f ms, pire %.1f ms\n", pass + 1, total / 41.0, worst);
    }
    ed.reset();
    return 0;
}
