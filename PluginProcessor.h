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

    void startMidiLearn (MidiTarget t) { learnTarget = learnIndexOf (t); }
    void cancelMidiLearn() { learnTarget = -1; }
    MidiTarget getLearnTarget() const
    {
        const int lt = learnTarget.load();
        return lt == lPedal ? MidiTarget::Pedal : lt == lChords ? MidiTarget::LeftFsw : lt == lFs ? MidiTarget::RightFsw : MidiTarget::None;
    }
    void clearMidi (MidiTarget t);
    juce::String getMidiDescription (MidiTarget t) const;

    // v1.9 MIDI Learn : toutes les commandes de la facade (clic droit sur la commande)
    enum LearnType { learnContinuous, learnToggle, learnSelector, learnFootswitch };
    struct LearnDef { const char* id; const char* name; int defaultCC; int type; int positions; };
    enum { lPedal = 0, lProgram = 1, lGlide = 2, lTone = 3, lMix = 4, lChords = 5, lFs = 6 };
    static constexpr int numLearn = 7;
    static const LearnDef& learnDef (int i);
    static int learnIndexOf (MidiTarget t) { return t == MidiTarget::Pedal ? lPedal : t == MidiTarget::LeftFsw ? lChords : t == MidiTarget::RightFsw ? lFs : -1; }
    std::atomic<int> ccMap[numLearn];            // numero de CC de chaque commande, -1 = non assigne
    std::atomic<int> learnTarget { -1 };         // commande en attente d'un CC (-1 = pas de learn)
    std::atomic<int> lastLearned { -1 };         // derniere commande apprise (lue et remise a -1 par la fenetre)
    void resetMidiMap();
    void clearLearn (int i);
    juce::String learnAssignText (int i) const;  // "CC n", "Note n" ou "none"

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
    std::atomic<int> noteLeft { -1 }, noteRight { -1 };      // CLASSIC / CHORDS et footswitch : CC dans ccMap, ou note MIDI
    std::atomic<bool> leftHeld { false }, rightHeld { false };
    std::atomic<float> autoMin { 1.0f }, autoMax { 0.0f };          // plage réellement envoyée par la pédale MIDI (apprise automatiquement)
    std::atomic<bool> calibrating { false };
    std::atomic<float> calMin { 1.0f }, calMax { 0.0f }, lastRaw { -1.0f };
    std::atomic<juce::uint32> lastMoveMs { 0 };

    // changements demandés depuis le thread audio (MIDI) et appliqués sur le thread de la fenêtre
    // anti-gel v2 : file de valeurs (aucun appel à l'hôte depuis l'audio), vidée par le timer du processeur
    static constexpr int numQueued = 6;
    static const char* queuedId (int i);              // "program", "chords", "active", "smooth", "tone", "mix"
    std::atomic<float>* queuedPtr[numQueued] {};
    juce::RangedAudioParameter* queuedParam[numQueued] {};
    void setParamFromAudio (int i, float plain);
    float queuedOrCurrent (int i) const;
    std::atomic<float> pendingValue[numQueued];
    std::atomic<bool>  pendingSet[numQueued];
    int currentFactory = 0;
    std::atomic<int> activeFactory { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WharryProcessor)
};
