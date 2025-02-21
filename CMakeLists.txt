cmake_minimum_required(VERSION 3.13)
project(DenoiseProject LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

if(UNIX)
	# Direct CMake to use icpx rather than the default C++ compiler/linker
	set(CMAKE_CXX_COMPILER icpx)
else()
	# Force CMake to use icx-cl rather than the default C++ compiler/linker 
	# (needed on Windows only)
	include (CMakeForceCompiler)
	CMAKE_FORCE_CXX_COMPILER (icx-cl IntelDPCPP)
	include (Platform/Windows-Clang)
endif()

if(WIN32)
	set(WIN_FLAG "/EHsc")
endif()

set(ONEAPI_ROOT "")
if (NOT "$ENV{ONEAPI_ROOT}")
	set(ONEAPI_ROOT /opt/intel/oneapi)
else()
	set(ONEAPI_ROOT $ENV{ONEAPI_ROOT})
endif()

find_package(embree 3.0 REQUIRED HINTS ${ONEAPI_ROOT}/embree/latest)
# find_package(IntelSYCL REQUIRED)

# set(SYCL_COMPILE_FLAGS "-fsycl -Xclang -fsycl-allow-func-ptr -fsycl-targets=spir64 ${WIN_FLAG}")
# set(SYCL_LINK_FLAGS "")
# if(NOT MSVC)
# 	set(SYCL_LINK_FLAGS "-fsycl -fsycl-targets=spir64")
# endif(NOT MSVC)


GET_FILENAME_COMPONENT(SYCL_COMPILER_DIR ${CMAKE_CXX_COMPILER} PATH)
include_directories(SYSTEM "${SYCL_COMPILER_DIR}/../include/sycl" "${SYCL_COMPILER_DIR}/../include")

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin)

set(TARGET_NAME denoise)
set(SOURCES src/main.cpp)

ADD_EXECUTABLE(${TARGET_NAME} ${SOURCES})
TARGET_LINK_LIBRARIES(${TARGET_NAME} embree)
# set_property(TARGET ${TARGET_NAME} APPEND PROPERTY COMPILE_FLAGS ${SYCL_COMPILE_FLAGS})
# set_property(TARGET ${TARGET_NAME} APPEND PROPERTY LINK_FLAGS	${SYCL_LINK_FLAGS})

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR})
endif()

install(TARGETS ${TARGET_NAME}
		RUNTIME DESTINATION bin/)