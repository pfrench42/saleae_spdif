cmake_minimum_required (VERSION 3.13)

project(spdifAnalyzer)

add_definitions( -DLOGIC2 )

# enable generation of compile_commands.json, helpful for IDEs to locate include files.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# custom CMake Modules are located in the cmake directory.
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(ExternalAnalyzerSDK)

set(SOURCES 
source/spdifAnalyzer.cpp
source/spdifAnalyzer.h
source/spdifAnalyzerResults.cpp
source/spdifAnalyzerResults.h
source/spdifAnalyzerSettings.cpp
source/spdifAnalyzerSettings.h
source/spdifSimulationDataGenerator.cpp
source/spdifSimulationDataGenerator.h
)

add_analyzer_plugin(${PROJECT_NAME} SOURCES ${SOURCES})
