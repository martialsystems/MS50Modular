// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

// Load one CPU slice of a VST3, require an Fx audio class, and process one dry block.
// FL Studio's macOS bridge is x86_64, so an arm64-only bundle never reaches this point there.
// Compile this file twice, once with -arch arm64 and once with -arch x86_64.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <string>

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsthostapplication.h"

namespace {

bool sameId(const Steinberg::TUID left, const Steinberg::TUID right)
{
    return std::memcmp(left, right, sizeof(Steinberg::TUID)) == 0;
}

int fail(const char* step, int code)
{
    std::fprintf(stderr, "LOAD_FAIL %s code=%d\n", step, code);
    return 1;
}

int failText(const char* step)
{
    std::fprintf(stderr, "LOAD_FAIL %s\n", step);
    return 1;
}

bool subcategoryHas(const char* field, const char* token)
{
    const std::size_t tokenLength = std::strlen(token);
    const char* cursor = field;
    while (*cursor != '\0')
    {
        const char* bar = std::strchr(cursor, '|');
        const std::size_t length = bar == nullptr ? std::strlen(cursor)
                                                   : static_cast<std::size_t>(bar - cursor);
        if (length == tokenLength && std::memcmp(cursor, token, length) == 0)
            return true;
        if (bar == nullptr)
            break;
        cursor = bar + 1;
    }
    return false;
}

class Host final : public Steinberg::Vst::IHostApplication
{
public:
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** obj) override
    {
        if (sameId(iid, Steinberg::FUnknown_iid) || sameId(iid, Steinberg::Vst::IHostApplication_iid))
        {
            *obj = static_cast<Steinberg::Vst::IHostApplication*>(this);
            addRef();
            return Steinberg::kResultOk;
        }
        *obj = nullptr;
        return Steinberg::kNoInterface;
    }

    Steinberg::uint32 PLUGIN_API addRef() override
    {
        return ++references_;
    }

    Steinberg::uint32 PLUGIN_API release() override
    {
        return --references_;
    }

    Steinberg::tresult PLUGIN_API getName(Steinberg::Vst::String128 name) override
    {
        static constexpr char kName[] = "MS50Host";
        for (std::size_t i = 0; i < sizeof(kName); ++i)
            name[i] = static_cast<Steinberg::char16>(kName[i]);
        return Steinberg::kResultOk;
    }

    Steinberg::tresult PLUGIN_API createInstance(Steinberg::TUID, Steinberg::TUID, void** obj) override
    {
        *obj = nullptr;
        return Steinberg::kNotImplemented;
    }

private:
    Steinberg::uint32 references_ = 1;
};

std::string binaryInBundle(const std::string& bundlePath)
{
    const std::string suffix = ".vst3";
    std::string path = bundlePath;
    while (!path.empty() && path.back() == '/')
        path.pop_back();
    if (path.size() < suffix.size() || path.compare(path.size() - suffix.size(), suffix.size(), suffix) != 0)
        return path;

    const std::string::size_type slash = path.find_last_of('/');
    const std::string bundleName = path.substr(slash == std::string::npos ? 0 : slash + 1);
    const std::string product = bundleName.substr(0, bundleName.size() - suffix.size());
    return path + "/Contents/MacOS/" + product;
}

const char* archName()
{
#if defined(__x86_64__)
    return "x86_64";
#elif defined(__aarch64__)
    return "arm64";
#else
    return "unknown";
#endif
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
        return failText("usage: vst3_load_check <plugin.vst3>");

    const std::string binaryPath = binaryInBundle(argv[1]);
    void* module = dlopen(binaryPath.c_str(), RTLD_NOW);
    if (module == nullptr)
    {
        std::fprintf(stderr, "LOAD_FAIL dlopen %s\n", dlerror());
        return 1;
    }

    using GetFactory = Steinberg::IPluginFactory* (*)();
    GetFactory getFactory = nullptr;
    *reinterpret_cast<void**>(&getFactory) = dlsym(module, "GetPluginFactory");
    if (getFactory == nullptr)
        return failText("GetPluginFactory");

    Steinberg::IPluginFactory* factory = getFactory();
    if (factory == nullptr)
        return failText("null factory");

    Steinberg::PFactoryInfo factoryInfo{};
    if (factory->getFactoryInfo(&factoryInfo) != Steinberg::kResultOk)
        return failText("factory info");
    if (std::strcmp(factoryInfo.vendor, "Personal") != 0)
        return failText("vendor");

    Steinberg::IPluginFactory2* factory2 = nullptr;
    const Steinberg::tresult factory2Result =
        factory->queryInterface(Steinberg::IPluginFactory2_iid, reinterpret_cast<void**>(&factory2));
    if (factory2Result != Steinberg::kResultOk || factory2 == nullptr)
        return fail("factory2", static_cast<int>(factory2Result));

    Steinberg::PClassInfo2 audioClass{};
    bool foundAudioClass = false;
    const Steinberg::int32 classCount = factory2->countClasses();
    for (Steinberg::int32 index = 0; index < classCount; ++index)
    {
        Steinberg::PClassInfo2 info{};
        if (factory2->getClassInfo2(index, &info) != Steinberg::kResultOk)
            return failText("class info");
        if (std::strcmp(info.category, "Audio Module Class") != 0)
            continue;
        audioClass = info;
        foundAudioClass = true;
        break;
    }
    if (!foundAudioClass)
        return failText("audio class");
    if (std::strcmp(audioClass.name, "RONIN") != 0)
        return failText("plugin name");
    if (!subcategoryHas(audioClass.subCategories, "Fx"))
        return failText("subcategory Fx");
    if (subcategoryHas(audioClass.subCategories, "Instrument") || subcategoryHas(audioClass.subCategories, "Synth"))
        return failText("subcategory instrument");

    void* created = nullptr;
    const Steinberg::tresult createResult =
        factory->createInstance(audioClass.cid, Steinberg::Vst::IComponent_iid, &created);
    if (createResult != Steinberg::kResultOk || created == nullptr)
        return fail("createInstance", static_cast<int>(createResult));

    auto* component = static_cast<Steinberg::Vst::IComponent*>(created);
    Host host;
    const Steinberg::tresult initResult = component->initialize(static_cast<Steinberg::FUnknown*>(&host));
    if (initResult != Steinberg::kResultOk)
        return fail("initialize", static_cast<int>(initResult));

    if (component->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kInput) != 1
        || component->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput) != 1)
        return failText("bus count");

    Steinberg::Vst::IAudioProcessor* processor = nullptr;
    const Steinberg::tresult processorResult =
        component->queryInterface(Steinberg::Vst::IAudioProcessor_iid, reinterpret_cast<void**>(&processor));
    if (processorResult != Steinberg::kResultOk || processor == nullptr)
        return fail("audio processor", static_cast<int>(processorResult));

    Steinberg::Vst::SpeakerArrangement inputArrangement = Steinberg::Vst::SpeakerArr::kStereo;
    Steinberg::Vst::SpeakerArrangement outputArrangement = Steinberg::Vst::SpeakerArr::kStereo;
    const Steinberg::tresult arrangementResult = processor->setBusArrangements(&inputArrangement, 1, &outputArrangement, 1);
    if (arrangementResult != Steinberg::kResultOk)
        return fail("bus arrangement", static_cast<int>(arrangementResult));

    if (component->activateBus(Steinberg::Vst::kAudio, Steinberg::Vst::kInput, 0, 1) != Steinberg::kResultOk
        || component->activateBus(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput, 0, 1) != Steinberg::kResultOk)
        return failText("activateBus");

    Steinberg::Vst::ProcessSetup setup{};
    setup.processMode = Steinberg::Vst::kRealtime;
    setup.symbolicSampleSize = Steinberg::Vst::kSample32;
    setup.maxSamplesPerBlock = 64;
    setup.sampleRate = 48000.0;
    if (processor->setupProcessing(setup) != Steinberg::kResultOk)
        return failText("setupProcessing");
    if (component->setActive(1) != Steinberg::kResultOk)
        return failText("setActive");
    if (processor->getLatencySamples() != 0)
        return failText("latency");

    constexpr int kBlock = 64;
    float inputLeft[kBlock];
    float inputRight[kBlock];
    float outputLeft[kBlock];
    float outputRight[kBlock];
    std::memset(outputLeft, 0, sizeof(outputLeft));
    std::memset(outputRight, 0, sizeof(outputRight));
    for (int sample = 0; sample < kBlock; ++sample)
    {
        const float time = static_cast<float>(sample) / 48000.0f;
        inputLeft[sample] = 0.25f * std::sin(2.0f * 3.14159265f * 220.0f * time);
        inputRight[sample] = 0.25f * std::sin(2.0f * 3.14159265f * 220.0f * time + 1.5707963f);
    }

    float* inputChannels[2] = {inputLeft, inputRight};
    float* outputChannels[2] = {outputLeft, outputRight};
    Steinberg::Vst::AudioBusBuffers inputBus{};
    inputBus.numChannels = 2;
    inputBus.channelBuffers32 = inputChannels;
    Steinberg::Vst::AudioBusBuffers outputBus{};
    outputBus.numChannels = 2;
    outputBus.channelBuffers32 = outputChannels;

    Steinberg::Vst::ProcessData data{};
    data.processMode = Steinberg::Vst::kRealtime;
    data.symbolicSampleSize = Steinberg::Vst::kSample32;
    data.numSamples = kBlock;
    data.numInputs = 1;
    data.numOutputs = 1;
    data.inputs = &inputBus;
    data.outputs = &outputBus;

    if (processor->setProcessing(1) != Steinberg::kResultOk)
        return failText("setProcessing");
    const Steinberg::tresult processResult = processor->process(data);
    processor->setProcessing(0);
    component->setActive(0);
    if (processResult != Steinberg::kResultOk)
        return fail("process", static_cast<int>(processResult));

    double maxError = 0.0;
    for (int sample = 0; sample < kBlock; ++sample)
    {
        maxError = std::max(maxError, std::abs(static_cast<double>(outputLeft[sample]) - inputLeft[sample]));
        maxError = std::max(maxError, std::abs(static_cast<double>(outputRight[sample]) - inputRight[sample]));
    }
    if (!std::isfinite(maxError) || maxError >= 1.0e-5)
    {
        std::fprintf(stderr, "LOAD_FAIL dry block max_error=%.3e\n", maxError);
        return 1;
    }

    component->terminate();
    processor->release();
    component->release();
    factory2->release();

    std::printf("LOAD_OK %s Fx latency=0 max_error=%.3e\n", archName(), maxError);
    return 0;
}
