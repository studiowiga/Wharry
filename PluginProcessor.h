#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include "WharryDSP.h"

// ce que l'on peut assigner en MIDI depuis la fenêtre (clic droit -> MIDI learn)
enum class MidiTarget { None = 0, Pedal = 1, LeftFsw = 2, RightFsw = 3 };

struct FactoryPreset
{
    const char* name;
    int  program;      // 0..20 (ordre du sélecteur)
    bool chords, studio;
    float glideMs, tone, mix;
};

class WharryProcessor : public juce::AudioProcessor,
                        private juce::AudioProcessorValueTreeState::Listener,
                        private juce::Timer
{
public:
    WharryProcessor();
    ~WharryProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Wharry"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // presets usine visibles dans le menu de presets du DAW
    static constexpr int kNumFactory = 14;
    static const FactoryPreset& factory (int i);
    int getNumPrograms() override { return kNumFactory; }
    int getCurrentProgram() override { return currentFactory; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override { return factory (index).name; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- utilisé par la fenêtre ----
    void selectWhammyProgram (int program0to20);     // programme d'origine de la pédale (sélecteur)
    void loadFactory (int index);
    int  getCurrentFactory() const { return currentFactory; }
    int  getActiveFactory() const { return activeFactory.load(); }   // preset usine chargé et non modifié depuis (-1 sinon)
    void setParamValue (const juce::String& id, float plainValue);
    void togglePedalSwitch (const juce::String& id);

    // position lissée de la pédale (0 = haute, 1 = basse) : celle du moteur audio, ou la consigne directe si l'audio
    // ne tourne pas (transport arrêté, plugin suspendu) -> le visuel suit toujours la pédale MIDI et les automations
    float getPedalShown() const
    {
        const auto now = juce::Time::getMillisecondCounter();
        return (now - lastBlockMs.load() > 250u) ? pedalLive.load() : pedalShown.load();
    }
    float getSemitones() const    { return semis.load(); }
    float getDetectedHz() const   { return detHz.load(); }
    int   getLatency() const      { return reportedLatency.load(); }

    void startMidiLearn (MidiTarget t) { learnTarget = (int) t; }
    void cancelMidiLearn() { learnTarget = 0; }
    MidiTarget getLearnTarget() const { return (MidiTarget) learnTarget.load(); }
    void clearMidi (MidiTarget t);
    juce::String getMidiDescription (MidiTarget t) const;

    // calibration de la pédale d'expression (pédales qui n'atteignent pas 0 / 127)
    void startCalibration();
    void finishCalibration();          // à appeler sur le thread de la fenêtre
    void resetCalibration();
    bool isCalibrating() const { return calibrating.load(); }
    float calibrationSpan() const { return calMax.load() - calMin.load(); }
    juce::uint32 msSincePedalMove() const { return juce::Time::getMillisecondCounter() - lastMoveMs.load(); }
    juce::String calibrationDescription() const;

    std::atomic<int> uiScaleIdx { -1 };    // -1 = taille par défaut (67 %), 1 = 75 %, 2 = 100 %

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String&, float) override;
    void timerCallback() override;          // anti-gel v2 : vide la file des changements demandés par l'audio (60 Hz)
    void handleMidiMessage (const juce::MidiMessage&);
    float mapPedal (float raw01, bool fromMidi) const;
    void  notePedalRaw (float raw01, bool fromMidi);
    wharry::Settings currentSettings() const;

    wharry::Engine engine;
    double sr = 44100.0;
    juce::AudioBuffer<float> mono;

    std::atomic<float> *pProgram, *pPedal, *pChords, *pStudio, *pActive, *pSmooth, *pHeel, *pToe, *pInvert, *pTone, *pMix, *pPC;

    std::atomic<float> pedalLive { 0.0f }, pedalShown { 0.0f }, semis { 0.0f }, detHz { 0.0f };
    std::atomic<int> reportedLatency { 0 }, latencyToSet { -1 };
    std::atomic<juce::uint32> lastBlockMs { 0 };
    std::atomic<int> learnTarget { 0 }, ccPedal { 11 }, ccLeft { -1 }, noteLeft { -1 }, ccRight { -1 }, noteRight { -1 };
    std::atomic<bool> leftHeld { false }, rightHeld { false };
    std::atomic<float> autoMin { 1.0f }, autoMax { 0.0f };          // plage réellement envoyée par la pédale MIDI (apprise automatiquement)
    std::atomic<bool> calibrating { false };
    std::atomic<float> calMin { 1.0f }, calMax { 0.0f }, lastRaw { -1.0f };
    std::atomic<juce::uint32> lastMoveMs { 0 };

    // changements demandés depuis le thread audio (MIDI) et appliqués sur le thread de la fenêtre
    // anti-gel v2 : file de valeurs (aucun appel à l'hôte depuis l'audio), vidée par le timer du processeur
    static constexpr int numQueued = 3;
    static const char* queuedId (int i);              // "program", "chords", "active"
    void setParamFromAudio (int i, float plain);
    float queuedOrCurrent (int i) const;
    std::atomic<float> pendingValue[numQueued];
    std::atomic<bool>  pendingSet[numQueued];
    int currentFactory = 0;
    std::atomic<int> activeFactory { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WharryProcessor)
};
