cmake_minimum_required(VERSION 3.13)
project(DenoiseProject LANGUAGES CXX)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the oneAPI DPC++ compiler
find_package(IntelDPCPP REQUIRED)

# Add the source files
add_executable(denoise main.cpp)

# Link the oneAPI DPC++ libraries
target_link_libraries(denoise PRIVATE DPCPP::dpcpp)