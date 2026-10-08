// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Modular/EffectSwitch.h"

namespace {

juce::String percentText (const juce::NormalisableRange<float>& range, float value)
{
    const float unit = range.convertTo0to1 (value);
    const float clamped = unit < 0.0f ? 0.0f : (unit > 1.0f ? 1.0f : unit);
    return juce::String (juce::roundToInt (clamped * 100.0f)) + "%";
}

float percentToValue (const juce::NormalisableRange<float>& range, const juce::String& text)
{
    const float percent = juce::jlimit (0.0f, 100.0f, text.retainCharacters ("0123456789.").getFloatValue());
    return range.convertFrom0to1 (percent / 100.0f);
}

}

RoninAudioProcessor::RoninAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    effectOn_ = new juce::AudioParameterBool (juce::ParameterID { "effectOn", 1 }, "Effect", false,
                                              juce::AudioParameterBoolAttributes().withStringFromValueFunction (
                                                  [] (bool on, int) { return on ? juce::String ("On") : juce::String ("Off"); }));
    addParameter (effectOn_);
    addKnobParameter (faceKnobBinding ("VCF", "CUTOFF"));
    addKnobParameter (faceKnobBinding ("VCF", "PEAK"));
    addKnobParameter (faceKnobBinding ("VCF", "MOD"));
    addKnobParameter (faceKnobBinding ("VCA 1", "INITIAL"));
    addKnobParameter (faceKnobBinding ("VCA 1", "MOD"));
    addKnobParameter (faceKnobBinding ("VCA 1", "LOW CUT"));
    addKnobParameter (faceKnobBinding ("VCA 2", "INITIAL"));
    addKnobParameter (faceKnobBinding ("VCA 2", "MOD"));
    addKnobParameter (faceKnobBinding ("EG 1", "ATTACK"));
    addKnobParameter (faceKnobBinding ("EG 1", "DECAY"));
    addKnobParameter (faceKnobBinding ("EG 1", "SUSTAIN"));
    addKnobParameter (faceKnobBinding ("EG 1", "RELEASE"));
    addKnobParameter (faceKnobBinding ("MG", "RATE"));
    addKnobParameter (faceKnobBinding ("MG", "PW"));
    addKnobParameter (faceKnobBinding ("VCO", "RANGE"));
    addKnobParameter (faceKnobBinding ("VCO", "FINE"));
    addKnobParameter (faceKnobBinding ("VCO", "PW"));
    addKnobParameter (faceKnobBinding ("VCO", "FM 1"));
    addKnobParameter (faceKnobBinding ("VCO", "FM 2"));
    addKnobParameter (faceKnobBinding ("EG 2", "HOLD"));
    addKnobParameter (faceKnobBinding ("EG 2", "DELAY"));
    addKnobParameter (faceKnobBinding ("EG 2", "ATTACK"));
    addKnobParameter (faceKnobBinding ("EG 2", "RELEASE"));
    addKnobParameter (faceKnobBinding ("INT", "TIME"));
    addKnobParameter (faceKnobBinding ("MIX", "LEVEL 1"));
    addKnobParameter (faceKnobBinding ("MIX", "LEVEL 2"));
    addKnobParameter (faceKnobBinding ("MIX", "LEVEL 3"));
    addKnobParameter (faceKnobBinding ("S&H", "RATE"));
    addKnobParameter (faceKnobBinding ("OUTPUT", "LEVEL"));
    addKnobParameter (faceKnobBinding ("OUTPUT", "MIX"));
    addKnobParameter (faceKnobBinding ("EXT IN", "THRESHOLD"));
    addKnobParameter (faceKnobBinding ("EXT IN", "RELEASE"));
    addKnobParameter (faceKnobBinding ("DIV", "RATIO SWITCH"));

    // VOICE tab settings (RONIN_Redesign §4.1). Not on the front panel.
    // TRI SHAPE: TRIANGLE for new patches / INIT / fresh instances; PARABOLA (legacy) for user format-1 states (M-R2).
    vcoTriShape_ = new juce::AudioParameterChoice (juce::ParameterID { "vcoTriShape", 1 }, "VCO Tri Shape",
                                                   juce::StringArray { "TRIANGLE", "PARABOLA (legacy)" }, 0);
    addParameter (vcoTriShape_);
    // HQ 2x (VCO + VCF): DECIDED (user) ships default OFF. 0 latency off, 23 samples on, reported (JCS R11).
    hqMode_ = new juce::AudioParameterBool (juce::ParameterID { "hqMode", 1 }, "HQ 2x", false,
                                            juce::AudioParameterBoolAttributes().withStringFromValueFunction (
                                                [] (bool on, int) { return on ? juce::String ("HQ ON") : juce::String ("OFF"); }));
    addParameter (hqMode_);

    extModuleIndex_ = graph.addModule (extIn);
    outputModuleIndex_ = graph.addModule (output);
    noiseModuleIndex_ = graph.addModule (noise);
    vcfModuleIndex_ = graph.addModule (vcf);
    vca1ModuleIndex_ = graph.addModule (vca1);
    vca2ModuleIndex_ = graph.addModule (vca2);
    eg1ModuleIndex_ = graph.addModule (eg1);
    mgModuleIndex_ = graph.addModule (mg);
    vcoModuleIndex_ = graph.addModule (vco);
    eg2ModuleIndex_ = graph.addModule (eg2);
    ringModuleIndex_ = graph.addModule (ring);
    dividerModuleIndex_ = graph.addModule (divider);
    inverterModuleIndex_ = graph.addModule (inverter);
    integratorModuleIndex_ = graph.addModule (integrator);
    mixerModuleIndex_ = graph.addModule (mixer);
    sampleHoldModuleIndex_ = graph.addModule (sampleHold);
    // A fresh instance is the INIT program: its cables, the default table, and Effect on.
    setCurrentProgram (kDefaultFactoryPreset);
}

void RoninAudioProcessor::addKnobParameter (const FaceKnobBinding& binding)
{
    if (binding.knob == FaceKnob::None || binding.parameterId == nullptr || binding.parameterId[0] == '\0')
        return;

    // The divider switch is two steps, /2 and /4. Every other knob is continuous travel.
    const bool ratio = binding.knob == FaceKnob::DividerRatio;
    const juce::NormalisableRange<float> range (binding.minimum, binding.maximum, ratio ? 1.0f : 0.0f);
    auto attributes = juce::AudioParameterFloatAttributes();
    if (ratio)
        attributes = attributes
                         .withStringFromValueFunction ([] (float value, int) { return value < 0.5f ? juce::String ("/2") : juce::String ("/4"); })
                         .withValueFromStringFunction ([] (const juce::String& text) { return text.contains ("4") ? 1.0f : 0.0f; });
    else
        attributes = attributes
                         .withStringFromValueFunction ([range] (float value, int) { return percentText (range, value); })
                         .withValueFromStringFunction ([range] (const juce::String& text) { return percentToValue (range, text); });
    auto* parameter = new juce::AudioParameterFloat (
        juce::ParameterID { binding.parameterId, 1 },
        binding.parameterName,
        range,
        binding.fallback,
        attributes);
    addParameter (parameter);

    if (binding.knob == FaceKnob::VcfCutoff)
        vcfCutoff_ = parameter;
    else if (binding.knob == FaceKnob::VcfPeak)
        vcfPeak_ = parameter;
    else if (binding.knob == FaceKnob::VcfAmount)
        vcfAmount_ = parameter;
    else if (binding.knob == FaceKnob::Vca1Initial)
        vca1Initial_ = parameter;
    else if (binding.knob == FaceKnob::Vca1Mod)
        vca1Mod_ = parameter;
    else if (binding.knob == FaceKnob::Vca1LowCut)
        vca1LowCut_ = parameter;
    else if (binding.knob == FaceKnob::Vca2Initial)
        vca2Initial_ = parameter;
    else if (binding.knob == FaceKnob::Vca2Mod)
        vca2Mod_ = parameter;
    else if (binding.knob == FaceKnob::Eg1Attack)
        eg1Attack_ = parameter;
    else if (binding.knob == FaceKnob::Eg1Decay)
        eg1Decay_ = parameter;
    else if (binding.knob == FaceKnob::Eg1Sustain)
        eg1Sustain_ = parameter;
    else if (binding.knob == FaceKnob::Eg1Release)
        eg1Release_ = parameter;
    else if (binding.knob == FaceKnob::MgRate)
        mgRate_ = parameter;
    else if (binding.knob == FaceKnob::MgPw)
        mgPw_ = parameter;
    else if (binding.knob == FaceKnob::VcoRange)
        vcoRange_ = parameter;
    else if (binding.knob == FaceKnob::VcoFine)
        vcoFine_ = parameter;
    else if (binding.knob == FaceKnob::VcoPw)
        vcoPw_ = parameter;
    else if (binding.knob == FaceKnob::VcoFm1)
        vcoFm1_ = parameter;
    else if (binding.knob == FaceKnob::VcoFm2)
        vcoFm2_ = parameter;
    else if (binding.knob == FaceKnob::Eg2Hold)
        eg2Hold_ = parameter;
    else if (binding.knob == FaceKnob::Eg2Delay)
        eg2Delay_ = parameter;
    else if (binding.knob == FaceKnob::Eg2Attack)
        eg2Attack_ = parameter;
    else if (binding.knob == FaceKnob::Eg2Release)
        eg2Release_ = parameter;
    else if (binding.knob == FaceKnob::IntegratorTime)
        integratorTime_ = parameter;
    else if (binding.knob == FaceKnob::MixerLevel1)
        mixerLevel1_ = parameter;
    else if (binding.knob == FaceKnob::MixerLevel2)
        mixerLevel2_ = parameter;
    else if (binding.knob == FaceKnob::MixerLevel3)
        mixerLevel3_ = parameter;
    else if (binding.knob == FaceKnob::SampleHoldRate)
        sampleHoldRate_ = parameter;
    else if (binding.knob == FaceKnob::OutputLevel)
        outputLevel_ = parameter;
    else if (binding.knob == FaceKnob::OutputMix)
        outputMix_ = parameter;
    else if (binding.knob == FaceKnob::ExtInThreshold)
        extInThreshold_ = parameter;
    else if (binding.knob == FaceKnob::ExtInRelease)
        extInRelease_ = parameter;
    else if (binding.knob == FaceKnob::DividerRatio)
        dividerRatio_ = parameter;
}

juce::AudioParameterFloat* RoninAudioProcessor::floatParameter (FaceKnob knob) const noexcept
{
    if (knob == FaceKnob::VcfCutoff)
        return vcfCutoff_;
    if (knob == FaceKnob::VcfPeak)
        return vcfPeak_;
    if (knob == FaceKnob::VcfAmount)
        return vcfAmount_;
    if (knob == FaceKnob::Vca1Initial)
        return vca1Initial_;
    if (knob == FaceKnob::Vca1Mod)
        return vca1Mod_;
    if (knob == FaceKnob::Vca1LowCut)
        return vca1LowCut_;
    if (knob == FaceKnob::Vca2Initial)
        return vca2Initial_;
    if (knob == FaceKnob::Vca2Mod)
        return vca2Mod_;
    if (knob == FaceKnob::Eg1Attack)
        return eg1Attack_;
    if (knob == FaceKnob::Eg1Decay)
        return eg1Decay_;
    if (knob == FaceKnob::Eg1Sustain)
        return eg1Sustain_;
    if (knob == FaceKnob::Eg1Release)
        return eg1Release_;
    if (knob == FaceKnob::MgRate)
        return mgRate_;
    if (knob == FaceKnob::MgPw)
        return mgPw_;
    if (knob == FaceKnob::VcoRange)
        return vcoRange_;
    if (knob == FaceKnob::VcoFine)
        return vcoFine_;
    if (knob == FaceKnob::VcoPw)
        return vcoPw_;
    if (knob == FaceKnob::VcoFm1)
        return vcoFm1_;
    if (knob == FaceKnob::VcoFm2)
        return vcoFm2_;
    if (knob == FaceKnob::Eg2Hold)
        return eg2Hold_;
    if (knob == FaceKnob::Eg2Delay)
        return eg2Delay_;
    if (knob == FaceKnob::Eg2Attack)
        return eg2Attack_;
    if (knob == FaceKnob::Eg2Release)
        return eg2Release_;
    if (knob == FaceKnob::IntegratorTime)
        return integratorTime_;
    if (knob == FaceKnob::MixerLevel1)
        return mixerLevel1_;
    if (knob == FaceKnob::MixerLevel2)
        return mixerLevel2_;
    if (knob == FaceKnob::MixerLevel3)
        return mixerLevel3_;
    if (knob == FaceKnob::SampleHoldRate)
        return sampleHoldRate_;
    if (knob == FaceKnob::OutputLevel)
        return outputLevel_;
    if (knob == FaceKnob::OutputMix)
        return outputMix_;
    if (knob == FaceKnob::ExtInThreshold)
        return extInThreshold_;
    if (knob == FaceKnob::ExtInRelease)
        return extInRelease_;
    if (knob == FaceKnob::DividerRatio)
        return dividerRatio_;
    return nullptr;
}

juce::AudioProcessorParameter* RoninAudioProcessor::parameterForPanelKnob (const char* section, const char* label) const
{
    return floatParameter (faceKnobBinding (section, label).knob);
}

bool RoninAudioProcessor::effectIsOn() const noexcept
{
    return effectOn_ != nullptr && effectOn_->get();
}

void RoninAudioProcessor::applyHostControls()
{
    auto apply = [] (juce::AudioParameterFloat* parameter, Module& module, int knob)
    {
        if (parameter == nullptr)
            return;
        module.setKnob (knob, parameter->convertTo0to1 (parameter->get()));
    };

    apply (vcfCutoff_, vcf, Vcf::kKnobCutoff);
    apply (vcfPeak_, vcf, Vcf::kKnobPeak);
    apply (vcfAmount_, vcf, Vcf::kKnobAmount);
    apply (vca1Initial_, vca1, Vca1::kKnobInitial);
    apply (vca1Mod_, vca1, Vca1::kKnobIntensity);
    apply (vca1LowCut_, vca1, Vca1::kKnobLowCut);
    apply (vca2Initial_, vca2, Vca2::kKnobInitial);
    apply (vca2Mod_, vca2, Vca2::kKnobMod);
    apply (eg1Attack_, eg1, Eg1::kKnobAttack);
    apply (eg1Decay_, eg1, Eg1::kKnobDecay);
    apply (eg1Sustain_, eg1, Eg1::kKnobSustain);
    apply (eg1Release_, eg1, Eg1::kKnobRelease);
    apply (mgRate_, mg, MgModule::kKnobFrequency);
    apply (mgPw_, mg, MgModule::kKnobPw);
    apply (vcoRange_, vco, Vco::kKnobScale);
    apply (vcoFine_, vco, Vco::kKnobFine);
    apply (vcoPw_, vco, Vco::kKnobPw);
    apply (vcoFm1_, vco, Vco::kKnobAmountA);
    apply (vcoFm2_, vco, Vco::kKnobAmountB);
    apply (eg2Hold_, eg2, Eg2::kKnobHold);
    apply (eg2Delay_, eg2, Eg2::kKnobDelay);
    apply (eg2Attack_, eg2, Eg2::kKnobAttack);
    apply (eg2Release_, eg2, Eg2::kKnobRelease);
    apply (integratorTime_, integrator, Integrator::kKnobTime);
    apply (mixerLevel1_, mixer, Mixer::kKnobLevel1);
    apply (mixerLevel2_, mixer, Mixer::kKnobLevel2);
    apply (mixerLevel3_, mixer, Mixer::kKnobLevel3);
    apply (sampleHoldRate_, sampleHold, SampleHold::kKnobRate);
    apply (extInThreshold_, extIn, ExtIn::kKnobThreshold);
    apply (extInRelease_, extIn, ExtIn::kKnobRelease);
    vco.setTriShape (triShape());
    const float mixKnob = outputMix_ != nullptr ? outputMix_->convertTo0to1 (outputMix_->get()) : 1.0f;
    output.setMix (outputMixAfterSwitch (effectIsOn(), mixKnob));
    if (outputLevel_ != nullptr)
        output.setOutputLevel (outputLevel_->convertTo0to1 (outputLevel_->get()));
}

float RoninAudioProcessor::effectiveOutputMix() const noexcept
{
    const float mixKnob = outputMix_ != nullptr ? outputMix_->convertTo0to1 (outputMix_->get()) : 1.0f;
    return outputMixAfterSwitch (effectIsOn(), mixKnob);
}

int RoninAudioProcessor::copyPublishedCables (Cable* dest, int capacity) const
{
    return graph.copyPublishedCables (dest, capacity);
}

namespace {

bool onMessageThread()
{
    auto* messages = juce::MessageManager::getInstanceWithoutCreating();
    return messages != nullptr && messages->isThisTheMessageThread();
}

}

PatchGraph::ConnectResult RoninAudioProcessor::connectJacks (int sourceModule, int sourcePort,
                                                                  int destModule, int destPort)
{
    jassert (onMessageThread());
    if (! onMessageThread())
        return PatchGraph::ConnectResult::Rejected;
    return graph.attemptConnect (sourceModule, sourcePort, destModule, destPort);
}

void RoninAudioProcessor::disconnectJacks (int sourceModule, int sourcePort, int destModule, int destPort)
{
    jassert (onMessageThread());
    if (! onMessageThread())
        return;
    graph.disconnect (sourceModule, sourcePort, destModule, destPort);
}

void RoninAudioProcessor::setOutputMix (float zeroToOne)
{
    jassert (onMessageThread());
    if (! onMessageThread())
        return;
    const float clamped = zeroToOne < 0.0f ? 0.0f : (zeroToOne > 1.0f ? 1.0f : zeroToOne);
    if (outputMix_ != nullptr)
        outputMix_->setValueNotifyingHost (outputMix_->convertTo0to1 (clamped));
    applyHostControls();
}

void RoninAudioProcessor::setExtInButtonHeld (bool held)
{
    extIn.setButtonHeld (held);
}

bool RoninAudioProcessor::extInButtonHeld() const noexcept
{
    return extIn.buttonHeld();
}

float RoninAudioProcessor::meterVolts() const noexcept
{
    const int module = meter_.readingModule (outputModuleIndex_);
    const int port = meter_.readingPort (2);
    return graph.portVolts (module, port);
}

RoninAudioProcessor::~RoninAudioProcessor()
{
    cancelPendingUpdate();
}

Vco::TriShape RoninAudioProcessor::triShape() const noexcept
{
    return vcoTriShape_ != nullptr && vcoTriShape_->getIndex() == 1 ? Vco::TriShape::Parabola : Vco::TriShape::Triangle;
}

double RoninAudioProcessor::engineSampleRate() const noexcept
{
    return hostRate_ * (hqActive() ? 2.0 : 1.0);
}

// HQ 2x (RONIN_Redesign §3.2, DESIGN CHOICE): the whole graph runs in one 2fs domain, as SHOGUN v2.2 §3.4 does,
// so the VCO and VCF (and everything they feed) are oversampled and latencies never stack. Host input enters
// through a zero-latency sample-and-hold; each host output leaves through the shared 93-tap half-band decimator
// (jidai::dsp::Downsampler2x from jidai-common; u0 = the earlier 2fs sample, u1 = the later one).
// Latency: 0 off, 23 base samples on, reported to the host (JCS R11). Default OFF (decided by the user).
void RoninAudioProcessor::prepareEngine (double hostRate, bool hq)
{
    hostRate_ = hostRate > 0.0 ? hostRate : 48000.0;
    hqActive_.store (hq, std::memory_order_relaxed);
    graph.prepare (engineSampleRate());
    decimatorL_.reset();
    decimatorR_.reset();
}

const PortDesc RoninAudioProcessor::portDesc (int module, int port) const
{
    auto* m = const_cast<PatchGraph&> (graph).moduleAt (module);
    if (m == nullptr || port < 0 || port >= m->numPorts())
        return { "", PortType::CV, PortDir::In };
    return m->port (port);
}

bool RoninAudioProcessor::setCableColour (int index, std::uint32_t argb)
{
    jassert (onMessageThread());
    return onMessageThread() && graph.setCableColour (index, argb);
}

bool RoninAudioProcessor::setCableLegacyInvert (int index, bool legacy)
{
    jassert (onMessageThread());
    return onMessageThread() && graph.setCableLegacyInvert (index, legacy);
}

void RoninAudioProcessor::handleAsyncUpdate()
{
    setLatencySamples (latencyForHq (hqActive()));
}

void RoninAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const bool hq = hqMode_ != nullptr && hqMode_->get();
    loadMeasurer_.reset (sampleRate, samplesPerBlock);
    prepareEngine (sampleRate, hq);
    applyHostControls();
    setLatencySamples (latencyForHq (hq));
}

void RoninAudioProcessor::releaseResources()
{
}

bool RoninAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.inputBuses.size() != 1 || layouts.outputBuses.size() != 1)
        return false;

    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void RoninAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numInputs = getTotalNumInputChannels();
    const int numOutputs = getTotalNumOutputChannels();

    for (int channel = numInputs; channel < numOutputs; ++channel)
        buffer.clear (channel, 0, numSamples);

    if (numInputs < 2 || numOutputs < 2)
        return;

    float* left = buffer.getWritePointer (0);
    float* right = buffer.getWritePointer (1);

    juce::AudioProcessLoadMeasurer::ScopedTimer cpuTimer (loadMeasurer_, numSamples);
    const bool hqWanted = hqMode_ != nullptr && hqMode_->get();
    if (hqWanted != hqActive())
    {
        // The engine rate changes: every module re-prepares (no allocation). The host hears the new latency
        // from the message thread.
        prepareEngine (hostRate_, hqWanted);
        triggerAsyncUpdate();
    }
    applyHostControls();

    if (! hqActive())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            extIn.setHostSample (left[i], right[i]);
            graph.process();
            left[i] = output.hostLeft();
            right[i] = output.hostRight();
        }
        return;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        extIn.setHostSample (left[i], right[i]);
        const hq::StereoPair pair = hq::renderPair (graph, output);   // earlier sub-sample first, one per statement
        left[i] = hq::decimate (decimatorL_, pair.earlierLeft, pair.laterLeft);
        right[i] = hq::decimate (decimatorR_, pair.earlierRight, pair.laterRight);
    }
}

juce::AudioProcessorEditor* RoninAudioProcessor::createEditor()
{
    return new RoninAudioProcessorEditor (*this);
}

bool RoninAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String RoninAudioProcessor::getName() const
{
    return "RONIN";
}

bool RoninAudioProcessor::acceptsMidi() const
{
    return false;
}

bool RoninAudioProcessor::producesMidi() const
{
    return false;
}

bool RoninAudioProcessor::isMidiEffect() const
{
    return false;
}

double RoninAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int RoninAudioProcessor::getNumPrograms()
{
    return kFactoryPresetCount;
}

int RoninAudioProcessor::getCurrentProgram()
{
    return currentProgram_;
}

void RoninAudioProcessor::applyProgramParameters (int index)
{
    // Every host knob goes back to the one default table, then takes the program's own value if it has one
    // (FactoryPresets.h, knobs by host parameter id). INIT has none, so it is the table as is.
    auto restore = [index] (juce::AudioParameterFloat* parameter, float fallback)
    {
        if (parameter != nullptr)
            parameter->setValueNotifyingHost (factoryKnob (index, parameter->getParameterID().toRawUTF8(), fallback));
    };

    if (effectOn_ != nullptr)
        effectOn_->setValueNotifyingHost (factoryPresetEffect (index) ? 1.0f : 0.0f);

    const FactoryProgramKnobs knobs = factoryProgramKnobs (index);
    restore (vcfCutoff_, knobs.vcfCutoff);
    restore (vcfPeak_, knobs.vcfPeak);
    restore (vcfAmount_, PanelDefault::kVcfMod);
    restore (vca1Initial_, factoryVca1Initial (index));
    restore (vca1Mod_, PanelDefault::kVca1Mod);
    restore (vca1LowCut_, PanelDefault::kVca1LowCut);
    restore (vca2Initial_, PanelDefault::kVca2Initial);
    restore (vca2Mod_, PanelDefault::kVca2Mod);
    restore (eg1Attack_, knobs.eg1Attack);
    restore (eg1Decay_, knobs.eg1Decay);
    restore (eg1Sustain_, knobs.eg1Sustain);
    restore (eg1Release_, knobs.eg1Release);
    restore (mgRate_, factoryMgRate (index));
    restore (mgPw_, PanelDefault::kMgPw);
    restore (vcoRange_, knobs.vcoRange);
    restore (vcoFine_, PanelDefault::kVcoFine);
    restore (vcoPw_, PanelDefault::kVcoPw);
    restore (vcoFm1_, PanelDefault::kVcoFm1);
    restore (vcoFm2_, PanelDefault::kVcoFm2);
    restore (eg2Hold_, PanelDefault::kEg2Hold);
    restore (eg2Delay_, PanelDefault::kEg2Delay);
    restore (eg2Attack_, PanelDefault::kEg2Attack);
    restore (eg2Release_, PanelDefault::kEg2Release);
    restore (integratorTime_, factoryIntegratorTime (index));
    restore (mixerLevel1_, PanelDefault::kMixerLevel);
    restore (mixerLevel2_, PanelDefault::kMixerLevel);
    restore (mixerLevel3_, PanelDefault::kMixerLevel);
    restore (sampleHoldRate_, factorySampleHoldRate (index));
    restore (outputLevel_, PanelDefault::kOutputLevel);
    restore (outputMix_, PanelDefault::kOutputMix);
    restore (extInThreshold_, PanelDefault::kExtInThreshold);
    restore (extInRelease_, PanelDefault::kExtInRelease);
    restore (dividerRatio_, PanelDefault::kDividerRatio);
}

void RoninAudioProcessor::setCurrentProgram (int index)
{
    if (! loadFactoryPreset (graph, index))
        return;

    currentProgram_ = index;
    applyProgramParameters (index);
    output.setLevel (1.0f);
    // A new patch starts on the true TRIANGLE (M-R2, decided by the user).
    if (vcoTriShape_ != nullptr)
        vcoTriShape_->setValueNotifyingHost (0.0f);
    applyHostControls();
    graph.prepare (engineSampleRate());
}

const juce::String RoninAudioProcessor::getProgramName (int index)
{
    return factoryPresetName (index);
}

void RoninAudioProcessor::changeProgramName (int, const juce::String&)
{
}

RackIndices RoninAudioProcessor::rackIndices() const noexcept
{
    RackIndices r;
    r.ext = extModuleIndex_;
    r.output = outputModuleIndex_;
    r.noise = noiseModuleIndex_;
    r.vcf = vcfModuleIndex_;
    r.vca1 = vca1ModuleIndex_;
    r.vca2 = vca2ModuleIndex_;
    r.eg1 = eg1ModuleIndex_;
    r.mg = mgModuleIndex_;
    r.vco = vcoModuleIndex_;
    r.eg2 = eg2ModuleIndex_;
    r.ring = ringModuleIndex_;
    r.divider = dividerModuleIndex_;
    r.inverter = inverterModuleIndex_;
    r.integrator = integratorModuleIndex_;
    r.mixer = mixerModuleIndex_;
    r.sampleHold = sampleHoldModuleIndex_;
    return r;
}

// State format 2 (JCS R7): knobs by parameter id, cables by canonical jack id, oldest first. See PatchState.h.
void RoninAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement xml ("RONIN");
    xml.setAttribute ("format", patchstate::kFormat);
    xml.setAttribute ("unit", patchstate::kUnit);
    xml.setAttribute ("uiCableColour", cableColourByRole() ? "ROLE" : "MANUAL");
    xml.setAttribute ("uiEgTime", egTimeInMs() ? "ms" : "s");
    xml.setAttribute ("uiScale", uiScalePercent());
    for (auto* parameter : getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            xml.setAttribute (ranged->getParameterID(), static_cast<double> (parameter->getValue()));
    }

    const RackIndices rack = rackIndices();
    Cable cables[PatchGraph::kMaxCables];
    const int count = graph.copyPublishedCables (cables, PatchGraph::kMaxCables);
    auto* list = xml.createNewChildElement ("CABLES");
    for (int i = 0; i < count; ++i)
    {
        const std::string from = patchstate::jackId (rack, cables[i].sourceModule, cables[i].sourcePort);
        const std::string to = patchstate::jackId (rack, cables[i].destModule, cables[i].destPort);
        if (from.empty() || to.empty())
            continue;
        auto* cable = list->createNewChildElement ("CABLE");
        cable->setAttribute ("from", juce::String::fromUTF8 (from.c_str()));
        cable->setAttribute ("to", juce::String::fromUTF8 (to.c_str()));
        if (cables[i].legacyInvert)
            cable->setAttribute ("legacyInvert", 1);
        if (cables[i].colour != 0)
            cable->setAttribute ("colour", juce::String::toHexString (static_cast<juce::int64> (cables[i].colour)).paddedLeft ('0', 8));
    }
    copyXmlToBinary (xml, destData);
}

void RoninAudioProcessor::readParameters (const juce::XmlElement& xml)
{
    for (auto* parameter : getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
        {
            const juce::String id = ranged->getParameterID();
            if (xml.hasAttribute (id))
                parameter->setValue (static_cast<float> (xml.getDoubleAttribute (id)));
        }
    }
}

void RoninAudioProcessor::loadFormat2 (const juce::XmlElement& xml, int format)
{
    if (format > patchstate::kFormat)
        loadReport_.add ("This patch was saved by a newer RONIN (format " + juce::String (format)
                         + "). It was loaded best-effort; settings this version does not know were ignored.");

    const RackIndices rack = rackIndices();
    Cable cables[PatchGraph::kMaxCables];
    int count = 0;
    if (auto* list = xml.getChildByName ("CABLES"))
    {
        for (auto* item : list->getChildWithTagNameIterator ("CABLE"))
        {
            const juce::String from = item->getStringAttribute ("from");
            const juce::String to = item->getStringAttribute ("to");
            Cable cable;
            if (! patchstate::jackAddress (rack, from.toStdString(), cable.sourceModule, cable.sourcePort)
                || ! patchstate::jackAddress (rack, to.toStdString(), cable.destModule, cable.destPort))
            {
                loadReport_.add ("Skipped a cable with an unknown jack: " + from + " -> " + to);
                continue;
            }
            if (count >= PatchGraph::kMaxCables)
            {
                loadReport_.add ("Skipped cables beyond " + juce::String (PatchGraph::kMaxCables));
                break;
            }
            cable.legacyInvert = item->getIntAttribute ("legacyInvert", 0) != 0;
            cable.colour = static_cast<std::uint32_t> (item->getStringAttribute ("colour", "0").getHexValue64());
            cables[count++] = cable;
        }
    }
    // Illegal cables (direction or type) are dropped one by one so the rest of the patch still loads.
    Cable legal[PatchGraph::kMaxCables];
    int legalCount = 0;
    for (int i = 0; i < count; ++i)
    {
        if (graph.cableIsLegal (cables[i]))
            legal[legalCount++] = cables[i];
        else
            loadReport_.add ("Skipped a cable the patch bay does not allow.");
    }
    graph.setCables (legal, legalCount);
    readParameters (xml);
    setCableColourByRole (xml.getStringAttribute ("uiCableColour", "ROLE") != "MANUAL");
    setEgTimeInMs (xml.getStringAttribute ("uiEgTime", "s") == "ms");
    setUiScalePercent (juce::jlimit (75, 200, xml.getIntAttribute ("uiScale", 100)));
}

// Format 1: an index blob plus parameter attributes. Migrated per RONIN_Redesign §6 and reported.
void RoninAudioProcessor::loadFormat1 (const juce::XmlElement& xml)
{
    if (xml.hasAttribute ("graph"))
    {
        juce::MemoryBlock block;
        block.loadFromHexString (xml.getStringAttribute ("graph"));
        if (! graph.setState (block.getData(), static_cast<int> (block.getSize())))
        {
            presetError_ = graph.stateError();
            return;
        }
    }

    // vca1Initial is a host parameter now, under the attribute older saves already wrote.
    // Absent on saves from before that attribute. Those sessions keep Initial at 0.
    if (! xml.hasAttribute ("vca1Initial"))
    {
        vca1.setKnob (Vca1::kKnobInitial, 0.0f);
        if (vca1Initial_ != nullptr)
            *vca1Initial_ = 0.0f;
    }
    readParameters (xml);

    // M-R1: EG knobs keep their segment durations under the new 1 ms .. 60 s real-time law.
    // Only knobs the session stored are migrated; an absent attribute keeps the current (new-law) value.
    auto migrate = [&xml] (juce::AudioParameterFloat* parameter, bool attack, bool& stalled)
    {
        if (parameter == nullptr || ! xml.hasAttribute (parameter->getParameterID()))
            return;
        const double old = static_cast<double> (parameter->convertTo0to1 (parameter->get()));
        if (attack && patchstate::attackStalledInV1 (old))
            stalled = true;
        const double now = attack ? patchstate::migrateEgAttack (old) : patchstate::migrateEgDecayRelease (old);
        *parameter = parameter->convertFrom0to1 (static_cast<float> (now));
    };
    bool stalled = false;
    migrate (eg1Attack_, true, stalled);
    migrate (eg1Decay_, false, stalled);
    migrate (eg1Release_, false, stalled);
    migrate (eg2Attack_, true, stalled);
    migrate (eg2Release_, false, stalled);
    loadReport_.add ("EG times: knobs moved to the real-time scale; every segment keeps its length.");
    if (stalled)
        loadReport_.add ("EG attack: an attack that used to stall now reaches decay and sustain (a fix).");

    // M-R2: user-saved format-1 patches keep the parabola triangle.
    if (vcoTriShape_ != nullptr)
        *vcoTriShape_ = 1;
    loadReport_.add ("VCO TRI SHAPE set to PARABOLA (legacy), the shape this patch was saved with.");

    // M-R3: Gate -> non-S-trig cables keep their old inversion.
    Cable cables[PatchGraph::kMaxCables];
    const int count = graph.copyPublishedCables (cables, PatchGraph::kMaxCables);
    const int marked = patchstate::markLegacyInvert (graph, cables, count);
    if (marked > 0)
    {
        graph.setCables (cables, count);
        loadReport_.add (juce::String (marked) + (marked == 1 ? " cable kept its" : " cables kept their")
                         + " old S-trig inversion.");
    }

    // M-R5: drive-pull cutoff compensation, only for a direct VCO SAW or PULSE cable into VCF IN.
    const auto feed = patchstate::directVcfFeed (cables, count, rackIndices());
    const double level = patchstate::referenceLevel (feed);
    if (level > 0.0 && vcfCutoff_ != nullptr)
    {
        const double old = static_cast<double> (vcfCutoff_->convertTo0to1 (vcfCutoff_->get()));
        const double now = patchstate::compensateCutoff (old, level);
        *vcfCutoff_ = vcfCutoff_->convertFrom0to1 (static_cast<float> (now));
        loadReport_.add ("VCF CUTOFF " + juce::String (old, 3) + " -> " + juce::String (now, 3)
                         + " (same cutoff for the VCO " + (feed == patchstate::VcfFeed::VcoSaw ? "SAW" : "PULSE")
                         + " under the new drive pull).");
    }
    else if (feed != patchstate::VcfFeed::None)
    {
        loadReport_.add ("VCF CUTOFF left as saved: the VCF input is not a single direct VCO SAW or PULSE "
                         "cable, so its level is unknown. The filter may sound brighter under the new drive pull.");
    }
}

void RoninAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("RONIN"))
        return;

    const int format = xml->getIntAttribute ("format", 1);
    juce::StringArray previousReport;
    previousReport.swapWith (loadReport_);
    presetError_.clear();
    if (format >= 2)
        loadFormat2 (*xml, format);
    else
        loadFormat1 (*xml);
    if (presetError_.isNotEmpty())
    {
        loadReport_.swapWith (previousReport);   // a rejected load leaves everything as it was
        return;
    }
    loadedFormat_ = format;
    applyHostControls();
    graph.prepare (engineSampleRate());
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RoninAudioProcessor();
}
