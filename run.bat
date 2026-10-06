@echo off
echo ====================================================
echo Starting Digital Audio Synthesizer...
echo ====================================================

set PATH=C:\Qt\6.11.2\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;%PATH%

if not exist "%~dp0build\Desktop_Qt_6_11_2_MinGW_64_bit_Debug\DigitalAudioSynthesizer.exe" (
    echo Executable not found. Configuring and building project now...
    cmake -B "%~dp0build\Desktop_Qt_6_11_2_MinGW_64_bit_Debug" -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="C:\Qt\6.11.2\mingw_64" "%~dp0"
    cmake --build "%~dp0build\Desktop_Qt_6_11_2_MinGW_64_bit_Debug"
)

start "" "%~dp0build\Desktop_Qt_6_11_2_MinGW_64_bit_Debug\DigitalAudioSynthesizer.exe"
