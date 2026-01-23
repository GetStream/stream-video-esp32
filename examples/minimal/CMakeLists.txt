# ESP-IDF Project CMakeLists.txt for minimal example
# This file allows building from the examples/minimal directory

cmake_minimum_required(VERSION 3.16)

# Set project name
set(PROJECT_NAME "minimal-example")

# Add components directory (relative to project root)
set(EXTRA_COMPONENT_DIRS "../../components")

# Include ESP-IDF build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Initialize project
project(${PROJECT_NAME})


