cmake_minimum_required(VERSION 3.22)

# macOS : binaire universel (Apple Silicon + Intel), macOS 10.15 minimum (C++20 / std::filesystem), a fixer AVANT project()
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "" FORCE)
endif()

project(Peachy VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Windows : runtime C++ statique (pas de redistribuable Visual C++ a installer)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
# JUCE : dossier local si JUCE_DIR est fourni, sinon telechargement automatique
set(JUCE_DIR "" CACHE PATH "Copie locale de JUCE (optionnel)")
if(JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
  add_subdirectory(${JUCE_DIR} JUCE)
else()
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW ON)
  FetchContent_MakeAvailable(JUCE)
endif()

# Eigen (en-tetes seulement, utilise par NeuralAmpModelerCore) : dossier local si EIGEN_DIR est fourni
set(EIGEN_DIR "" CACHE PATH "Copie locale d'Eigen (optionnel)")
if(NOT (EIGEN_DIR AND EXISTS "${EIGEN_DIR}/Eigen/Dense"))
  FetchContent_Declare(eigen
    GIT_REPOSITORY https://github.com/eigen-mirror/eigen.git
    GIT_TAG 5.0.1
    GIT_SHALLOW ON
    SOURCE_SUBDIR do-not-build)
  FetchContent_MakeAvailable(eigen)
  set(EIGEN_DIR ${eigen_SOURCE_DIR})
endif()

# visuel, police et captures NAM integres au plugin (aucun fichier externe a l'execution)
juce_add_binary_data(PeachyAssets SOURCES
  background.jpg toggle_strip.png glow_l.png glow_r.png assets/fonts/SpecialElite-Regular.ttf
  models/fuzz_G50.nam models/fuzz_G65.nam models/fuzz_G75.nam models/fuzz_G90.nam models/fuzz_G100.nam)
set_target_properties(PeachyAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

juce_add_plugin(Peachy
  COMPANY_NAME "Studio Wiga"
  PLUGIN_MANUFACTURER_CODE Wiga
  PLUGIN_CODE WgPc
  BUNDLE_ID com.studiowiga.peachy
  FORMATS VST3
  VST3_CATEGORIES Fx Distortion
  PRODUCT_NAME "Peachy"
  VERSION 1.0.0
  DESCRIPTION "Preamp + fuzz (NAM captures) - double pedal format"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE)

file(GLOB NAM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/third_party/NAM/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/third_party/NAM/*/*.cpp)
target_sources(Peachy PRIVATE PluginProcessor.cpp PluginEditor.cpp PeachyDSP.cpp ${NAM_SOURCES})
target_include_directories(Peachy PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann
  ${EIGEN_DIR})

if(MSVC)
  target_compile_options(Peachy PRIVATE /utf-8 /bigobj /arch:AVX2)
endif()

target_compile_definitions(Peachy PUBLIC
  NAM_SAMPLE_FLOAT
  NOMINMAX
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Peachy
  PRIVATE
    PeachyAssets
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags)
