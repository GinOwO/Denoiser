cmake_minimum_required(VERSION 3.13)
project(DenoiseProject LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)

if(UNIX)
	# Direct CMake to use icpx rather than the default C++ compiler/linker
	set(CMAKE_CXX_COMPILER icpx)
else()
	# Force CMake to use icx-cl rather than the default C++ compiler/linker (needed on Windows only)
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

# Find required packages
find_package(embree 3.0 REQUIRED HINTS ${ONEAPI_ROOT}/embree/latest)
find_package(glfw3 3.3 REQUIRED)
find_package(glm REQUIRED)
find_package(OpenGL REQUIRED)
find_package(OpenMP REQUIRED)

find_path(TINYOBJLOADER_INCLUDE_DIR tiny_obj_loader.h)
if(TINYOBJLOADER_INCLUDE_DIR)
	include_directories(${TINYOBJLOADER_INCLUDE_DIR})
else()
	message(FATAL_ERROR "tiny_obj_loader.h not found!")
endif()


# Set the executable output directory
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin)

SET(WARNING_FLAGS "-Wall -Wextra -Wno-unused-parameter -Wno-unused-variable \
		 -Wno-unused-function -Wno-unused-but-set-variable \
		 -Wno-unused-value -Wno-unused-private-field \
		 -Wno-unused-const-variable -Wno-unused-const-variable")

SET(COMPILE_FLAGS "${WARNING_FLAGS} -mavx")
SET(LINK_FLAGS "${OpenMP_CXX_FLAGS} ${WIN_FLAG}")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMPILE_FLAGS}")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LINK_FLAGS}")

set(TARGET_NAME denoise)
set(SOURCES 
	${PROJECT_SOURCE_DIR}/src/lighting.cpp
	${PROJECT_SOURCE_DIR}/src/renderer.cpp
	${PROJECT_SOURCE_DIR}/src/main.cpp
)

add_executable(${TARGET_NAME} ${SOURCES})
target_link_libraries(${TARGET_NAME} PRIVATE 
	embree 
	glfw 
	OpenGL::GL 
	glm::glm 
	tinyobjloader
	OpenMP::OpenMP_CXX
)

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR})
endif()

install(TARGETS ${TARGET_NAME}
	RUNTIME DESTINATION bin/)
