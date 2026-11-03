/*
* Audacity: A Digital Audio Editor
*/
#include "videoframeprovider.h"

#include "videoframestore.h"

using namespace au::video;

VideoFrameProvider::VideoFrameProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage VideoFrameProvider::requestImage(const QString&, QSize* size, const QSize& requestedSize)
{
    QImage image = VideoFrameStore::instance().frame();
    if (image.isNull()) {
        image = QImage(1, 1, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
    } else if (requestedSize.isValid() && !requestedSize.isEmpty()) {
        image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    if (size) {
        *size = image.size();
    }
    return image;
}
