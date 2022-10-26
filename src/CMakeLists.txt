file(GLOB_RECURSE COMMON_SRC "${CMAKE_SOURCE_DIR}/src/common/*.cpp")
file(GLOB_RECURSE MUXER_SRC "${CMAKE_SOURCE_DIR}/src/muxer/*.cpp")
file(GLOB_RECURSE RECORD_SRC "${CMAKE_SOURCE_DIR}/src/record/*.cpp")
file(GLOB_RECURSE PHOTO_SRC "${CMAKE_SOURCE_DIR}/src/photo/*.cpp")
file(GLOB_RECURSE STREAM_SRC "${CMAKE_SOURCE_DIR}/src/stream/*.cpp")
file(GLOB_RECURSE PLAYER_SRC "${CMAKE_SOURCE_DIR}/src/player/*.cpp")
file(GLOB_RECURSE PARAM_SRC "${CMAKE_SOURCE_DIR}/src/param/*.cpp")
file(GLOB_RECURSE RTSP_SRC "${CMAKE_SOURCE_DIR}/src/live/rtsp/*.cpp")
file(GLOB_RECURSE OSD_SRC "${CMAKE_SOURCE_DIR}/src/osd/*.cpp")
#file(GLOB_RECURSE RTMP_SRC "${CMAKE_SOURCE_DIR}/src/live/rtmp/*.cpp")
#file(GLOB_RECURSE DISP_SRC "${CMAKE_SOURCE_DIR}/src/display/*.cpp")

if(${RKADK_CHIP} STREQUAL "rv1126")
	file(GLOB_RECURSE VI_SRC "${CMAKE_SOURCE_DIR}/src/vi/rkadk_aiq_uapi1.cpp")
else()
	file(GLOB_RECURSE VI_SRC "${CMAKE_SOURCE_DIR}/src/vi/rkadk_aiq_uapi2.cpp")
endif()

file(GLOB_RECURSE STORAGE_SRC "${CMAKE_SOURCE_DIR}/src/storage/*.cpp")
file(GLOB_RECURSE INIPARSER_SRC "${CMAKE_SOURCE_DIR}/src/third-party/iniparser/*.c")
file(GLOB_RECURSE CJSON_SRC "${CMAKE_SOURCE_DIR}/src/third-party/cjson/*.c")
add_definitions(-g -O0 -ggdb -gdwarf -funwind-tables -rdynamic -D_GNU_SOURCE)

link_directories(${CMAKE_SOURCE_DIR}/lib/arm-rockchip830-linux-gnueabihf)

add_library(rkadk SHARED
		${COMMON_SRC}
		${MUXER_SRC}
		${RECORD_SRC}
		${PHOTO_SRC}
		${STREAM_SRC}
		${PLAYER_SRC}
		${PARAM_SRC}
                ${RTMP_SRC}
		${RTSP_SRC}
		${OSD_SRC}
                ${DISP_SRC}
                ${VI_SRC}
		${STORAGE_SRC}
		${INIPARSER_SRC}
		${CJSON_SRC})

set_target_properties(rkadk PROPERTIES LINK_FLAGS -Wl,--version-script,${CMAKE_CURRENT_LIST_DIR}/export.map)
#set_target_properties(rkadk PROPERTIES COMPILE_OPTIONS -Wl,-Bsymbolic)
target_include_directories(rkadk PUBLIC "${CMAKE_SOURCE_DIR}/src/record")
target_include_directories(rkadk PUBLIC "${CMAKE_SOURCE_DIR}/src/third-party")
target_include_directories(rkadk PUBLIC "${CMAKE_SOURCE_DIR}/src/common")

if(CMAKE_C_COMPILER MATCHES "arm-rockchip830-linux-uclibcgnueabihf*")
	target_link_libraries(rkadk rockit rga rkmuxer iconv rtsp z rkaudio rkfsmk)
else()
	target_link_libraries(rkadk rockit rga rtsp rkfsmk rkmuxer z)
endif()

if(USE_RKAIQ)
    target_link_libraries(rkadk rkaiq)
endif()

# install target
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
