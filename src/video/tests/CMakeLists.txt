#
# Audacity: A Digital Audio Editor
#

set(MODULE_TEST video_tests)

set(MODULE_TEST_SRC
    ${CMAKE_CURRENT_LIST_DIR}/ffmpegvideodecoder_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/videoservice_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/videotimemap_tests.cpp
    )

set(MODULE_TEST_INCLUDE
    ${CMAKE_CURRENT_LIST_DIR}/..
    )

set(MODULE_TEST_LINK
    video
    au3wrap
    Qt::Gui
    )

set(MODULE_TEST_DATA_ROOT ${CMAKE_CURRENT_LIST_DIR})

include(SetupGTest)
