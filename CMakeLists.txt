cmake_minimum_required(VERSION 3.28)

project(vst_host VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.0
)

FetchContent_MakeAvailable(juce)

juce_add_gui_app(vst_host
    PRODUCT_NAME "VST Host"
    COMPANY_NAME "gbruce"
    VERSION ${PROJECT_VERSION}
)

juce_generate_juce_header(vst_host)

target_sources(vst_host
    PRIVATE
        src/Main.cpp
        src/MainComponent.cpp
        src/MainComponent.h
)

target_compile_definitions(vst_host
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_PLUGINHOST_VST3=1
)

target_link_libraries(vst_host
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

