# Outil hors plugin : compile le processeur + la fenetre de PEACHY sous Linux et exporte des captures PNG (apercu, notice)
cmake_minimum_required(VERSION 3.22)
project(PeachyRender VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 20)
add_subdirectory(${JUCE_DIR} JUCE)
set(W ${CMAKE_CURRENT_SOURCE_DIR}/../..)
juce_add_binary_data(PeachyAssets SOURCES
  ${W}/background.jpg ${W}/toggle_strip.png ${W}/glow_l.png ${W}/glow_r.png ${W}/assets/fonts/SpecialElite-Regular.ttf
  ${W}/models/fuzz_G50.nam ${W}/models/fuzz_G65.nam ${W}/models/fuzz_G75.nam ${W}/models/fuzz_G90.nam ${W}/models/fuzz_G100.nam)
juce_add_console_app(PeachyRender PRODUCT_NAME "PeachyRender")
file(GLOB NAM_SOURCES ${W}/third_party/NAM/*.cpp ${W}/third_party/NAM/*/*.cpp)
target_sources(PeachyRender PRIVATE main.cpp ${W}/PluginProcessor.cpp ${W}/PluginEditor.cpp ${W}/PeachyDSP.cpp ${NAM_SOURCES})
target_include_directories(PeachyRender PRIVATE ${W} ${W}/third_party ${W}/third_party/nlohmann ${EIGEN_DIR})
target_compile_definitions(PeachyRender PRIVATE NAM_SAMPLE_FLOAT JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ALSA=0 JUCE_JACK=0
  JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JucePlugin_Name="Peachy" JucePlugin_Build_VST3=0
  JucePlugin_VersionString="1.0.0" JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(PeachyRender PRIVATE PeachyAssets juce::juce_audio_utils juce::juce_recommended_config_flags)
