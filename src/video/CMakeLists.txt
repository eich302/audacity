#
# Audacity: A Digital Audio Editor
#

declare_module(video)

set(MODULE_QRC video.qrc)
set(MODULE_QML_IMPORT ${CMAKE_CURRENT_LIST_DIR}/qml)

set(MODULE_SRC
    ${CMAKE_CURRENT_LIST_DIR}/videomodule.cpp
    ${CMAKE_CURRENT_LIST_DIR}/videomodule.h
    ${CMAKE_CURRENT_LIST_DIR}/ivideodecoder.h
    ${CMAKE_CURRENT_LIST_DIR}/ivideoservice.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/videoservice.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/videoservice.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au3/videoprojectinfo.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au3/videoprojectinfo.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/ffmpegvideodecoder.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/ffmpegvideodecoder.h

    ${CMAKE_CURRENT_LIST_DIR}/view/videoframestore.cpp
    ${CMAKE_CURRENT_LIST_DIR}/view/videoframestore.h
    ${CMAKE_CURRENT_LIST_DIR}/view/videoframeprovider.cpp
    ${CMAKE_CURRENT_LIST_DIR}/view/videoframeprovider.h
    ${CMAKE_CURRENT_LIST_DIR}/view/videopreviewmodel.cpp
    ${CMAKE_CURRENT_LIST_DIR}/view/videopreviewmodel.h
    )

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG_VIDEO REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale)

set(MODULE_LINK
    au3wrap
    au3-project
    au3-project-file-io
    au3-project-history
    au3-registries
    au3-xml
    Qt::Gui
    Qt::Quick
    Qt::Qml
    PkgConfig::FFMPEG_VIDEO
    )

setup_module()

if (AU_BUILD_VIDEO_TESTS)
    add_subdirectory(tests)
endif()
