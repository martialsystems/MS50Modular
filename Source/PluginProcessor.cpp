// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Modular/DefaultPatch.h"
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

MS50ModularAudioProcessor::MS50ModularAudioProcessor()
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
    addKnobParameter (faceKnobBinding ("VCA 1", "LOW CUT"));
    addKnobParameter (faceKnobBinding ("EG 1", "ATTACK"));
    addKnobParameter (faceKnobBinding ("EG 1", "DECAY"));
    addKnobParameter (faceKnobBinding ("EG 1", "SUSTAIN"));
    addKnobParameter (faceKnobBinding ("EG 1", "RELEASE"));

    extModuleIndex_ = graph.addModule (extIn);
    outputModuleIndex_ = graph.addModule (output);
    noiseModuleIndex_ = graph.addModule (noise);
    vcfModuleIndex_ = graph.addModule (vcf);
    vca1ModuleIndex_ = graph.addModule (vca1);
    vca2ModuleIndex_ = graph.addModule (vca2);
    eg1ModuleIndex_ = graph.addModule (eg1);
    connectFactoryCables (graph, extModuleIndex_, outputModuleIndex_, vcfModuleIndex_, vca1ModuleIndex_,
                          eg1ModuleIndex_);
    applyHostControls();
}

void MS50ModularAudioProcessor::addKnobParameter (const FaceKnobBinding& binding)
{
    if (binding.knob == FaceKnob::None || binding.parameterId == nullptr || binding.parameterId[0] == '\0')
        return;

    const juce::NormalisableRange<float> range (binding.minimum, binding.maximum);
    auto* parameter = new juce::AudioParameterFloat (
        juce::ParameterID { binding.parameterId, 1 },
        binding.parameterName,
        range,
        binding.fallback,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([range] (float value, int) { return percentText (range, value); })
            .withValueFromStringFunction ([range] (const juce::String& text) { return percentToValue (range, text); }));
    addParameter (parameter);

    if (binding.knob == FaceKnob::VcfCutoff)
        vcfCutoff_ = parameter;
    else if (binding.knob == FaceKnob::VcfPeak)
        vcfPeak_ = parameter;
    else if (binding.knob == FaceKnob::VcfAmount)
        vcfAmount_ = parameter;
    else if (binding.knob == FaceKnob::Vca1LowCut)
        vca1LowCut_ = parameter;
    else if (binding.knob == FaceKnob::Eg1Attack)
        eg1Attack_ = parameter;
    else if (binding.knob == FaceKnob::Eg1Decay)
        eg1Decay_ = parameter;
    else if (binding.knob == FaceKnob::Eg1Sustain)
        eg1Sustain_ = parameter;
    else if (binding.knob == FaceKnob::Eg1Release)
        eg1Release_ = parameter;
}

juce::AudioParameterFloat* MS50ModularAudioProcessor::floatParameter (FaceKnob knob) const noexcept
{
    if (knob == FaceKnob::VcfCutoff)
        return vcfCutoff_;
    if (knob == FaceKnob::VcfPeak)
        return vcfPeak_;
    if (knob == FaceKnob::VcfAmount)
        return vcfAmount_;
    if (knob == FaceKnob::Vca1LowCut)
        return vca1LowCut_;
    if (knob == FaceKnob::Eg1Attack)
        return eg1Attack_;
    if (knob == FaceKnob::Eg1Decay)
        return eg1Decay_;
    if (knob == FaceKnob::Eg1Sustain)
        return eg1Sustain_;
    if (knob == FaceKnob::Eg1Release)
        return eg1Release_;
    return nullptr;
}

juce::AudioProcessorParameter* MS50ModularAudioProcessor::parameterForPanelKnob (const char* section, const char* label) const
{
    return floatParameter (faceKnobBinding (section, label).knob);
}

bool MS50ModularAudioProcessor::effectIsOn() const noexcept
{
    return effectOn_ != nullptr && effectOn_->get();
}

void MS50ModularAudioProcessor::applyHostControls()
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
    apply (vca1LowCut_, vca1, Vca1::kKnobLowCut);
    apply (eg1Attack_, eg1, Eg1::kKnobAttack);
    apply (eg1Decay_, eg1, Eg1::kKnobDecay);
    apply (eg1Sustain_, eg1, Eg1::kKnobSustain);
    apply (eg1Release_, eg1, Eg1::kKnobRelease);
    output.setMix (outputMixForEffect (effectIsOn()));
}

int MS50ModularAudioProcessor::copyPublishedCables (Cable* dest, int capacity) const
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

PatchGraph::ConnectResult MS50ModularAudioProcessor::connectJacks (int sourceModule, int sourcePort,
                                                                  int destModule, int destPort)
{
    jassert (onMessageThread());
    if (! onMessageThread())
        return PatchGraph::ConnectResult::Rejected;
    return graph.attemptConnect (sourceModule, sourcePort, destModule, destPort);
}

void MS50ModularAudioProcessor::disconnectJacks (int sourceModule, int sourcePort, int destModule, int destPort)
{
    jassert (onMessageThread());
    if (! onMessageThread())
        return;
    graph.disconnect (sourceModule, sourcePort, destModule, destPort);
}

void MS50ModularAudioProcessor::setOutputMix (float zeroToOne)
{
    jassert (onMessageThread());
    if (! onMessageThread())
        return;
    output.setMix (zeroToOne);
}

MS50ModularAudioProcessor::~MS50ModularAudioProcessor() = default;

void MS50ModularAudioProcessor::prepareToPlay (double sampleRate, int)
{
    graph.prepare (sampleRate);
    extIn.prepare (sampleRate);
    output.prepare (sampleRate);
    noise.prepare (sampleRate);
    vcf.prepare (sampleRate);
    vca1.prepare (sampleRate);
    vca2.prepare (sampleRate);
    eg1.prepare (sampleRate);
    applyHostControls();
    setLatencySamples (0);
}

void MS50ModularAudioProcessor::releaseResources()
{
}

bool MS50ModularAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.inputBuses.size() != 1 || layouts.outputBuses.size() != 1)
        return false;

    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MS50ModularAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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
    applyHostControls();

    for (int i = 0; i < numSamples; ++i)
    {
        const float inLeft = left[i];
        const float inRight = right[i];
        extIn.setHostSample (inLeft, inRight);
        graph.process();
        left[i] = output.hostLeft();
        right[i] = output.hostRight();
    }
}

juce::AudioProcessorEditor* MS50ModularAudioProcessor::createEditor()
{
    return new MS50ModularAudioProcessorEditor (*this);
}

bool MS50ModularAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String MS50ModularAudioProcessor::getName() const
{
    return "MS-50 Modular";
}

bool MS50ModularAudioProcessor::acceptsMidi() const
{
    return false;
}

bool MS50ModularAudioProcessor::producesMidi() const
{
    return false;
}

bool MS50ModularAudioProcessor::isMidiEffect() const
{
    return false;
}

double MS50ModularAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MS50ModularAudioProcessor::getNumPrograms()
{
    // Hosts that divide by the program count reject a plugin that reports zero.
    return 1;
}

int MS50ModularAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MS50ModularAudioProcessor::setCurrentProgram (int)
{
}

const juce::String MS50ModularAudioProcessor::getProgramName (int)
{
    return {};
}

void MS50ModularAudioProcessor::changeProgramName (int, const juce::String&)
{
}

void MS50ModularAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement xml ("MS50");
    for (auto* parameter : getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            xml.setAttribute (ranged->getParameterID(), static_cast<double> (parameter->getValue()));
    }
    copyXmlToBinary (xml, destData);
}

void MS50ModularAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("MS50"))
        return;

    for (auto* parameter : getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
        {
            const juce::String id = ranged->getParameterID();
            if (xml->hasAttribute (id))
                parameter->setValue (static_cast<float> (xml->getDoubleAttribute (id)));
        }
    }
    applyHostControls();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MS50ModularAudioProcessor();
}
