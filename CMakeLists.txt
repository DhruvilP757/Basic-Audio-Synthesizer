cmake_minimum_required(VERSION 3.16)
project(DigitalAudioSynthesizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
# If using Qt 5, replace the above line with:
# find_package(Qt5 REQUIRED COMPONENTS Widgets)

add_executable(DigitalAudioSynthesizer
    main.cpp
    MainWindow.h
    MainWindow.cpp
    WaveformDisplay.h
    AudioEngine.h
)

target_link_libraries(DigitalAudioSynthesizer PRIVATE Qt6::Widgets winmm)
# If using Qt 5, replace with: target_link_libraries(DigitalAudioSynthesizer PRIVATE Qt5::Widgets winmm)