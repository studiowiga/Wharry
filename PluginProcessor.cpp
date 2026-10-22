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
        { "Studio Octave Up (HQ, latency)",     1,  true,  true,   25.0f, 1.00f, 1.00f },
    };
    return f[jlimit (0, kNumFactory - 1, i)];
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
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "heel", 1 }, "MIDI Pedal Calibration Heel (min)", NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "toe", 1 },  "MIDI Pedal Calibration Toe (max)",  NormalisableRange<float> (0.0f, 1.0f), 1.0f));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "invert", 1 }, "MIDI Pedal Invert", false));
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
}

WharryProcessor::~WharryProcessor()
{
    apvts.removeParameterListener ("pedal", this);
    cancelPendingUpdate();
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
    s.programIdx = jlimit (0, wharry::kNumPrograms - 1, (int) std::lround (pProgram->load()));
    s.chords   = pChords->load() > 0.5f;
    s.studio   = pStudio->load() > 0.5f;
    s.active   = pActive->load() > 0.5f;
    s.pedal    = jlimit (0.0f, 1.0f, pedalLive.load());
    s.smoothMs = pSmooth->load();
    s.tone     = pTone->load();
    s.mix      = pMix->load();
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
    if (id == "pedal") pedalLive = v;
}

// position MIDI brute (0..1) -> position pédale, avec calibration talon / pointe et inversion
float WharryProcessor::mapPedal (float raw) const
{
    const float lo = pHeel->load(), hi = pToe->load();
    float v = hi > lo + 0.01f ? (raw - lo) / (hi - lo) : raw;
    v = jlimit (0.0f, 1.0f, v);
    return pInvert->load() > 0.5f ? 1.0f - v : v;
}

// ------------------------------------------------------------------ MIDI
void WharryProcessor::handleMidiMessage (const MidiMessage& m)
{
    const auto learn = (MidiTarget) learnTarget.load();
    if (learn != MidiTarget::None && ((m.isController() && m.getControllerNumber() < 120) || m.isNoteOn()))
    {
        const int cc = m.isController() ? m.getControllerNumber() : -1;
        const int note = m.isNoteOn() ? m.getNoteNumber() : -1;
        if (learn == MidiTarget::Pedal)        { if (cc >= 0) ccPedal = cc; }
        else if (learn == MidiTarget::LeftFsw) { ccLeft = cc; noteLeft = note; }
        else                                   { ccRight = cc; noteRight = note; }
        if (learn != MidiTarget::Pedal || cc >= 0) learnTarget = 0;
        return;
    }

    if (m.isController())
    {
        const int c = m.getControllerNumber(), v = m.getControllerValue();
        if (c == ccPedal.load()) pedalLive = mapPedal ((float) v / 127.0f);
        const bool on = v >= 64;
        if (c == ccLeft.load())  { if (on && ! leftHeld.load())  { pendingToggleLeft = true;  triggerAsyncUpdate(); } leftHeld = on; }
        if (c == ccRight.load()) { if (on && ! rightHeld.load()) { pendingToggleRight = true; triggerAsyncUpdate(); } rightHeld = on; }
    }
    else if (m.isNoteOn())
    {
        if (m.getNoteNumber() == noteLeft.load())  { pendingToggleLeft = true;  triggerAsyncUpdate(); }
        if (m.getNoteNumber() == noteRight.load()) { pendingToggleRight = true; triggerAsyncUpdate(); }
    }
    else if (m.isProgramChange() && pPC->load() > 0.5f)
    {
        // même logique que la Whammy 5 : 21 programmes actifs, 21 en bypass, en CLASSIC puis en CHORDS
        // (PC 1-21 Classic on, 22-42 Classic bypass, 43-63 Chords on, 64-84 Chords bypass ; numérotation 1-84)
        const int pc = m.getProgramChangeNumber();
        if (pc < 84)
        {
            const int group = pc / 21;
            pendingProgram = pc % 21;
            pendingChords  = group >= 2 ? 1 : 0;
            pendingActive  = (group % 2 == 0) ? 1 : 0;
            triggerAsyncUpdate();
        }
    }
}

void WharryProcessor::clearMidi (MidiTarget t)
{
    if (t == MidiTarget::Pedal)        ccPedal = 11;
    else if (t == MidiTarget::LeftFsw) { ccLeft = -1; noteLeft = -1; }
    else                               { ccRight = -1; noteRight = -1; }
    if (getLearnTarget() == t) learnTarget = 0;
}

String WharryProcessor::getMidiDescription (MidiTarget t) const
{
    if (t == MidiTarget::Pedal) return "CC " + String (ccPedal.load());
    const int cc = t == MidiTarget::LeftFsw ? ccLeft.load() : ccRight.load();
    const int note = t == MidiTarget::LeftFsw ? noteLeft.load() : noteRight.load();
    if (cc >= 0) return "CC " + String (cc);
    if (note >= 0) return "Note " + String (note);
    return {};
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
    if (lat != reportedLatency.load()) { reportedLatency = lat; latencyToSet = lat; triggerAsyncUpdate(); }
}

void WharryProcessor::handleAsyncUpdate()
{
    const int lat = latencyToSet.exchange (-1);
    if (lat >= 0) setLatencySamples (lat);

    const int prog = pendingProgram.exchange (-1);
    if (prog >= 0) setParamValue ("program", (float) prog);
    const int ch = pendingChords.exchange (-1);
    if (ch >= 0) setParamValue ("chords", (float) ch);
    const int ac = pendingActive.exchange (-1);
    if (ac >= 0) setParamValue ("active", (float) ac);
    if (pendingToggleLeft.exchange (false))  togglePedalSwitch ("chords");
    if (pendingToggleRight.exchange (false)) togglePedalSwitch ("active");
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
    setParamValue ("program", (float) jlimit (0, wharry::kNumPrograms - 1, p));
}

void WharryProcessor::loadFactory (int index)
{
    const auto& f = factory (index);
    currentFactory = jlimit (0, kNumFactory - 1, index);
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
    st.setProperty ("ccPedal", ccPedal.load(), nullptr);
    st.setProperty ("ccLeft", ccLeft.load(), nullptr);
    st.setProperty ("noteLeft", noteLeft.load(), nullptr);
    st.setProperty ("ccRight", ccRight.load(), nullptr);
    st.setProperty ("noteRight", noteRight.load(), nullptr);
    st.setProperty ("uiScaleIdx", uiScaleIdx.load(), nullptr);
    st.setProperty ("factory", currentFactory, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void WharryProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            const auto st = ValueTree::fromXml (*xml);
            apvts.replaceState (st);
            ccPedal   = (int) st.getProperty ("ccPedal", 11);
            ccLeft    = (int) st.getProperty ("ccLeft", -1);
            noteLeft  = (int) st.getProperty ("noteLeft", -1);
            ccRight   = (int) st.getProperty ("ccRight", -1);
            noteRight = (int) st.getProperty ("noteRight", -1);
            uiScaleIdx = jlimit (-1, 2, (int) st.getProperty ("uiScaleIdx", -1));
            currentFactory = jlimit (0, kNumFactory - 1, (int) st.getProperty ("factory", 0));
            pedalLive = pPedal->load();
            leftHeld = false; rightHeld = false;
        }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WharryProcessor(); }
