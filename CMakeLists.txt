message(STATUS "cmake version ${CMAKE_VERSION}")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)

project(rkadk)
include(version/Version.cmake)

install(DIRECTORY include/ DESTINATION include/rkadk FILES_MATCHING PATTERN "*.h")
include_directories(include)

if(${RKADK_CHIP} STREQUAL "rv1106")
        add_definitions(-DRV1106_1103)
else()
        add_definitions(-DTHUMB_NORMAL)
endif()

if(USE_RKAIQ)
    find_package(RkAiq REQUIRED)
    include_directories(${RKAIQ_INCLUDE_DIRS})
    add_definitions(-DRKAIQ)

    if(${RKADK_CHIP} STREQUAL "rv1106")
	add_definitions(-DRKAIQ_UAPI2)
    endif()
endif()

#-Wextra
add_definitions(-Wall
		-Wshadow
		-ffast-math
		-Wignored-qualifiers
		-Werror-implicit-function-declaration
		-fPIC
		-Wl,-Bsymbolic)

add_subdirectory(src)
add_subdirectory(examples)

#Install rkadk default configuration file
if(${RKADK_CHIP} STREQUAL "rv1106")
	set(RKADK_INICFG_PATH inicfg/rv1106_1103)
elseif(${RKADK_CHIP} STREQUAL "rv1126")
	set(RKADK_INICFG_PATH inicfg/rv1126_1109)
else()
	message(FATAL_ERROR "CMake Error, Select an invalid chip")
endif()

file(GLOB RKADK_INICFG_LIST ${RKADK_INICFG_PATH}/rkadk_defsetting*.ini)
install(FILES ${RKADK_INICFG_LIST} DESTINATION etc)
