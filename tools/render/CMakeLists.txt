# Outil hors plugin : compile le processeur + la fenêtre de Wharry sous Linux et exporte des captures PNG (aperçu)
cmake_minimum_required(VERSION 3.22)
project(WharryRender VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
add_subdirectory(${JUCE_DIR} JUCE)
set(W ${CMAKE_CURRENT_SOURCE_DIR}/../..)
juce_add_binary_data(WharryAssets SOURCES
  ${W}/background.png ${W}/treadle_up.png ${W}/treadle_down.png
  ${W}/cap_big.png ${W}/cap_fsL.png ${W}/cap_fsR.png
  ${W}/led_r_on.png ${W}/led_r_off.png ${W}/led_p_on.png ${W}/led_p_off.png ${W}/led_s_on.png ${W}/led_s_off.png)
juce_add_console_app(WharryRender PRODUCT_NAME "WharryRender")
target_sources(WharryRender PRIVATE main.cpp ${W}/PluginProcessor.cpp ${W}/PluginEditor.cpp)
target_include_directories(WharryRender PRIVATE ${W})
target_compile_definitions(WharryRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ALSA=0 JUCE_JACK=0
  JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JucePlugin_Name="Wharry" JucePlugin_Build_VST3=0
  JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(WharryRender PRIVATE WharryAssets juce::juce_audio_utils juce::juce_recommended_config_flags)
