/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <cstdint>
#include <optional>

#include <QImage>

#include "framework/global/async/notification.h"
#include "framework/global/io/path.h"
#include "framework/global/modularity/imoduleinterface.h"

namespace au::video {
class IVideoService : MODULE_CONTEXT_INTERFACE
{
    INTERFACE_ID(IVideoService)

public:
    virtual ~IVideoService() = default;

    virtual bool loadVideo(const muse::io::path_t& path) = 0;
    virtual void unloadVideo() = 0;
    virtual bool hasVideo() const = 0;
    virtual muse::io::path_t videoPath() const = 0;
    virtual muse::async::Notification videoChanged() const = 0;

    virtual double fps() const = 0;
    virtual double duration() const = 0;
    virtual int64_t frameCount() const = 0;

    // Thread-safe. Returns a null image if no video is loaded or decoding fails.
    virtual QImage frameAt(double secs) = 0;

    // Loads the video and stores its path in the current project (marks it modified).
    // An empty path removes the video from the project.
    virtual void setProjectVideoPath(const muse::io::path_t& path) = 0;

    // Returns true if the file can be opened and contains a decodable video stream.
    // Does not change the loaded video.
    virtual bool probeHasVideo(const muse::io::path_t& path) const = 0;

    // Call before importing a media file into the current project...
    virtual void beginImport() = 0;
    // ...and after a successful import. If the file has video, it becomes the project video
    // and the audio clips created by the import get an identity video mapping.
    virtual void finishImport(const muse::io::path_t& path) = 0;

    // Main thread only. Maps a project time to the time in the source video file.
    // nullopt: there is no video at this position.
    // Projects without any mapped clip (created before mapping existed) use project time == video time.
    virtual std::optional<double> sourceTimeAt(double projectTime) const = 0;

    // Emitted when the mapping may have changed (edits, undo/redo, import)
    virtual muse::async::Notification mappingChanged() const = 0;
};
}
