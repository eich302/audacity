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
    ${CMAKE_CURRENT_LIST_DIR}/internal/videotimemap.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/videotimemap.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au3/videoclipmapping.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au3/videoclipmapping.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/ffmpegvideodecoder.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/ffmpegvideodecoder.h

    ${CMAKE_CURRENT_LIST_DIR}/view/videoframestore.cpp
    ${CMAKE_CURRENT_LIST_DIR}/view/videoframestore.h
    ${CMAKE_CURRENT_LIST_DIR}/view/videoframeprovider.cpp
    ${CMAKE_CURRENT_LIST_DIR}/view/videoframeprovider.h
    ${CMAKE_CURRENT_LIST_DIR}/view/videopreviewmodel.cpp
    ${CMAKE_CURRENT_LIST_DIR}/view/videopreviewmodel.h
    )

# FFmpeg: pkg-config first (Linux/macOS), otherwise find_path/find_library (e.g. Windows).
# On Windows set FFMPEG_ROOT to a prefix containing include/ and lib/ (e.g. a vcpkg or BtbN "shared" build).
set(FFMPEG_ROOT "" CACHE PATH "FFmpeg install prefix (used when pkg-config is unavailable)")

set(FFMPEG_VIDEO_LINK)
find_package(PkgConfig QUIET)
if (PkgConfig_FOUND)
    pkg_check_modules(FFMPEG_VIDEO QUIET IMPORTED_TARGET libavformat libavcodec libavutil libswscale)
endif()

if (TARGET PkgConfig::FFMPEG_VIDEO)
    set(FFMPEG_VIDEO_LINK PkgConfig::FFMPEG_VIDEO)
else()
    set(_ffmpeg_hints ${FFMPEG_ROOT} $ENV{FFMPEG_ROOT})

    find_path(FFMPEG_VIDEO_INCLUDE_DIR libavformat/avformat.h
        HINTS ${_ffmpeg_hints}
        PATH_SUFFIXES include
    )

    set(_ffmpeg_libs)
    foreach(_lib avformat avcodec avutil swscale)
        find_library(FFMPEG_VIDEO_${_lib}_LIBRARY
            NAMES ${_lib}
            HINTS ${_ffmpeg_hints}
            PATH_SUFFIXES lib lib64
        )
        list(APPEND _ffmpeg_libs ${FFMPEG_VIDEO_${_lib}_LIBRARY})
    endforeach()

    if (NOT FFMPEG_VIDEO_INCLUDE_DIR OR NOT FFMPEG_VIDEO_avformat_LIBRARY OR NOT FFMPEG_VIDEO_avcodec_LIBRARY
        OR NOT FFMPEG_VIDEO_avutil_LIBRARY OR NOT FFMPEG_VIDEO_swscale_LIBRARY)
        message(FATAL_ERROR "FFmpeg (libavformat, libavcodec, libavutil, libswscale) not found for the video module. "
                            "Install the development packages, or set FFMPEG_ROOT, or disable it with -DAU_BUILD_VIDEO_MODULE=OFF")
    endif()

    set(MODULE_INCLUDE_PRIVATE ${FFMPEG_VIDEO_INCLUDE_DIR})
    set(FFMPEG_VIDEO_LINK ${_ffmpeg_libs})

    # Windows: ship the FFmpeg runtime DLLs next to the executable (same dir as the app's install(TARGETS ... DESTINATION bin)).
    if (WIN32)
        set(_ffmpeg_bin_hints)
        foreach(_root ${_ffmpeg_hints})
            list(APPEND _ffmpeg_bin_hints ${_root}/bin)
        endforeach()
        get_filename_component(_ffmpeg_inc_prefix "${FFMPEG_VIDEO_INCLUDE_DIR}/.." ABSOLUTE)
        list(APPEND _ffmpeg_bin_hints ${_ffmpeg_inc_prefix}/bin)

        # DLL names carry the library's major version (e.g. avcodec-62.dll, swscale-9.dll), so glob for them
        set(_ffmpeg_dlls)
        foreach(_dll avformat avcodec avutil swscale swresample)
            set(_found)
            foreach(_bin ${_ffmpeg_bin_hints})
                file(GLOB _found "${_bin}/${_dll}.dll" "${_bin}/${_dll}-[0-9]*.dll")
                if (_found)
                    break()
                endif()
            endforeach()
            if (_found)
                list(APPEND _ffmpeg_dlls ${_found})
            else()
                message(WARNING "FFmpeg runtime DLL for ${_dll} not found in ${_ffmpeg_bin_hints}")
            endif()
        endforeach()

        if (_ffmpeg_dlls)
            install(FILES ${_ffmpeg_dlls} DESTINATION bin)
        endif()
    endif()
endif()

set(MODULE_LINK
    au3wrap
    au3-project
    au3-project-file-io
    au3-project-history
    au3-track
    au3-wave-track
    au3-registries
    au3-xml
    Qt::Gui
    Qt::Quick
    Qt::Qml
    ${FFMPEG_VIDEO_LINK}
    )

set(MODULE_USE_UNITY OFF)

setup_module()

if (AU_BUILD_VIDEO_TESTS AND MUSE_ENABLE_UNIT_TESTS)
    add_subdirectory(tests)
endif()
