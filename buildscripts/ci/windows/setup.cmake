
message(STATUS "Setup Windows build environment")

# Config
set(BUILD_TOOLS "c:/build_tools")
set(ENV_FILE ${BUILD_TOOLS}/environment.sh)

# Options
set(BUILD_WIN_PORTABLE "" CACHE STRING "Build number")
option(BUILD_WIN_PORTABLE "Build app portable" OFF)


# Install tools
execute_process(COMMAND choco install -y git.install)
execute_process(COMMAND choco install -y wget)
execute_process(COMMAND choco install -y 7zip.install)

# Set dir
file(REMOVE ${ENV_FILE})

set(TEMP_DIR "${BUILD_TOOLS}/temp")
file(MAKE_DIRECTORY ${TEMP_DIR})

file(WRITE ${ENV_FILE} "")

# FFmpeg (shared LGPL build with MSVC import libs) for the video module.
# BtbN builds: https://github.com/BtbN/FFmpeg-Builds/releases
set(FFMPEG_ARCH "" CACHE STRING "FFmpeg build arch: win64 or winarm64 (default: detected)")
if (NOT FFMPEG_ARCH)
    if ("$ENV{PROCESSOR_ARCHITECTURE}" STREQUAL "ARM64" OR "$ENV{PROCESSOR_ARCHITEW6432}" STREQUAL "ARM64")
        set(FFMPEG_ARCH "winarm64")
    else()
        set(FFMPEG_ARCH "win64")
    endif()
endif()

set(FFMPEG_NAME "ffmpeg-n7.1-latest-${FFMPEG_ARCH}-lgpl-shared-7.1")
set(FFMPEG_URL "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/${FFMPEG_NAME}.zip")
set(FFMPEG_ARCHIVE "${TEMP_DIR}/${FFMPEG_NAME}.zip")
set(FFMPEG_EXTRACT_DIR "${BUILD_TOOLS}/ffmpeg_extract")
set(FFMPEG_DIR "${BUILD_TOOLS}/ffmpeg")

message(STATUS "Download FFmpeg: ${FFMPEG_URL}")
file(DOWNLOAD "${FFMPEG_URL}" "${FFMPEG_ARCHIVE}" STATUS FFMPEG_DL_STATUS SHOW_PROGRESS)
list(GET FFMPEG_DL_STATUS 0 FFMPEG_DL_CODE)

set(FFMPEG_OK OFF)
if (FFMPEG_DL_CODE EQUAL 0)
    file(REMOVE_RECURSE "${FFMPEG_EXTRACT_DIR}" "${FFMPEG_DIR}")
    file(MAKE_DIRECTORY "${FFMPEG_EXTRACT_DIR}")
    file(ARCHIVE_EXTRACT INPUT "${FFMPEG_ARCHIVE}" DESTINATION "${FFMPEG_EXTRACT_DIR}")
    # The zip has a single top-level folder
    if (EXISTS "${FFMPEG_EXTRACT_DIR}/${FFMPEG_NAME}/include")
        file(RENAME "${FFMPEG_EXTRACT_DIR}/${FFMPEG_NAME}" "${FFMPEG_DIR}")
        set(FFMPEG_OK ON)
    endif()
endif()

if (FFMPEG_OK)
    file(APPEND ${ENV_FILE} "export FFMPEG_ROOT=${FFMPEG_DIR}\n")
    message(STATUS "FFmpeg ready: ${FFMPEG_DIR}")
elseif (FFMPEG_ARCH STREQUAL "winarm64")
    # No prebuilt arm64 FFmpeg available: build without the video module
    message(WARNING "FFmpeg for ${FFMPEG_ARCH} not available (${FFMPEG_DL_STATUS}); disabling the video module")
    file(APPEND ${ENV_FILE} "export AU_BUILD_VIDEO_MODULE=OFF\n")
else()
    message(FATAL_ERROR "Failed to download/extract FFmpeg: ${FFMPEG_DL_STATUS}")
endif()

message(STATUS "Setup script done")
