cmake_minimum_required(VERSION 3.16)

# Add components directory (relative to project root)
set(EXTRA_COMPONENT_DIRS "../../components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(minimal_video)
