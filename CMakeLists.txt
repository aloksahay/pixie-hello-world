# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# You can also add a build timestamp
#string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S")
#add_compile_definitions(BUILD_TIMESTAMP=\"${BUILD_TIMESTAMP}\")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hello-world)

