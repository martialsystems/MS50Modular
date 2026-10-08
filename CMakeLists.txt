cmake_minimum_required(VERSION 3.22)
project(Ronin VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Ronin
  COMPANY_NAME "Martial Systems"
  BUNDLE_ID com.martialsystems.ronin
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  PLUGIN_MANUFACTURER_CODE Psnl
  PLUGIN_CODE Rnin
  FORMATS VST3
  PRODUCT_NAME "RONIN")

juce_add_binary_data(RoninPanelAssets
  HEADER_NAME PanelAssets.h
  NAMESPACE PanelAssets
  SOURCES panel/assets/panel_bg.svg)

target_sources(Ronin PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/Modular/Module.cpp
  Source/Modular/PatchGraph.cpp
  Source/Modular/ExtIn.cpp
  Source/Modular/OutputModule.cpp
  Source/Modular/Noise.cpp
  Source/Modular/Vcf.cpp
  Source/Modular/Vca1.cpp
  Source/Modular/Vca2.cpp
  Source/Modular/Eg1.cpp
  Source/Modular/Mg.cpp
  Source/Modular/Vco.cpp
  Source/Modular/Eg2.cpp
  Source/Modular/Ring.cpp
  Source/Modular/Divider.cpp
  Source/Modular/Inverter.cpp
  Source/Modular/Integrator.cpp
  Source/Modular/Mixer.cpp
  Source/Modular/SampleHold.cpp
  Source/UI/PatchBayLogic.cpp
  Source/UI/PatchBayView.cpp)

target_include_directories(Ronin PRIVATE Source)

target_compile_definitions(Ronin PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Ronin PRIVATE
  RoninPanelAssets
  juce::juce_audio_utils
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags)

add_executable(RoninTests
  Source/Tests/GraphTests.cpp
  Source/Tests/Step3Tests.cpp
  Source/Tests/SineThroughFxTests.cpp
  Source/Tests/OutputLevelTests.cpp
  Source/Tests/OutputMixTests.cpp
  Source/Tests/NoiseTests.cpp
  Source/Tests/PatchBayTests.cpp
  Source/Tests/VcfTests.cpp
  Source/Tests/VcaTests.cpp
  Source/Tests/Eg1Tests.cpp
  Source/Tests/ExtInTests.cpp
  Source/Tests/MgTests.cpp
  Source/Tests/VcoTests.cpp
  Source/Tests/Eg2Tests.cpp
  Source/Tests/RingTests.cpp
  Source/Tests/DividerTests.cpp
  Source/Tests/InverterTests.cpp
  Source/Tests/IntegratorTests.cpp
  Source/Tests/MixerTests.cpp
  Source/Tests/SampleHoldTests.cpp
  Source/Tests/MeterTests.cpp
  Source/Tests/FactoryPresetTests.cpp
  Source/Tests/PresetTests.cpp
  Source/Tests/FeedbackTests.cpp
  Source/Tests/EgRedesignTests.cpp
  Source/Tests/VcoRedesignTests.cpp
  Source/Modular/Module.cpp
  Source/Modular/PatchGraph.cpp
  Source/Modular/ExtIn.cpp
  Source/Modular/OutputModule.cpp
  Source/Modular/Noise.cpp
  Source/Modular/Vcf.cpp
  Source/Modular/Vca1.cpp
  Source/Modular/Vca2.cpp
  Source/Modular/Eg1.cpp
  Source/Modular/Mg.cpp
  Source/Modular/Vco.cpp
  Source/Modular/Eg2.cpp
  Source/Modular/Ring.cpp
  Source/Modular/Divider.cpp
  Source/Modular/Inverter.cpp
  Source/Modular/Integrator.cpp
  Source/Modular/Mixer.cpp
  Source/Modular/SampleHold.cpp
  Source/UI/PatchBayLogic.cpp)

juce_add_gui_app(RoninPanelProbe PRODUCT_NAME "RoninPanelProbe")

target_sources(RoninPanelProbe PRIVATE
  tools/PanelProbe.cpp
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/Modular/Module.cpp
  Source/Modular/PatchGraph.cpp
  Source/Modular/ExtIn.cpp
  Source/Modular/OutputModule.cpp
  Source/Modular/Noise.cpp
  Source/Modular/Vcf.cpp
  Source/Modular/Vca1.cpp
  Source/Modular/Vca2.cpp
  Source/Modular/Eg1.cpp
  Source/Modular/Mg.cpp
  Source/Modular/Vco.cpp
  Source/Modular/Eg2.cpp
  Source/Modular/Ring.cpp
  Source/Modular/Divider.cpp
  Source/Modular/Inverter.cpp
  Source/Modular/Integrator.cpp
  Source/Modular/Mixer.cpp
  Source/Modular/SampleHold.cpp
  Source/UI/PatchBayLogic.cpp
  Source/UI/PatchBayView.cpp)

target_include_directories(RoninPanelProbe PRIVATE Source)

target_compile_definitions(RoninPanelProbe PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0)

target_link_libraries(RoninPanelProbe PRIVATE
  RoninPanelAssets
  juce::juce_audio_utils
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags)

target_include_directories(RoninTests PRIVATE Source)

target_compile_features(RoninTests PRIVATE cxx_std_20)

target_compile_definitions(RoninTests PRIVATE
  RONIN_PATCH_GRAPH_SOURCE="${CMAKE_SOURCE_DIR}/Source/Modular/PatchGraph.cpp"
  RONIN_PROCESSOR_SOURCE="${CMAKE_SOURCE_DIR}/Source/PluginProcessor.cpp"
  RONIN_PATCH_BAY_VIEW_SOURCE="${CMAKE_SOURCE_DIR}/Source/UI/PatchBayView.cpp")

target_compile_options(RoninTests PRIVATE -Wall -Wextra -Wpedantic)
