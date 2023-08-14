# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# Add RainMaker and rotary encoder components
set(EXTRA_COMPONENT_DIRS $ENV{RUNMAKER_PATH}/components/esp-insights/components $ENV{RUNMAKER_PATH}/components $ENV{RUNMAKER_PATH}/examples/common)

set(PROJECT_VER "1.0")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(fan)
