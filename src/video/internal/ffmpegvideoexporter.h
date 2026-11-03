/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace au::video {
enum class VideoCodec {
    H264,
    H265
};

struct ExportParams {
    std::string outputPath;
    std::string sourceVideoPath;
    // One entry per output frame: time in the source video, or nullopt for a black frame
    std::vector<std::optional<double> > frameSourceTimes;
    double fps = 25.0;
    // <= 0: use the (possibly downscaled) size of the source video. Rounded down to even numbers.
    int width = 0;
    int height = 0;
    std::string audioWavPath;
    VideoCodec videoCodec = VideoCodec::H264;
};

// Encodes a video file from a list of source frames plus a WAV file. No au3 dependencies.
class FfmpegVideoExporter
{
public:
    // progress: 0..1; returning false cancels the export (the partial output file is deleted).
    // Returns true on success. On failure (or cancel) lastError() holds a message (empty on cancel).
    bool exportVideo(const ExportParams& params, std::function<bool(double)> progress);

    const std::string& lastError() const { return m_error; }
    bool wasCanceled() const { return m_canceled; }
    // Name of the video encoder that was used (valid after a successful or failed-after-open export)
    const std::string& videoEncoderName() const { return m_videoEncoder; }

private:
    std::string m_error;
    std::string m_videoEncoder;
    bool m_canceled = false;
};
}
