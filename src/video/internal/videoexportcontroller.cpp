/*
* Audacity: A Digital Audio Editor
*/
#include "videoexportcontroller.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <thread>

#include <QCoreApplication>
#include <QMetaObject>
#include <QStandardPaths>

#include "framework/global/defer.h"
#include "framework/global/log.h"
#include "framework/global/translation.h"
#include "framework/rcommand/actiontocommand.h"

#include "project/iaudacityproject.h"
#include "trackedit/itrackeditproject.h"

#include "au3-project/Project.h"
#include "au3-project-rate/ProjectRate.h"

#include "../videocommands.h"

using namespace au::video;
using namespace muse;

namespace {
constexpr int BTN_H264 = static_cast<int>(IInteractive::Button::CustomButton) + 1;
constexpr int BTN_H265 = static_cast<int>(IInteractive::Button::CustomButton) + 2;

std::string lowerExtension(const std::string& path)
{
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (!ext.empty() && ext[0] == '.') {
        ext.erase(0, 1);
    }
    return ext;
}

bool isSupportedVideoExtension(const std::string& ext)
{
    return ext == "mp4" || ext == "mkv" || ext == "webm" || ext == "mov" || ext == "avi";
}
}

void VideoExportController::init()
{
    commandDispatcher()->onRequest(this, VIDEO_EXPORT_COMMAND, [this]() { return exportVideo(); });

    static const std::vector<muse::rcommand::ActionToCommand> actionToCommand = {
        { VIDEO_EXPORT_ACTION, VIDEO_EXPORT_COMMAND, {} },
    };
    muse::rcommand::registerActionToCommand(this, actionToCommand, commandDispatcher(), dispatcher());
}

void VideoExportController::deinit()
{
    *m_alive = false;
}

muse::Ret VideoExportController::renderAudioMix(const std::string& wavPath)
{
    // Find the WAV export format by its extension instead of hardcoding its (translatable) name
    std::string wavFormat;
    for (const std::string& format : exporter()->formatsList()) {
        for (std::string ext : exporter()->formatExtensions(format)) {
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext == "wav") {
                wavFormat = format;
                break;
            }
        }
        if (!wavFormat.empty()) {
            break;
        }
    }
    if (wavFormat.empty()) {
        return muse::make_ret(Ret::Code::InternalError, muse::trc("video", "WAV export is not available"));
    }

    importexport::IExporter::Options options;
    options[importexport::IExporter::OptionKey::Format] = muse::Val(wavFormat);
    options[importexport::IExporter::OptionKey::ProcessType] = muse::Val(importexport::ExportProcessType::FULL_PROJECT_AUDIO);
    // ExportChannelsPref::ExportChannels::STEREO
    options[importexport::IExporter::OptionKey::ExportChannelsType] = muse::Val(2);

    //! NOTE: the export preferences may not contain a sample rate yet (0 = invalid for libsndfile),
    //! so pass the project rate explicitly like the cloud export does
    auto project = globalContext()->currentProject();
    ::AudacityProject* au3 = project ? reinterpret_cast< ::AudacityProject*>(project->au3ProjectPtr()) : nullptr;
    if (au3) {
        options[importexport::IExporter::OptionKey::ExportSampleRate]
            = muse::Val(static_cast<int>(ProjectRate::Get(*au3).GetRate()));
    }

    // The audio must start at project time 0 to stay in sync with the picture
    const bool trim = exportConfiguration()->trimBlankSpace();
    exportConfiguration()->setTrimBlankSpace(false);
    muse::Defer restore([this, trim]() { exportConfiguration()->setTrimBlankSpace(trim); });

    const muse::Ret ret = exporter()->exportData(muse::io::path_t(wavPath), options, nullptr, project);
    if (!ret) {
        LOGE() << "audio mix for video export failed: " << ret.toString();
    }
    return ret;
}

muse::Ret VideoExportController::exportVideo()
{
    if (m_busy) {
        return muse::make_ret(Ret::Code::InternalError);
    }

    auto project = globalContext()->currentProject();
    auto trackeditProject = globalContext()->currentTrackeditProject();
    if (!project || !trackeditProject || !videoService()->hasVideo()) {
        return muse::make_ret(Ret::Code::InternalError);
    }

    // 1. Where to save
    //! NOTE: default to the folder of the source video; fall back to the user's video folder
    muse::io::path_t defaultDir = muse::io::dirpath(videoService()->videoPath());
    if (defaultDir.empty() || !std::filesystem::is_directory(defaultDir.toStdString())) {
        defaultDir = muse::io::path_t(QStandardPaths::writableLocation(QStandardPaths::MoviesLocation));
    }
    muse::io::path_t defaultPath = defaultDir.appendingComponent(project->displayName() + ".mp4");

    const std::vector<std::string> filter = {
        muse::trc("video", "MP4 (*.mp4)"),
        muse::trc("video", "Matroska (*.mkv)"),
        muse::trc("video", "WebM (*.webm)"),
        muse::trc("video", "QuickTime (*.mov)"),
        muse::trc("video", "AVI (*.avi)"),
    };

    muse::io::path_t outPath = interactive()->selectSavingFileSync(muse::trc("video", "Export video"), defaultPath, filter);
    if (outPath.empty()) {
        return muse::make_ret(Ret::Code::Cancel);
    }

    std::string outputPath = outPath.toStdString();
    std::string ext = lowerExtension(outputPath);
    if (!isSupportedVideoExtension(ext)) {
        outputPath += ".mp4";
        ext = "mp4";
    }

    // 2. Codec
    VideoCodec codec = VideoCodec::H264;
    if (ext == "mp4" || ext == "mkv" || ext == "mov") {
        const IInteractive::ButtonDatas buttons = {
            interactive()->buttonData(IInteractive::Button::Cancel),
            IInteractive::ButtonData(BTN_H265, muse::trc("video", "H.265 / HEVC (smaller files)"), false, false,
                                     IInteractive::ButtonRole::ApplyRole),
            IInteractive::ButtonData(BTN_H264, muse::trc("video", "H.264 (most compatible)"), /*accent=*/ true, false,
                                     IInteractive::ButtonRole::ApplyRole),
        };
        const IInteractive::Result res = interactive()->questionSync(muse::trc("video", "Video codec"),
                                                                     IInteractive::Text(muse::trc("video", "Choose the video codec.")),
                                                                     buttons, BTN_H264);
        LOGI() << "video codec choice: " << res.button();
        if (res.isButton(BTN_H265)) {
            codec = VideoCodec::H265;
        } else if (!res.isButton(BTN_H264)) {
            return muse::make_ret(Ret::Code::Cancel);
        }
    }

    // 3. Frame list (main thread: sourceTimeAt is main-thread only)
    const double fps = videoService()->fps();
    const double duration = trackeditProject->totalTime().to_double();
    if (fps <= 0.0 || duration <= 0.0) {
        interactive()->error(muse::trc("video", "Export video"), muse::trc("video", "The project is empty."));
        return muse::make_ret(Ret::Code::InternalError);
    }

    ExportParams params;
    params.outputPath = outputPath;
    params.sourceVideoPath = videoService()->videoPath().toStdString();
    params.fps = fps;
    params.videoCodec = codec;

    const int64_t frameCount = static_cast<int64_t>(std::ceil(duration * fps));
    params.frameSourceTimes.reserve(static_cast<size_t>(frameCount));
    for (int64_t i = 0; i < frameCount; ++i) {
        params.frameSourceTimes.push_back(videoService()->sourceTimeAt(static_cast<double>(i) / fps));
    }

    // 4. Audio mix
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::string wavPath = (std::filesystem::temp_directory_path() / ("audacity_video_export_" + std::to_string(stamp) + ".wav")).string();

    const muse::Ret audioRet = renderAudioMix(wavPath);
    if (!audioRet) {
        std::error_code ec;
        std::filesystem::remove(wavPath, ec);
        interactive()->error(muse::trc("video", "Export video"),
                             audioRet.text().empty() ? muse::trc("video", "Could not render the audio") : audioRet.text());
        return audioRet;
    }
    params.audioWavPath = wavPath;

    // 5. Encode on a worker thread
    m_busy = true;

    auto progress = std::make_shared<muse::Progress>();
    auto job = std::make_shared<JobState>();
    progress->start();
    progress->canceled().onNotify(this, [job]() { job->canceled = true; });
    interactive()->showProgress(muse::trc("video", "Exporting video…"), *progress);

    std::weak_ptr<bool> alive = m_alive;
    auto post = [alive](std::function<void()> fn) {
        QMetaObject::invokeMethod(QCoreApplication::instance(), [alive, fn]() {
            if (!alive.expired() && *alive.lock()) {
                fn();
            }
        }, Qt::QueuedConnection);
    };

    std::thread([=, this]() {
        FfmpegVideoExporter videoExporter;
        int lastPercent = -1;
        const bool ok = videoExporter.exportVideo(params, [&](double p) {
            const int percent = static_cast<int>(p * 100.0);
            if (percent != lastPercent) {
                lastPercent = percent;
                post([progress, percent]() { progress->progress(percent, 100); });
            }
            return !job->canceled.load();
        });

        const bool canceled = videoExporter.wasCanceled() || job->canceled.load();
        const std::string error = videoExporter.lastError();
        post([=, this]() { finishJob(progress, params.outputPath, params.audioWavPath, ok, canceled, error); });
    }).detach();

    return muse::make_ok();
}

void VideoExportController::finishJob(const muse::ProgressPtr& progress, const std::string& outputPath, const std::string& wavPath,
                                      bool ok, bool canceled, const std::string& error)
{
    m_busy = false;

    std::error_code ec;
    std::filesystem::remove(wavPath, ec);

    if (canceled) {
        std::filesystem::remove(outputPath, ec);
        if (!progress->isCanceled()) {
            progress->cancel();
        }
        return;
    }

    if (ok) {
        progress->finish(muse::make_ok());
        interactive()->info(muse::trc("video", "Export video"),
                            muse::mtrc("video", "The video was exported to “%1”.").arg(muse::String::fromStdString(outputPath)).toStdString());
    } else {
        progress->finish(muse::make_ret(Ret::Code::InternalError, error));
        interactive()->error(muse::trc("video", "Export video"),
                             error.empty() ? muse::trc("video", "Export failed") : error);
    }
}
