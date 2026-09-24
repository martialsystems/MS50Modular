cmake_minimum_required(VERSION 3.22)
project(MS50Modular VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(MS50Modular
  COMPANY_NAME "Personal"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  PLUGIN_MANUFACTURER_CODE Psnl
  PLUGIN_CODE Ms50
  FORMATS VST3
  PRODUCT_NAME "MS-50 Modular")

target_sources(MS50Modular PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/Modular/Module.cpp
  Source/Modular/PatchGraph.cpp
  Source/Modular/ExtIn.cpp
  Source/Modular/OutputModule.cpp
  Source/UI/JackView.cpp
  Source/UI/RackView.cpp)

target_include_directories(MS50Modular PRIVATE Source)

target_compile_definitions(MS50Modular PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MS50Modular PRIVATE
  juce::juce_audio_utils
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags)

add_executable(MS50ModularTests
  Source/Tests/GraphTests.cpp
  Source/Tests/Step3Tests.cpp
  Source/Modular/Module.cpp
  Source/Modular/PatchGraph.cpp
  Source/Modular/ExtIn.cpp
  Source/Modular/OutputModule.cpp)

target_include_directories(MS50ModularTests PRIVATE Source)

target_compile_features(MS50ModularTests PRIVATE cxx_std_20)

target_compile_definitions(MS50ModularTests PRIVATE
  MS50_PATCH_GRAPH_SOURCE="${CMAKE_SOURCE_DIR}/Source/Modular/PatchGraph.cpp")

target_compile_options(MS50ModularTests PRIVATE -Wall -Wextra -Wpedantic)
