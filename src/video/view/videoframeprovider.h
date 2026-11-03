/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <QQuickImageProvider>

namespace au::video {
//! Serves "image://video/<frameId>" with the latest frame from VideoFrameStore.
class VideoFrameProvider : public QQuickImageProvider
{
public:
    VideoFrameProvider();

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
};
}
