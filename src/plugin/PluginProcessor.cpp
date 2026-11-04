#include "PluginProcessor.h"
#include "Presets.h"
#if ! HV_HEADLESS
 #include "gui/PluginEditor.h"
#endif

namespace hv
{
using namespace params;

//==============================================================================
SlotParam::SlotParam (const ParamDef& def, int s, int i, juce::AudioParameterChoice* typeParam)
    : juce::AudioParameterFloat (juce::ParameterID { def.id, 1 }, def.name, juce::NormalisableRange<float> (0.0f, 1.0f), def.def,
                                 juce::AudioParameterFloatAttributes()
                                     .withStringFromValueFunction ([this] (float v, int) -> juce::String
                                     {
                                         if (auto* sp = spec()) return sp->format (sp->toReal (v));
                                         return juce::String (v, 3);
                                     })
                                     .withValueFromStringFunction ([this] (const juce::String& t) -> float
                                     {
                                         if (auto* sp = spec())
                                         {
                                             if (sp->map == Map::Choice)
                                                 for (int k = 0; k < sp->numChoices(); ++k)
                                                     if (t.equalsIgnoreCase (sp->choices[(size_t) k])) return sp->toNorm ((float) k);
                                             if (sp->map == Map::Toggle) return t.equalsIgnoreCase ("on") || t.getFloatValue() > 0.5f ? 1.0f : 0.0f;
                                             if (t.containsIgnoreCase ("inf")) return 0.0f;
                                             auto v = t.getFloatValue();
                                             if (t.containsIgnoreCase ("khz")) v *= 1000.0f;
                                             return sp->toNorm (v);
                                         }
                                         return t.getFloatValue();
                                     })),
      slot (s), index (i), type (typeParam)
{
}

const ParamSpec* SlotParam::spec() const
{
    const auto& ps = moduleInfo (type->getIndex()).params;
    return index < (int) ps.size() ? &ps[(size_t) index] : nullptr;
}

juce::String SlotParam::getName (int maxLen) const
{
    juce::String n = "S" + juce::String (slotLabel (slot)) + " ";
    if (auto* sp = spec()) n << sp->name;
    else n << "P" << (index + 1) << " (unused)";
    return n.substring (0, maxLen);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout HarshVoxProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto defs = buildParamDefs();
    auto typeParams = std::make_shared<std::array<juce::AudioParameterChoice*, kNumSlots>>();

    auto targetName = [typeParams] (int tgt) -> juce::String
    {
        if (tgt <= 0) return "None";
        const int s = (tgt - 1) / kTargetsPerSlot, k = (tgt - 1) % kTargetsPerSlot;
        juce::String n = "S" + juce::String (slotLabel (s)) + " ";
        if (k == kParamsPerSlot) return n + "Mix";
        const int t = (*typeParams)[(size_t) s] != nullptr ? (*typeParams)[(size_t) s]->getIndex() : 0;
        const auto& ps = moduleInfo (t).params;
        return n + (k < (int) ps.size() ? juce::String (ps[(size_t) k].name) : "P" + juce::String (k + 1));
    };

    // slot type params are created first (in definition order below) so SlotParams can reference them
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> all ((size_t) kNumParams);
    for (int s = 0; s < kNumSlots; ++s)
    {
        const auto& d = defs[(size_t) slotIndex (s, SType)];
        juce::StringArray ch;
        for (auto& c : d.choices) ch.add (c);
        auto p = std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { d.id, 1 }, d.name, ch, 0);
        (*typeParams)[(size_t) s] = p.get();
        all[(size_t) slotIndex (s, SType)] = std::move (p);
    }
    for (int i = 0; i < kNumParams; ++i)
    {
        if (all[(size_t) i] != nullptr) continue;
        const auto& d = defs[(size_t) i];
        const int rel = i - slotIndex (0, 0);
        if (rel >= 0 && rel % kSlotFields >= SP0)
        {
            const int s = rel / kSlotFields, k = rel % kSlotFields - SP0;
            all[(size_t) i] = std::make_unique<SlotParam> (d, s, k, (*typeParams)[(size_t) s]);
        }
        else if (d.isBool)
            all[(size_t) i] = std::make_unique<juce::AudioParameterBool> (juce::ParameterID { d.id, 1 }, d.name, d.def >= 0.5f);
        else if (! d.choices.empty())
        {
            juce::StringArray ch;
            for (auto& c : d.choices) ch.add (c);
            all[(size_t) i] = std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { d.id, 1 }, d.name, ch, (int) d.def);
        }
        else if (i >= modIndex (0, 0) && i < modIndex (kNumMods, 0) && (i - modIndex (0, 0)) % kModFields == MTgt)
            all[(size_t) i] = std::make_unique<juce::AudioParameterInt> (juce::ParameterID { d.id, 1 }, d.name, (int) d.min, (int) d.max, (int) d.def,
                                juce::AudioParameterIntAttributes().withStringFromValueFunction ([targetName] (int v, int) { return targetName (v); }));
        else if (d.isInt)
            all[(size_t) i] = std::make_unique<juce::AudioParameterInt> (juce::ParameterID { d.id, 1 }, d.name, (int) d.min, (int) d.max, (int) d.def);
        else
        {
            juce::String unit;
            if (d.id.find ("gain") != std::string::npos || d.id.find ("_out") != std::string::npos) unit = " dB";
            else if (d.id.find ("mix") != std::string::npos) unit = " %";
            else if (d.id.find ("env_") != std::string::npos || d.id.find ("smooth") != std::string::npos) unit = " ms";
            all[(size_t) i] = std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { d.id, 1 }, d.name,
                                juce::NormalisableRange<float> (d.min, d.max), d.def,
                                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([unit] (float v, int) { return juce::String (v, 2) + unit; }));
        }
    }
    for (auto& p : all) layout.add (std::move (p));
    return layout;
}

HarshVoxProcessor::HarshVoxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HarshVox", createLayout())
{
    const auto defs = buildParamDefs();
    ParamRefs refs;
    plist.resize ((size_t) kNumParams);
    for (int i = 0; i < kNumParams; ++i)
    {
        plist[(size_t) i] = apvts.getParameter (defs[(size_t) i].id);
        refs.p[(size_t) i] = apvts.getRawParameterValue (defs[(size_t) i].id);
        jassert (plist[(size_t) i] != nullptr && refs.p[(size_t) i] != nullptr);
    }
    eng.bind (refs);
    for (int s = 0; s < kNumSlots; ++s) lastTypes[(size_t) s] = slotType (s);
    applyFactoryPreset (*this, 0);
#if ! HV_HEADLESS
    startTimerHz (10);
#endif
}

HarshVoxProcessor::~HarshVoxProcessor() { stopTimer(); }

bool HarshVoxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void HarshVoxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    eng.prepare (sampleRate, samplesPerBlock);
    prepared = true;
    refreshLatency();
}

void HarshVoxProcessor::refreshLatency()
{
    const int l = eng.latencyFromParams();
    if (l != lastLatency)
    {
        lastLatency = l;
        setLatencySamples (l);
    }
}

void HarshVoxProcessor::timerCallback()
{
    if (prepared) refreshLatency();
    bool changed = false;
    for (int s = 0; s < kNumSlots; ++s)
    {
        const int t = slotType (s);
        if (t != lastTypes[(size_t) s]) { lastTypes[(size_t) s] = t; changed = true; }
    }
    if (changed) updateHostDisplay (ChangeDetails().withParameterInfoChanged (true));
}

void HarshVoxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels();
    if (buffer.getNumChannels() < 2 || n == 0)
    {
        buffer.clear();
        return;
    }
    const float* inL = buffer.getReadPointer (0);
    const float* inR = inCh > 1 ? buffer.getReadPointer (1) : nullptr;
    // engine reads input sample i before writing output sample i within each sub-block, but it writes
    // outputs for the whole sub-block after reading it — safe for in-place processing.
    eng.process (inL, inR, buffer.getWritePointer (0), buffer.getWritePointer (1), n);
    for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
}

juce::AudioProcessorEditor* HarshVoxProcessor::createEditor()
{
#if HV_HEADLESS
    return nullptr;
#else
    return new HarshVoxEditor (*this);
#endif
}

bool HarshVoxProcessor::hasEditor() const
{
#if HV_HEADLESS
    return false;
#else
    return true;
#endif
}

//==============================================================================
int HarshVoxProcessor::getNumPrograms() { return std::max (1, numFactoryPresets()); }
void HarshVoxProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= numFactoryPresets()) return;
    currentProgram = index;
    applyFactoryPreset (*this, index);
}
const juce::String HarshVoxProcessor::getProgramName (int index) { return factoryPresetName (index); }

void HarshVoxProcessor::setParamNorm (int index, float norm)
{
    auto* p = plist[(size_t) index];
    p->beginChangeGesture();
    p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
    p->endChangeGesture();
}

void HarshVoxProcessor::setParamReal (int index, float real)
{
    auto* p = plist[(size_t) index];
    setParamNorm (index, p->convertTo0to1 (real));
}

int HarshVoxProcessor::slotType (int slot) const
{
    return (int) std::lround (paramReal (slotIndex (slot, SType)));
}

void HarshVoxProcessor::setSlotType (int slot, ModuleType type)
{
    setParamReal (slotIndex (slot, SType), (float) (int) type);
    const auto& specs = moduleInfo (type).params;
    for (int k = 0; k < kParamsPerSlot; ++k)
        setParamNorm (slotIndex (slot, SP0 + k), k < (int) specs.size() ? specs[(size_t) k].toNorm (specs[(size_t) k].def) : 0.0f);
    setParamReal (slotIndex (slot, SOn), 1.0f);
    setParamReal (slotIndex (slot, SMix), 100.0f);
    setParamReal (slotIndex (slot, SOut), 0.0f);
}

HarshVoxProcessor::SlotState HarshVoxProcessor::readSlot (int s) const
{
    SlotState st {};
    st.type = slotType (s);
    st.on   = plist[(size_t) slotIndex (s, SOn)]->getValue();
    st.mix  = plist[(size_t) slotIndex (s, SMix)]->getValue();
    st.out  = plist[(size_t) slotIndex (s, SOut)]->getValue();
    for (int k = 0; k < kParamsPerSlot; ++k) st.p[k] = plist[(size_t) slotIndex (s, SP0 + k)]->getValue();
    st.captured = const_cast<Engine&> (eng).slotShared (s).getCaptured();
    return st;
}

void HarshVoxProcessor::writeSlot (int s, const SlotState& st)
{
    setParamReal (slotIndex (s, SType), (float) st.type);
    setParamNorm (slotIndex (s, SOn), st.on);
    setParamNorm (slotIndex (s, SMix), st.mix);
    setParamNorm (slotIndex (s, SOut), st.out);
    for (int k = 0; k < kParamsPerSlot; ++k) setParamNorm (slotIndex (s, SP0 + k), st.p[k]);
    eng.slotShared (s).setCaptured (st.captured);
}

void HarshVoxProcessor::moveSlot (int from, int to)
{
    if (from == to || from < 0 || to < 0 || from >= kMainSlots || to >= kMainSlots) return;
    std::array<SlotState, kMainSlots> st;
    for (int s = 0; s < kMainSlots; ++s) st[(size_t) s] = readSlot (s);
    const auto moving = st[(size_t) from];
    if (from < to) std::rotate (st.begin() + from, st.begin() + from + 1, st.begin() + to + 1);
    else           std::rotate (st.begin() + to, st.begin() + from, st.begin() + from + 1);
    st[(size_t) to] = moving;
    for (int s = 0; s < kMainSlots; ++s) writeSlot (s, st[(size_t) s]);
}

void HarshVoxProcessor::copySlot (int from, int to)
{
    if (from == to) return;
    writeSlot (to, readSlot (from));
}

void HarshVoxProcessor::clearSlot (int slot) { setSlotType (slot, ModuleType::Empty); }

void HarshVoxProcessor::resetToDefaults()
{
    const auto defs = buildParamDefs();
    for (int i = 0; i < kNumParams; ++i)
        plist[(size_t) i]->setValueNotifyingHost (plist[(size_t) i]->getDefaultValue());
    for (int s = 0; s < kNumSlots; ++s) eng.slotShared (s).setCaptured (Formants {});
}

//==============================================================================
juce::ValueTree HarshVoxProcessor::capturesToTree() const
{
    juce::ValueTree t ("Captures");
    for (int s = 0; s < kNumSlots; ++s)
    {
        const auto f = const_cast<Engine&> (eng).slotShared (s).getCaptured();
        if (f.count <= 0) continue;
        juce::ValueTree c ("Capture");
        c.setProperty ("slot", s, nullptr);
        c.setProperty ("count", f.count, nullptr);
        juce::String fs, bs;
        for (int i = 0; i < kMaxFormants; ++i) { fs << f.freq[i] << " "; bs << f.bw[i] << " "; }
        c.setProperty ("freq", fs.trim(), nullptr);
        c.setProperty ("bw", bs.trim(), nullptr);
        t.appendChild (c, nullptr);
    }
    return t;
}

void HarshVoxProcessor::capturesFromTree (const juce::ValueTree& t)
{
    for (int s = 0; s < kNumSlots; ++s) eng.slotShared (s).setCaptured (Formants {});
    for (const auto& c : t)
    {
        const int s = c.getProperty ("slot");
        if (s < 0 || s >= kNumSlots) continue;
        Formants f;
        f.count = juce::jlimit (0, kMaxFormants, (int) c.getProperty ("count"));
        auto fs = juce::StringArray::fromTokens (c.getProperty ("freq").toString(), " ", "");
        auto bs = juce::StringArray::fromTokens (c.getProperty ("bw").toString(), " ", "");
        for (int i = 0; i < kMaxFormants; ++i)
        {
            f.freq[i] = i < fs.size() ? fs[i].getFloatValue() : 0.0f;
            f.bw[i]   = i < bs.size() ? bs[i].getFloatValue() : 80.0f;
        }
        eng.slotShared (s).setCaptured (f);
    }
}

void HarshVoxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPresetName, nullptr);
    state.setProperty ("version", HV_VERSION_STRING, nullptr);
    state.removeChild (state.getChildWithName ("Captures"), nullptr);
    state.appendChild (capturesToTree(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void HarshVoxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.hasType (apvts.state.getType())) return;
        capturesFromTree (tree.getChildWithName ("Captures"));
        currentPresetName = tree.getProperty ("preset", "Init").toString();
        tree.removeChild (tree.getChildWithName ("Captures"), nullptr);
        apvts.replaceState (tree);
        // APVTS skips parameters whose snapped value already matches the tree (e.g. a bool sitting at a raw
        // 0.74), so push every stored value explicitly to guarantee an exact restore.
        for (auto* p : plist)
        {
            auto child = tree.getChildWithProperty ("id", p->getParameterID());
            const float norm = child.isValid() ? p->convertTo0to1 ((float) child.getProperty ("value")) : p->getDefaultValue();
            if (p->getValue() != norm) p->setValueNotifyingHost (norm);
        }
        if (prepared) refreshLatency();
    }
}

juce::File HarshVoxProcessor::userPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Static Ruin").getChildFile ("Harsh Vox").getChildFile ("Presets");
}

bool HarshVoxProcessor::savePresetFile (const juce::File& f)
{
    juce::MemoryBlock mb;
    auto state = apvts.copyState();
    state.setProperty ("preset", f.getFileNameWithoutExtension(), nullptr);
    state.appendChild (capturesToTree(), nullptr);
    f.getParentDirectory().createDirectory();
    if (auto xml = state.createXml())
    {
        currentPresetName = f.getFileNameWithoutExtension();
        return xml->writeTo (f);
    }
    return false;
}

bool HarshVoxProcessor::loadPresetFile (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr) return false;
    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.hasType (apvts.state.getType())) return false;
    capturesFromTree (tree.getChildWithName ("Captures"));
    tree.removeChild (tree.getChildWithName ("Captures"), nullptr);
    // apply through parameters so the host is notified
    for (int i = 0; i < kNumParams; ++i)
    {
        auto* p = plist[(size_t) i];
        auto child = tree.getChildWithProperty ("id", p->getParameterID());
        if (child.isValid()) p->setValueNotifyingHost (p->convertTo0to1 ((float) child.getProperty ("value")));
        else p->setValueNotifyingHost (p->getDefaultValue());
    }
    currentPresetName = f.getFileNameWithoutExtension();
    return true;
}
} // namespace hv

#if ! HV_HEADLESS
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new hv::HarshVoxProcessor(); }
#endif
