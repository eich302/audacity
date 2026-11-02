/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <cstdint>
#include <string>

#include <QImage>
#include <QSize>

namespace au::video {
class IVideoDecoder
{
public:
    virtual ~IVideoDecoder() = default;

    virtual bool open(const std::string& path) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    virtual double fps() const = 0;
    virtual int64_t frameCount() const = 0;
    virtual double duration() const = 0;

    // Returns the frame shown at the given time (RGB32), or a null image on failure.
    virtual QImage frameAt(double secs) = 0;

    // Size of the images returned by frameAt (already downscaled).
    virtual QSize frameSize() const = 0;
};
}
