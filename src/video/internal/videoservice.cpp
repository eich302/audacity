/*
* Audacity: A Digital Audio Editor
*/
#include "videoservice.h"

#include <algorithm>

#include "framework/global/log.h"
#include "framework/global/translation.h"

#include "au3-project/Project.h"
#include "au3-project-history/ProjectHistory.h"
#include "au3-project-history/UndoManager.h"
#include "au3-track/Track.h"
#include "au3-wave-track/WaveTrack.h"

#include "au3/videoclipmapping.h"
#include "au3/videoprojectinfo.h"
#include "ffmpegvideodecoder.h"

using namespace au::video;

VideoService::VideoService(const muse::modularity::ContextPtr& ctx)
    : VideoService(ctx, std::make_unique<FfmpegVideoDecoder>())
{
}

VideoService::VideoService(const muse::modularity::ContextPtr& ctx, std::unique_ptr<IVideoDecoder> decoder)
    : muse::Contextable(ctx), m_decoder(std::move(decoder))
{
}

void VideoService::init()
{
    globalContext()->currentProjectChanged().onNotify(this, [this]() {
        onCurrentProjectChanged();
    });
    onCurrentProjectChanged();
}

void VideoService::deinit()
{
    unloadVideo();
}

bool VideoService::loadVideo(const muse::io::path_t& path)
{
    bool ok = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_decoder->close();
        m_path = muse::io::path_t();
        if (!path.empty() && m_decoder->open(path.toStdString())) {
            m_path = path;
            ok = true;
        }
    }
    if (!ok && !path.empty()) {
        LOGW() << "failed to open video: " << path;
    } else if (ok) {
        LOGI() << "video loaded: " << path;
    }
    m_videoChanged.notify();
    return ok;
}

void VideoService::unloadVideo()
{
    bool had = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        had = m_decoder->isOpen();
        m_decoder->close();
        m_path = muse::io::path_t();
    }
    if (had) {
        m_videoChanged.notify();
    }
}

bool VideoService::hasVideo() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_decoder->isOpen();
}

muse::io::path_t VideoService::videoPath() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_path;
}

muse::async::Notification VideoService::videoChanged() const
{
    return m_videoChanged;
}

double VideoService::fps() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_decoder->isOpen() ? m_decoder->fps() : 0.0;
}

double VideoService::duration() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_decoder->isOpen() ? m_decoder->duration() : 0.0;
}

int64_t VideoService::frameCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_decoder->isOpen() ? m_decoder->frameCount() : 0;
}

QImage VideoService::frameAt(double secs)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_decoder->isOpen()) {
        return QImage();
    }
    return m_decoder->frameAt(secs);
}

static ::AudacityProject* au3Project(const au::project::IAudacityProjectPtr& project)
{
    return project ? reinterpret_cast< ::AudacityProject*>(project->au3ProjectPtr()) : nullptr;
}

void VideoService::onCurrentProjectChanged()
{
    auto project = globalContext()->currentProject();
    ::AudacityProject* au3 = au3Project(project);
    subscribeToProjectHistory(au3);
    if (!au3) {
        unloadVideo();
        return;
    }

    const std::string stored = projectinfo::VideoProjectInfo::Get(*au3).resolvedPath(project->path().toStdString());
    if (stored.empty()) {
        unloadVideo();
        return;
    }

    //! NOTE: resolvedPath() already tried the stored absolute path and the one relative to the project file.
    //! If neither can be opened, tell the user (non-blocking) and leave the project without video.
    if (!loadVideo(stored)) {
        LOGW() << "video of the project is missing or can't be opened: " << stored;
        if (interactive()) {
            interactive()->warning(muse::trc("video", "Video file not found"),
                                   muse::trc("video", "The video file attached to this project could not be opened:")
                                   + "\n" + stored);
        }
    }
}

void VideoService::setProjectVideoPath(const muse::io::path_t& path)
{
    loadVideo(path);

    auto project = globalContext()->currentProject();
    ::AudacityProject* au3 = au3Project(project);
    if (!au3) {
        return;
    }

    projectinfo::VideoProjectInfo::Get(*au3).setPath(path.toStdString());
    ProjectHistory::Get(*au3).ModifyState(true);
}

bool VideoService::probeHasVideo(const muse::io::path_t& path) const
{
    FfmpegVideoDecoder decoder;
    const bool hasVideo = decoder.open(path.toStdString()) && decoder.isOpen();
    LOGI() << "probe " << path << ": has video = " << hasVideo;
    return hasVideo;
}

void VideoService::subscribeToProjectHistory(::AudacityProject* project)
{
    m_undoSubscription = {};
    if (!project) {
        return;
    }

    m_undoSubscription = UndoManager::Get(*project).Subscribe([this](const UndoRedoMessage& message) {
        switch (message.type) {
        case UndoRedoMessage::Pushed:
        case UndoRedoMessage::Modified:
        case UndoRedoMessage::UndoOrRedo:
        case UndoRedoMessage::Reset:
            m_mappingChanged.notify();
            break;
        default:
            break;
        }
    });
}

void VideoService::beginImport()
{
    m_importSnapshot.clear();

    ::AudacityProject* au3 = au3Project(globalContext()->currentProject());
    if (!au3) {
        return;
    }

    for (const Track* track : TrackList::Get(*au3)) {
        m_importSnapshot.insert(track->GetId().raw());
    }
}

void VideoService::finishImport(const muse::io::path_t& path)
{
    if (!probeHasVideo(path)) {
        return;
    }

    ::AudacityProject* au3 = au3Project(globalContext()->currentProject());
    if (!au3) {
        setProjectVideoPath(path);
        return;
    }

    for (WaveTrack* track : TrackList::Get(*au3).Any<WaveTrack>()) {
        if (m_importSnapshot.count(track->GetId().raw())) {
            continue;
        }
        for (const auto& clip : track->Intervals()) {
            const int rate = clip->GetRate();
            const size_t channels = std::max<size_t>(clip->NChannels(), 1);
            if (rate <= 0) {
                continue;
            }
            const double duration = clip->GetSequenceSamplesCount().as_double() / channels / rate;
            VideoClipMapping::Get(*clip).map = VideoTimeMap::identity(duration);
        }
    }

    m_importSnapshot.clear();

    //! NOTE: must come after the mappings are set: it updates the current undo state (ModifyState),
    //! so that the mappings are part of the undo snapshot created by the import
    setProjectVideoPath(path);
    m_mappingChanged.notify();
}

std::optional<double> VideoService::sourceTimeAt(double projectTime) const
{
    ::AudacityProject* au3 = au3Project(globalContext()->currentProject());
    if (!au3) {
        return projectTime;
    }

    bool anyMapped = false;
    std::optional<double> result;
    bool found = false;

    for (WaveTrack* track : TrackList::Get(*au3).Any<WaveTrack>()) {
        for (const auto& clip : track->Intervals()) {
            const VideoClipMapping* mapping = VideoClipMapping::Find(*clip);
            if (!mapping || mapping->map.empty()) {
                continue;
            }
            anyMapped = true;
            if (found) {
                continue;
            }
            if (projectTime < clip->GetPlayStartTime() || projectTime >= clip->GetPlayEndTime()) {
                continue;
            }
            const double stretch = clip->GetStretchRatio() > 0.0 ? clip->GetStretchRatio() : 1.0;
            const double seqT = std::max(0.0, (projectTime - clip->GetSequenceStartTime()) / stretch);
            const auto src = mapping->map.sourceTimeAt(seqT);
            if (src) {
                result = src;
                found = true;
            }
        }
    }

    if (!anyMapped) {
        return projectTime; // legacy project: video time == project time
    }
    return result;
}

muse::async::Notification VideoService::mappingChanged() const
{
    return m_mappingChanged;
}
