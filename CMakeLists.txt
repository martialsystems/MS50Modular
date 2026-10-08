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

# Jidai Cable Standard v1.1 (vendored jidai-common, see third_party/jidai-common/VENDOR.md). Header-only.
set(JIDAI_COMMON_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/jidai-common)

# RONIN's own sources. The engine list is JUCE-free (DSP modules, patch graph, state format 2, patch-bay logic):
# it is what the JIDAI rack compiles. The plugin list adds the JUCE processor, editor and tab pages.
set(RONIN_ENGINE_SOURCES
  Source/Modular/Module.cpp
  Source/Modular/PatchGraph.cpp
  Source/Modular/PatchState.cpp
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

set(RONIN_PLUGIN_SOURCES
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/UI/TabPages.cpp
  Source/UI/PatchBayView.cpp)

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

target_sources(Ronin PRIVATE ${RONIN_PLUGIN_SOURCES} ${RONIN_ENGINE_SOURCES})

target_include_directories(Ronin PRIVATE Source)
target_link_libraries(Ronin PRIVATE jidai::common)

target_compile_definitions(Ronin PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Ronin PRIVATE
  RoninPanelAssets
  juce::juce_audio_utils
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags)

set(RONIN_UNIT_TEST_SOURCES
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
  Source/Tests/DspRedesignTests.cpp
  Source/Tests/GraphRedesignTests.cpp
  Source/Tests/JackIdTests.cpp)

add_executable(RoninTests ${RONIN_UNIT_TEST_SOURCES} ${RONIN_ENGINE_SOURCES})
juce_add_gui_app(RoninPanelProbe PRODUCT_NAME "RoninPanelProbe")

target_sources(RoninPanelProbe PRIVATE tools/PanelProbe.cpp ${RONIN_PLUGIN_SOURCES} ${RONIN_ENGINE_SOURCES})

target_include_directories(RoninPanelProbe PRIVATE Source)
target_link_libraries(RoninPanelProbe PRIVATE jidai::common)

target_compile_definitions(RoninPanelProbe PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0)

target_link_libraries(RoninPanelProbe PRIVATE
  RoninPanelAssets
  juce::juce_audio_utils
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags)

target_include_directories(RoninTests PRIVATE Source)
target_link_libraries(RoninTests PRIVATE jidai::common)

target_compile_features(RoninTests PRIVATE cxx_std_20)

target_compile_definitions(RoninTests PRIVATE
  RONIN_PATCH_GRAPH_SOURCE="${CMAKE_SOURCE_DIR}/Source/Modular/PatchGraph.cpp"
  RONIN_PROCESSOR_SOURCE="${CMAKE_SOURCE_DIR}/Source/PluginProcessor.cpp"
  RONIN_PATCH_BAY_VIEW_SOURCE="${CMAKE_SOURCE_DIR}/Source/UI/PatchBayView.cpp")

target_compile_options(RoninTests PRIVATE -Wall -Wextra -Wpedantic)

# Processor-level tests (JUCE): state v2, migration, TRI SHAPE by origin, HQ latency. Run ./build/RoninProcessorTests_artefacts/Release/RoninProcessorTests
juce_add_console_app(RoninProcessorTests PRODUCT_NAME "RoninProcessorTests")

target_sources(RoninProcessorTests PRIVATE Source/Tests/ProcessorTests.cpp ${RONIN_PLUGIN_SOURCES} ${RONIN_ENGINE_SOURCES})

target_include_directories(RoninProcessorTests PRIVATE Source)
target_link_libraries(RoninProcessorTests PRIVATE jidai::common)

target_compile_definitions(RoninProcessorTests PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0)

target_link_libraries(RoninProcessorTests PRIVATE
  RoninPanelAssets
  juce::juce_audio_utils
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags)

# RONIN_STRICT_WARNINGS: the JIDAI rack's warning set on RONIN's own sources, as errors. JUCE's recommended warning
# flags plus -Wfloat-equal -Wimplicit-int-float-conversion (clang only; g++ has no such flag) -Wshadow -Wconversion
# -Wdouble-promotion -Werror. JUCE, its generated headers and vendored jidai-common are SYSTEM includes, so only
# RONIN code is checked; JUCE's own module .cpp files keep their normal flags. Check:
#   cmake -B build-strict -DRONIN_STRICT_WARNINGS=ON && cmake --build build-strict
option(RONIN_STRICT_WARNINGS "Build RONIN sources with the JIDAI rack warning set as errors" OFF)

if(RONIN_STRICT_WARNINGS)
  if(CMAKE_VERSION VERSION_LESS 3.25)
    message(FATAL_ERROR "RONIN_STRICT_WARNINGS needs CMake 3.25+ (SYSTEM target property for JUCE headers)")
  endif()

  foreach(dep jidai_common RoninPanelAssets
      juce_core juce_events juce_data_structures juce_graphics juce_gui_basics juce_gui_extra
      juce_audio_basics juce_audio_devices juce_audio_formats juce_audio_processors juce_audio_utils
      juce_audio_plugin_client juce_audio_plugin_client_VST3)
    if(TARGET ${dep})
      get_target_property(ronin_dep_aliased ${dep} ALIASED_TARGET)
      if(ronin_dep_aliased)
        set_target_properties(${ronin_dep_aliased} PROPERTIES SYSTEM TRUE)
      else()
        set_target_properties(${dep} PROPERTIES SYSTEM TRUE)
      endif()
    endif()
  endforeach()

  # juce_add_plugin / gui_app / console_app put their generated JuceLibraryCode dir on the target itself.
  foreach(target Ronin RoninPanelProbe RoninProcessorTests)
    get_target_property(ronin_gen_dirs ${target} INCLUDE_DIRECTORIES)
    foreach(dir ${ronin_gen_dirs})
      if(dir MATCHES "JuceLibraryCode")
        target_include_directories(${target} SYSTEM PRIVATE ${dir})
      endif()
    endforeach()
  endforeach()

  target_link_libraries(RoninTests PRIVATE juce::juce_recommended_warning_flags)

  set(RONIN_RACK_WARNING_FLAGS
    -Wfloat-equal
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wimplicit-int-float-conversion>
    -Wshadow
    -Wconversion
    -Wdouble-promotion
    -Werror)

  set_source_files_properties(
      ${RONIN_ENGINE_SOURCES} ${RONIN_PLUGIN_SOURCES} ${RONIN_UNIT_TEST_SOURCES}
      Source/Tests/ProcessorTests.cpp tools/PanelProbe.cpp
    PROPERTIES COMPILE_OPTIONS "${RONIN_RACK_WARNING_FLAGS}")
endif()
