// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

// Load one CPU slice of a VST3 and check it as a host would: vendor, an Fx audio class, the INIT state (Effect on:
// the first block is the wet patch, not the dry input), the wet path sounding, and a sample-exact dry block once the
// host switches Effect off.
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
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
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

constexpr const char* kVendor = "Martial Systems";   // COMPANY_NAME in CMakeLists.txt
constexpr int kBlock = 64;
constexpr double kRate = 48000.0;
constexpr double kNotDryMin = 1.0e-2;    // the INIT first block differs from the dry input by at least this
constexpr int kWetBlocks = 150;          // 200 ms at 64 / 48 kHz
constexpr double kWetPeakMin = 1.0e-3;   // the wet patch reaches the output within kWetBlocks
constexpr int kSettleBlocks = 150;       // 200 ms: 20 time constants of the 10 ms Output Mix smoother

bool titleIs(const Steinberg::Vst::String128 title, const char* text)
{
    std::size_t i = 0;
    for (; text[i] != '\0'; ++i)
        if (title[i] != static_cast<Steinberg::char16>(text[i]))
            return false;
    return title[i] == 0;
}

template <typename Interface>
class NoRefCount : public Interface
{
public:
    Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
    Steinberg::uint32 PLUGIN_API release() override { return 1; }
};

// One parameter, one point at sample 0.
class ValueQueue final : public NoRefCount<Steinberg::Vst::IParamValueQueue>
{
public:
    ValueQueue(Steinberg::Vst::ParamID id, double value) : id_(id), value_(value) {}
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** obj) override
    {
        if (sameId(iid, Steinberg::FUnknown_iid) || sameId(iid, Steinberg::Vst::IParamValueQueue_iid))
        {
            *obj = static_cast<Steinberg::Vst::IParamValueQueue*>(this);
            return Steinberg::kResultOk;
        }
        *obj = nullptr;
        return Steinberg::kNoInterface;
    }
    Steinberg::Vst::ParamID PLUGIN_API getParameterId() override { return id_; }
    Steinberg::int32 PLUGIN_API getPointCount() override { return 1; }
    Steinberg::tresult PLUGIN_API getPoint(Steinberg::int32 index, Steinberg::int32& sampleOffset,
                                           Steinberg::Vst::ParamValue& value) override
    {
        if (index != 0)
            return Steinberg::kResultFalse;
        sampleOffset = 0;
        value = value_;
        return Steinberg::kResultOk;
    }
    Steinberg::tresult PLUGIN_API addPoint(Steinberg::int32, Steinberg::Vst::ParamValue, Steinberg::int32&) override
    {
        return Steinberg::kResultFalse;
    }

private:
    Steinberg::Vst::ParamID id_;
    double value_;
};

class ParameterChanges final : public NoRefCount<Steinberg::Vst::IParameterChanges>
{
public:
    ParameterChanges(Steinberg::Vst::ParamID id, double value) : queue_(id, value) {}
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** obj) override
    {
        if (sameId(iid, Steinberg::FUnknown_iid) || sameId(iid, Steinberg::Vst::IParameterChanges_iid))
        {
            *obj = static_cast<Steinberg::Vst::IParameterChanges*>(this);
            return Steinberg::kResultOk;
        }
        *obj = nullptr;
        return Steinberg::kNoInterface;
    }
    Steinberg::int32 PLUGIN_API getParameterCount() override { return 1; }
    Steinberg::Vst::IParamValueQueue* PLUGIN_API getParameterData(Steinberg::int32 index) override
    {
        return index == 0 ? &queue_ : nullptr;
    }
    Steinberg::Vst::IParamValueQueue* PLUGIN_API addParameterData(const Steinberg::Vst::ParamID&, Steinberg::int32&) override
    {
        return nullptr;
    }

private:
    ValueQueue queue_;
};

struct BlockResult
{
    double errorVsDry = 0.0;   // max |out - in| over both channels
    double peak = 0.0;         // max |out|
    bool finite = true;
};

// Feeds a continuous 220 Hz stereo sine (0.25, quadrature) block after block.
class BlockRunner
{
public:
    explicit BlockRunner(Steinberg::Vst::IAudioProcessor* processor) : processor_(processor) {}

    bool block(Steinberg::Vst::IParameterChanges* changes, BlockResult& result)
    {
        float inputLeft[kBlock];
        float inputRight[kBlock];
        float outputLeft[kBlock] = {};
        float outputRight[kBlock] = {};
        for (int sample = 0; sample < kBlock; ++sample, ++position_)
        {
            const double phase = 2.0 * 3.14159265358979323846 * 220.0 * static_cast<double>(position_) / kRate;
            inputLeft[sample] = static_cast<float>(0.25 * std::sin(phase));
            inputRight[sample] = static_cast<float>(0.25 * std::sin(phase + 1.5707963267948966));
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
        data.inputParameterChanges = changes;
        if (processor_->process(data) != Steinberg::kResultOk)
            return false;
        result = {};
        for (int sample = 0; sample < kBlock; ++sample)
        {
            const double left = outputLeft[sample];
            const double right = outputRight[sample];
            result.finite = result.finite && std::isfinite(left) && std::isfinite(right);
            result.peak = std::max({result.peak, std::abs(left), std::abs(right)});
            result.errorVsDry = std::max({result.errorVsDry, std::abs(left - inputLeft[sample]),
                                          std::abs(right - inputRight[sample])});
        }
        return true;
    }

private:
    Steinberg::Vst::IAudioProcessor* processor_;
    long position_ = 0;
};

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
        static constexpr char kName[] = "RoninHost";
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
#if defined(__APPLE__)
    return path + "/Contents/MacOS/" + product;
#elif defined(__linux__)
    return path + "/Contents/" + std::string(archName()) + "-linux/" + product + ".so";
#else
    return path;
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
    if (std::strcmp(factoryInfo.vendor, kVendor) != 0)
    {
        std::fprintf(stderr, "LOAD_FAIL vendor \"%s\" (expected \"%s\")\n", factoryInfo.vendor, kVendor);
        return 1;
    }

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

    // The edit controller, connected to the component as a host does: it supplies the Effect parameter id.
    Steinberg::TUID controllerCid{};
    if (component->getControllerClassId(controllerCid) != Steinberg::kResultOk)
        return failText("controller class id");
    void* createdController = nullptr;
    const Steinberg::tresult controllerResult =
        factory->createInstance(controllerCid, Steinberg::Vst::IEditController_iid, &createdController);
    if (controllerResult != Steinberg::kResultOk || createdController == nullptr)
        return fail("controller createInstance", static_cast<int>(controllerResult));
    auto* controller = static_cast<Steinberg::Vst::IEditController*>(createdController);
    if (controller->initialize(static_cast<Steinberg::FUnknown*>(&host)) != Steinberg::kResultOk)
        return failText("controller initialize");
    Steinberg::Vst::IConnectionPoint* componentPoint = nullptr;
    Steinberg::Vst::IConnectionPoint* controllerPoint = nullptr;
    component->queryInterface(Steinberg::Vst::IConnectionPoint_iid, reinterpret_cast<void**>(&componentPoint));
    controller->queryInterface(Steinberg::Vst::IConnectionPoint_iid, reinterpret_cast<void**>(&controllerPoint));
    if (componentPoint == nullptr || controllerPoint == nullptr)
        return failText("connection points");
    componentPoint->connect(controllerPoint);
    controllerPoint->connect(componentPoint);

    Steinberg::Vst::ParamID effectId = 0;
    bool foundEffect = false;
    const Steinberg::int32 parameterCount = controller->getParameterCount();
    for (Steinberg::int32 index = 0; index < parameterCount && !foundEffect; ++index)
    {
        Steinberg::Vst::ParameterInfo info{};
        if (controller->getParameterInfo(index, info) != Steinberg::kResultOk)
            continue;
        if (titleIs(info.title, "Effect"))
        {
            effectId = info.id;
            foundEffect = true;
        }
    }
    if (!foundEffect)
        return failText("Effect parameter");
    // The controller's value cache follows the processor on JUCE's message-thread timer, which this host does not
    // run, so the INIT Effect state is checked from the audio: wet first (step 1), dry once switched off (step 3).

    Steinberg::Vst::ProcessSetup setup{};
    setup.processMode = Steinberg::Vst::kRealtime;
    setup.symbolicSampleSize = Steinberg::Vst::kSample32;
    setup.maxSamplesPerBlock = kBlock;
    setup.sampleRate = kRate;
    if (processor->setupProcessing(setup) != Steinberg::kResultOk)
        return failText("setupProcessing");
    if (component->setActive(1) != Steinberg::kResultOk)
        return failText("setActive");
    if (processor->getLatencySamples() != 0)
        return failText("latency");
    if (processor->setProcessing(1) != Steinberg::kResultOk)
        return failText("setProcessing");

    BlockRunner run(processor);

    // 1. First block in the INIT state: Effect on, so the output is the wet patch (EXT IN -> VCF -> VCA 1, the
    //    VCA opened by EG 1 from the EXT IN gate), not the dry input. It must be finite, bounded, and not dry.
    BlockResult first{};
    if (!run.block(nullptr, first))
        return failText("process (first block)");
    if (!first.finite || first.peak > 1.0)
    {
        std::fprintf(stderr, "LOAD_FAIL first block not finite or above 1.0 (peak %.3e)\n", first.peak);
        return 1;
    }
    if (first.errorVsDry < kNotDryMin)
    {
        std::fprintf(stderr, "LOAD_FAIL first block is dry (max |out-in| %.3e) but INIT is Effect on\n", first.errorVsDry);
        return 1;
    }

    // 2. The wet path sounds: within kWetBlocks blocks (~200 ms) the EXT IN gate has opened EG 1 and the VCA,
    //    so filtered signal reaches the output. Every block stays finite and bounded.
    double wetPeak = first.peak;
    for (int index = 1; index < kWetBlocks; ++index)
    {
        BlockResult wet{};
        if (!run.block(nullptr, wet))
            return failText("process (wet block)");
        if (!wet.finite || wet.peak > 1.0)
        {
            std::fprintf(stderr, "LOAD_FAIL wet block %d not finite or above 1.0 (peak %.3e)\n", index, wet.peak);
            return 1;
        }
        wetPeak = std::max(wetPeak, wet.peak);
    }
    if (wetPeak < kWetPeakMin)
    {
        std::fprintf(stderr, "LOAD_FAIL the INIT wet path stays silent (peak %.3e)\n", wetPeak);
        return 1;
    }

    // 3. Effect off through a host parameter change: once the 10 ms Output Mix smoother has settled the block is
    //    the dry input, sample for sample (the original dry-block check, now on the state where it applies).
    ParameterChanges effectOff(effectId, 0.0);
    BlockResult switching{};
    if (!run.block(&effectOff, switching))
        return failText("process (effect off)");
    for (int index = 0; index < kSettleBlocks; ++index)
    {
        BlockResult settle{};
        if (!run.block(nullptr, settle))
            return failText("process (settle)");
    }
    BlockResult dry{};
    if (!run.block(nullptr, dry))
        return failText("process (dry block)");
    processor->setProcessing(0);
    component->setActive(0);
    if (!dry.finite || dry.errorVsDry >= 1.0e-5)
    {
        std::fprintf(stderr, "LOAD_FAIL dry block after Effect off max_error=%.3e\n", dry.errorVsDry);
        return 1;
    }
    const double maxError = dry.errorVsDry;

    controllerPoint->disconnect(componentPoint);
    componentPoint->disconnect(controllerPoint);
    controllerPoint->release();
    componentPoint->release();
    controller->terminate();
    controller->release();
    component->terminate();
    processor->release();
    component->release();
    factory2->release();

    std::printf("LOAD_OK %s Fx vendor=\"%s\" latency=0 init_effect=on first_block_wet max|out-in|=%.3e "
                "wet_peak=%.3e dry_after_effect_off max_error=%.3e\n",
                archName(), kVendor, first.errorVsDry, wetPeak, maxError);
    return 0;
}
