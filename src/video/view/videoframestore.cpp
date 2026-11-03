/*
* Audacity: A Digital Audio Editor
*/
#include "videoframestore.h"

using namespace au::video;

VideoFrameStore& VideoFrameStore::instance()
{
    static VideoFrameStore store;
    return store;
}

void VideoFrameStore::setFrame(const QImage& image)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_frame = image;
}

QImage VideoFrameStore::frame() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_frame;
}
