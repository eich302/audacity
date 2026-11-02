#
# Audacity: A Digital Audio Editor
#

declare_module(video)

set(MODULE_SRC
    ${CMAKE_CURRENT_LIST_DIR}/videomodule.cpp
    ${CMAKE_CURRENT_LIST_DIR}/videomodule.h
    ${CMAKE_CURRENT_LIST_DIR}/ivideodecoder.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/ffmpegvideodecoder.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/ffmpegvideodecoder.h
    )

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG_VIDEO REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale)

set(MODULE_LINK
    Qt::Gui
    PkgConfig::FFMPEG_VIDEO
    )

setup_module()

if (AU_BUILD_VIDEO_TESTS)
    add_subdirectory(tests)
endif()
