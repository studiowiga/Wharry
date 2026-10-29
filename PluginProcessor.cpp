#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

// ------------------------------------------------------------------ presets usine
const FactoryPreset& WharryProcessor::factory (int i)
{
    static const FactoryPreset f[kNumFactory] = {
        //  nom                                programme  chords studio  glide   tone   mix
        { "Octave Scream (2 Oct Up)",           0,  false, false,  20.0f, 1.00f, 1.00f },
        { "Classic Octave Up",                  1,  false, false,  25.0f, 1.00f, 1.00f },
        { "Power Fifth Rise",                   2,  true,  false,  30.0f, 0.90f, 1.00f },
        { "Half-Step Drop (2nd Dn chords)",     4,  true,  false,  40.0f, 1.00f, 1.00f },
        { "Seven-String Sub (Oct Dn chords)",   7,  true,  false,  30.0f, 0.80f, 1.00f },
        { "Doom Floor (2 Oct Dn chords)",       8,  true,  false,  45.0f, 0.70f, 1.00f },
        { "Dive Bomb",                          9,  false, false,  15.0f, 1.00f, 1.00f },
        { "Shallow Chorus (Detune)",           10,  false, false,  60.0f, 0.85f, 0.90f },
        { "Deep Detune Wash",                  11,  true,  false,  60.0f, 0.80f, 1.00f },
        { "Octave Spread (Oct Dn/Oct Up)",     12,  true,  false,  35.0f, 0.90f, 0.85f },
        { "Bass Fifth Bend (5th/7th)",         14,  false, false,  30.0f, 0.90f, 0.90f },
        { "Twin Lead Thirds (3rd/4th)",        17,  false, false,  25.0f, 1.00f, 0.90f },
        { "Pedal Steel Bend (2nd/3rd)",        19,  false, false,  50.0f, 0.85f, 0.90f },
        { "Whammy Octave Down",                 7,  false, false,  25.0f, 1.00f, 1.00f },
    };
    return f[jlimit (0, kNumFactory - 1, i)];
}

// ------------------------------------------------------------------ MIDI Learn
const WharryProcessor::LearnDef& WharryProcessor::learnDef (int i)
{
    // pedale d'expression : CC 11 par defaut (standard Whammy, comme avant) ; les autres commandes ne sont pas assignees
    static const LearnDef defs[numLearn] =
    {
        { "pedal",  "Expression pedal",        11, learnContinuous, 0 },
        { "program","Program selector",        -1, learnSelector,   wharry::kNumPrograms },
        { "smooth", "Glide",                   -1, learnContinuous, 0 },
        { "tone",   "Tone",                    -1, learnContinuous, 0 },
        { "mix",    "Mix",                     -1, learnContinuous, 0 },
        { "chords", "CLASSIC / CHORDS switch", -1, learnFootswitch, 0 },   // bascule a chaque appui, comme avant
        { "active", "ON / OFF footswitch",     -1, learnFootswitch, 0 },
    };
    return defs[jlimit (0, numLearn - 1, i)];
}

void WharryProcessor::resetMidiMap()
{
    learnTarget = -1;
    for (int i = 0; i < numLearn; ++i) ccMap[i].store (learnDef (i).defaultCC);
    noteLeft = -1; noteRight = -1;
}

void WharryProcessor::clearLearn (int i)
{
    if (i < 0 || i >= numLearn) return;
    ccMap[i].store (-1);
    if (i == lChords) noteLeft = -1;
    if (i == lFs)     noteRight = -1;
    if (learnTarget.load() == i) learnTarget = -1;
}

String WharryProcessor::learnAssignText (int i) const
{
    const int cc = ccMap[jlimit (0, numLearn - 1, i)].load();
    if (cc >= 0) return "CC " + String (cc);
    const int note = i == lChords ? noteLeft.load() : (i == lFs ? noteRight.load() : -1);
    if (note >= 0) return "Note " + String (note);
    return "none";
}

// ------------------------------------------------------------------ paramètres
AudioProcessorValueTreeState::ParameterLayout WharryProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    StringArray names;
    for (int i = 0; i < wharry::kNumPrograms; ++i) names.add (wharry::program (i).name);

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "program", 1 }, "Program (selector)", names, 1));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "pedal", 1 }, "Expression Pedal (toe up 0 % / toe down 100 %)",
                                                        NormalisableRange<float> (0.0f, 1.0f), 0.0f,
                                                        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "chords", 1 }, "Classic / Chords (switch)", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "active", 1 }, "Effect On (footswitch)", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "studio", 1 }, "Chords Studio HQ engine (adds latency)", false));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "smooth", 1 }, "Glide (pedal smoothing)",
                                                        NormalisableRange<float> (0.0f, 250.0f, 0.0f, 0.5f), 30.0f,
                                                        AudioParameterFloatAttributes().withLabel ("ms")));
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "tone", 1 }, "Tone", NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 },  "Mix",  NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "heel", 1 }, "Pedal Calibration Heel (min)", NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "toe", 1 },  "Pedal Calibration Toe (max)",  NormalisableRange<float> (0.0f, 1.0f), 1.0f));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "invert", 1 }, "MIDI Pedal Invert", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "pc", 1 }, "Respond to MIDI Program Change (Whammy 5 map)", true));
    return { p.begin(), p.end() };
}

WharryProcessor::WharryProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "WHARRY", createLayout())
{
    pProgram = apvts.getRawParameterValue ("program");
    pPedal   = apvts.getRawParameterValue ("pedal");
    pChords  = apvts.getRawParameterValue ("chords");
    pStudio  = apvts.getRawParameterValue ("studio");
    pActive  = apvts.getRawParameterValue ("active");
    pSmooth  = apvts.getRawParameterValue ("smooth");
    pHeel    = apvts.getRawParameterValue ("heel");
    pToe     = apvts.getRawParameterValue ("toe");
    pInvert  = apvts.getRawParameterValue ("invert");
    pTone    = apvts.getRawParameterValue ("tone");
    pMix     = apvts.getRawParameterValue ("mix");
    pPC      = apvts.getRawParameterValue ("pc");
    apvts.addParameterListener ("pedal", this);
    pedalLive = pPedal->load();
    for (int i = 0; i < numQueued; ++i)
    {
        pendingValue[i] = 0.0f; pendingSet[i] = false;
        queuedPtr[i] = apvts.getRawParameterValue (queuedId (i));
        queuedParam[i] = apvts.getParameter (queuedId (i));
    }
    resetMidiMap();
    startTimerHz (60);   // anti-gel v2 : aucun appel à l'hôte depuis le thread audio
}

WharryProcessor::~WharryProcessor()
{
    stopTimer();
    apvts.removeParameterListener ("pedal", this);
}

bool WharryProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    return in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
}

wharry::Settings WharryProcessor::currentSettings() const
{
    wharry::Settings s;
    s.programIdx = jlimit (0, wharry::kNumPrograms - 1, (int) std::lround (pProgram->load (std::memory_order_relaxed)));
    s.chords   = pChords->load (std::memory_order_relaxed) > 0.5f;
    s.studio   = pStudio->load (std::memory_order_relaxed) > 0.5f;
    s.active   = pActive->load (std::memory_order_relaxed) > 0.5f;
    s.pedal    = jlimit (0.0f, 1.0f, pedalLive.load());
    // valeur MIDI encore en file (pas encore transmise par le timer) prise tout de suite ; le moteur lisse ensuite
    s.smoothMs = queuedOrCurrent (3);
    s.tone     = queuedOrCurrent (4);
    s.mix      = queuedOrCurrent (5);
    return s;
}

void WharryProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    mono.setSize (1, jmax (1, samplesPerBlock) * 2, false, true, true);
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate, samplesPerBlock);
    const int lat = engine.currentLatency();
    reportedLatency = lat;
    setLatencySamples (lat);
}

void WharryProcessor::parameterChanged (const String& id, float v)
{
    if (id == "pedal") { notePedalRaw (v, false); pedalLive = mapPedal (v, false); }
}

// position brute (0..1) -> position pédale :
//  1. plage de la pédale : calibration manuelle (talon / pointe) si elle a été faite, sinon plage apprise
//     automatiquement sur le MIDI reçu (une M-Audio EX-P envoie souvent ~2..116 au lieu de 0..127) ;
//  2. petite zone morte aux deux butées : pédale relevée = exactement 0, enfoncée = exactement 100 % ;
//  3. inversion éventuelle (MIDI seulement).
float WharryProcessor::mapPedal (float raw, bool fromMidi) const
{
    float lo = pHeel->load (std::memory_order_relaxed), hi = pToe->load (std::memory_order_relaxed);
    const bool manual = lo > 0.001f || hi < 0.999f;
    if (! manual && fromMidi && autoMax.load() - autoMin.load() > 0.4f) { lo = autoMin.load(); hi = autoMax.load(); }
    float v = hi > lo + 0.01f ? (raw - lo) / (hi - lo) : raw;
    constexpr float dz = 0.02f;
    v = jlimit (0.0f, 1.0f, (v - dz) / (1.0f - 2.0f * dz));
    return (fromMidi && pInvert->load (std::memory_order_relaxed) > 0.5f) ? 1.0f - v : v;
}

void WharryProcessor::notePedalRaw (float raw, bool fromMidi)
{
    if (fromMidi)
    {
        if (raw < autoMin.load()) autoMin = raw;
        if (raw > autoMax.load()) autoMax = raw;
    }
    if (calibrating.load())
    {
        if (raw < calMin.load()) calMin = raw;
        if (raw > calMax.load()) calMax = raw;
    }
    if (std::abs (raw - lastRaw.load()) > 0.004f) { lastRaw = raw; lastMoveMs = Time::getMillisecondCounter(); }
}

void WharryProcessor::startCalibration()
{
    calMin = 1.0f; calMax = 0.0f; lastMoveMs = Time::getMillisecondCounter();
    calibrating = true;
}

void WharryProcessor::finishCalibration()
{
    if (! calibrating.exchange (false)) return;
    const float lo = calMin.load(), hi = calMax.load();
    if (hi - lo > 0.3f)
    {
        setParamValue ("heel", jlimit (0.0f, 1.0f, lo));
        setParamValue ("toe", jlimit (0.0f, 1.0f, hi));
        if (hi > 0.999f && lo < 0.001f) { setParamValue ("heel", 0.0f); setParamValue ("toe", 1.0f); }
    }
}

void WharryProcessor::resetCalibration()
{
    calibrating = false;
    autoMin = 1.0f; autoMax = 0.0f;
    setParamValue ("heel", 0.0f); setParamValue ("toe", 1.0f);
}

String WharryProcessor::calibrationDescription() const
{
    const float lo = pHeel->load (std::memory_order_relaxed), hi = pToe->load (std::memory_order_relaxed);
    auto cc = [] (float v) { return String (roundToInt (v * 127.0f)); };
    if (lo > 0.001f || hi < 0.999f) return "calibrated " + cc (lo) + " - " + cc (hi);
    if (autoMax.load() - autoMin.load() > 0.4f) return "auto " + cc (autoMin.load()) + " - " + cc (autoMax.load());
    return "auto";
}

// ------------------------------------------------------------------ MIDI
void WharryProcessor::handleMidiMessage (const MidiMessage& m)
{
    // MIDI Learn : le premier CC recu (ou une note pour CLASSIC / CHORDS et le footswitch) est affecte a la commande visee
    const int lt = learnTarget.load();
    if (lt >= 0 && lt < numLearn)
    {
        if (m.isController() && m.getControllerNumber() < 120)
        {
            const int cc = m.getControllerNumber();
            const bool on = m.getControllerValue() >= 64;
            for (int j = 0; j < numLearn; ++j)
                if (j != lt && ccMap[j].load() == cc) ccMap[j].store (-1);   // numero deja pris : l'autre commande est desassignee
            ccMap[lt].store (cc);
            if (lt == lChords) { noteLeft = -1;  leftHeld = on; }
            if (lt == lFs)     { noteRight = -1; rightHeld = on; }
            lastLearned = lt; learnTarget = -1;
            return;
        }
        if (m.isNoteOn() && (lt == lChords || lt == lFs))
        {
            const int note = m.getNoteNumber();
            if (lt == lChords) { noteLeft = note;  if (noteRight.load() == note) noteRight = -1; }
            else               { noteRight = note; if (noteLeft.load() == note)  noteLeft = -1; }
            ccMap[lt].store (-1);
            lastLearned = lt; learnTarget = -1;
            return;
        }
    }

    if (m.isController())
    {
        const int c = m.getControllerNumber(), v = m.getControllerValue();
        if (c == ccMap[lPedal].load (std::memory_order_relaxed)) { const float raw = (float) v / 127.0f; notePedalRaw (raw, true); pedalLive = mapPedal (raw, true); }
        // sélecteur (21 programmes) et potards GLIDE / TONE / MIX appris : 0-127 réparti sur la course
        if (c == ccMap[lProgram].load (std::memory_order_relaxed))
        {
            activeFactory = -1;
            setParamFromAudio (0, (float) jmin (wharry::kNumPrograms - 1, v * wharry::kNumPrograms / 128));
        }
        for (int k = 0; k < 3; ++k)
            if (c == ccMap[lGlide + k].load (std::memory_order_relaxed))
                setParamFromAudio (3 + k, queuedParam[3 + k]->convertFrom0to1 ((float) v / 127.0f));
        const bool on = v >= 64;
        // anti-gel v2 : bascule mise en file (valeur en file si elle existe, sinon valeur courante)
        auto toggle = [this] (int qi) { setParamFromAudio (qi, queuedOrCurrent (qi) > 0.5f ? 0.0f : 1.0f); };
        if (c == ccMap[lChords].load (std::memory_order_relaxed)) { if (on && ! leftHeld.load())  toggle (1); leftHeld = on; }
        if (c == ccMap[lFs].load (std::memory_order_relaxed))     { if (on && ! rightHeld.load()) toggle (2); rightHeld = on; }
    }
    else if (m.isNoteOn())
    {
        auto toggle = [this] (int qi) { setParamFromAudio (qi, queuedOrCurrent (qi) > 0.5f ? 0.0f : 1.0f); };
        if (m.getNoteNumber() == noteLeft.load())  toggle (1);
        if (m.getNoteNumber() == noteRight.load()) toggle (2);
    }
    else if (m.isProgramChange() && pPC->load (std::memory_order_relaxed) > 0.5f)
    {
        // même logique que la Whammy 5 : 21 programmes actifs, 21 en bypass, en CLASSIC puis en CHORDS
        // (PC 1-21 Classic on, 22-42 Classic bypass, 43-63 Chords on, 64-84 Chords bypass ; numérotation 1-84)
        const int pc = m.getProgramChangeNumber();
        if (pc < 84)
        {
            const int group = pc / 21;
            setParamFromAudio (0, (float) (pc % 21));
            setParamFromAudio (1, group >= 2 ? 1.0f : 0.0f);
            setParamFromAudio (2, (group % 2 == 0) ? 1.0f : 0.0f);
        }
    }
}

void WharryProcessor::clearMidi (MidiTarget t)
{
    if (t == MidiTarget::Pedal) { ccMap[lPedal] = 11; if (learnTarget.load() == lPedal) learnTarget = -1; }   // « Reset to CC 11 »
    else clearLearn (learnIndexOf (t));
}

String WharryProcessor::getMidiDescription (MidiTarget t) const
{
    const auto d = learnAssignText (learnIndexOf (t));
    return d == "none" ? String() : d;
}

// ------------------------------------------------------------------ audio
void WharryProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nIn = getTotalNumInputChannels(), nOut = getTotalNumOutputChannels();
    if (n == 0) return;
    if (mono.getNumSamples() < n) return;   // ne devrait pas arriver (dimensionné avec marge)

    // entrée mono (guitare) : moyenne des canaux d'entrée
    float* m = mono.getWritePointer (0);
    if (nIn <= 0) FloatVectorOperations::clear (m, n);
    else
    {
        FloatVectorOperations::copy (m, buffer.getReadPointer (0), n);
        if (nIn > 1) { FloatVectorOperations::add (m, buffer.getReadPointer (1), n); FloatVectorOperations::multiply (m, 0.5f, n); }
    }

    // traitement découpé aux instants des messages MIDI : la pédale suit le contrôleur à l'échantillon près
    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = jlimit (0, n, meta.samplePosition);
        if (at > pos) { engine.setSettings (currentSettings()); engine.process (m + pos, m + pos, at - pos); pos = at; }
        handleMidiMessage (meta.getMessage());
    }
    if (pos < n) { engine.setSettings (currentSettings()); engine.process (m + pos, m + pos, n - pos); }

    for (int c = 0; c < nOut; ++c) buffer.copyFrom (c, 0, m, n);

    pedalShown = engine.smoothedPedal();
    lastBlockMs = Time::getMillisecondCounter();
    semis = engine.currentSemitones();
    detHz = engine.detectedHz();

    const int lat = engine.currentLatency();
    if (lat != reportedLatency.load()) { reportedLatency = lat; latencyToSet = lat; }   // transmise à l'hôte par le timer
}

// Anti-gel v2 (Studio Wiga) : jamais d'appel à l'hôte (setValueNotifyingHost, setLatencySamples) ni d'allocation
// depuis le thread audio. Le MIDI met la valeur en file (pendingValue / pendingSet) ; le timer du processeur
// (60 Hz, thread de messages) la transmet au DAW et à la fenêtre.
const char* WharryProcessor::queuedId (int i)
{
    static const char* ids[numQueued] = { "program", "chords", "active", "smooth", "tone", "mix" };
    return ids[jlimit (0, numQueued - 1, i)];
}

void WharryProcessor::setParamFromAudio (int i, float plain)
{
    pendingValue[i].store (plain, std::memory_order_relaxed);
    pendingSet[i].store (true, std::memory_order_release);
}

float WharryProcessor::queuedOrCurrent (int i) const
{
    if (pendingSet[i].load (std::memory_order_acquire)) return pendingValue[i].load (std::memory_order_relaxed);
    return queuedPtr[jlimit (0, numQueued - 1, i)]->load (std::memory_order_relaxed);
}

void WharryProcessor::timerCallback()
{
    const int lat = latencyToSet.exchange (-1);
    if (lat >= 0) setLatencySamples (lat);

    for (int i = 0; i < numQueued; ++i)
        if (pendingSet[i].exchange (false, std::memory_order_acquire))
            setParamValue (queuedId (i), pendingValue[i].load (std::memory_order_relaxed));
}

// ------------------------------------------------------------------ commandes de la fenêtre
void WharryProcessor::setParamValue (const String& id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        p->endChangeGesture();
    }
}

void WharryProcessor::togglePedalSwitch (const String& id)
{
    if (auto* p = apvts.getRawParameterValue (id)) setParamValue (id, p->load() > 0.5f ? 0.0f : 1.0f);
}

void WharryProcessor::selectWhammyProgram (int p)
{
    activeFactory = -1;
    setParamValue ("program", (float) jlimit (0, wharry::kNumPrograms - 1, p));
}

void WharryProcessor::loadFactory (int index)
{
    const auto& f = factory (index);
    currentFactory = jlimit (0, kNumFactory - 1, index);
    activeFactory = currentFactory;
    setParamValue ("program", (float) f.program);
    setParamValue ("chords", f.chords ? 1.0f : 0.0f);
    setParamValue ("studio", f.studio ? 1.0f : 0.0f);
    setParamValue ("smooth", f.glideMs);
    setParamValue ("tone", f.tone);
    setParamValue ("mix", f.mix);
    setParamValue ("active", 1.0f);
}

void WharryProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= kNumFactory) return;
    if (MessageManager::getInstanceWithoutCreating() != nullptr && MessageManager::getInstance()->isThisTheMessageThread())
        loadFactory (index);
}

// ------------------------------------------------------------------ état
AudioProcessorEditor* WharryProcessor::createEditor() { return new WharryEditor (*this); }

void WharryProcessor::getStateInformation (MemoryBlock& dest)
{
    auto st = apvts.copyState();
    st.setProperty ("ccPedal", ccMap[lPedal].load(), nullptr);      // anciennes cles (projets < 1.9)
    st.setProperty ("ccLeft", ccMap[lChords].load(), nullptr);
    st.setProperty ("noteLeft", noteLeft.load(), nullptr);
    st.setProperty ("ccRight", ccMap[lFs].load(), nullptr);
    for (int i = 0; i < numLearn; ++i)
        st.setProperty (String ("cc_") + learnDef (i).id, ccMap[i].load(), nullptr);
    st.setProperty ("noteRight", noteRight.load(), nullptr);
    st.setProperty ("uiScaleIdx", uiScaleIdx.load(), nullptr);
    st.setProperty ("factory", currentFactory, nullptr);
    st.setProperty ("autoMin", (double) autoMin.load(), nullptr);
    st.setProperty ("autoMax", (double) autoMax.load(), nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void WharryProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            const auto st = ValueTree::fromXml (*xml);
            apvts.replaceState (st);
            // assignations MIDI Learn (-1 = non assigne) ; projet < 1.9 : ccPedal / ccLeft / ccRight
            learnTarget = -1;
            for (int i = 0; i < numLearn; ++i)
            {
                const var legacy = i == lPedal  ? st.getProperty ("ccPedal", 11)
                                 : i == lChords ? st.getProperty ("ccLeft", -1)
                                 : i == lFs     ? st.getProperty ("ccRight", -1)
                                                : var (learnDef (i).defaultCC);
                ccMap[i].store (jlimit (-1, 127, (int) st.getProperty (String ("cc_") + learnDef (i).id, legacy)));
            }
            noteLeft  = (int) st.getProperty ("noteLeft", -1);
            noteRight = (int) st.getProperty ("noteRight", -1);
            uiScaleIdx = jlimit (-1, 2, (int) st.getProperty ("uiScaleIdx", -1));
            currentFactory = jlimit (0, kNumFactory - 1, (int) st.getProperty ("factory", 0));
            autoMin = (float) (double) st.getProperty ("autoMin", 1.0);
            autoMax = (float) (double) st.getProperty ("autoMax", 0.0);
            calibrating = false;
            pedalLive = mapPedal (pPedal->load(), false);
            leftHeld = false; rightHeld = false;
        }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WharryProcessor(); }
