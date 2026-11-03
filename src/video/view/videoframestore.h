/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <mutex>

#include <QImage>

namespace au::video {
//! Holds the latest decoded frame shared between VideoPreviewModel (writer)
//! and VideoFrameProvider (reader, may run on a QML thread).
class VideoFrameStore
{
public:
    static VideoFrameStore& instance();

    void setFrame(const QImage& image);
    QImage frame() const;

private:
    VideoFrameStore() = default;

    mutable std::mutex m_mutex;
    QImage m_frame;
};
}
