/*
* Audacity: A Digital Audio Editor
*/
#include "videoservice.h"

#include "framework/global/log.h"

#include "au3-project/Project.h"
#include "au3-project-history/ProjectHistory.h"

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
    if (!au3) {
        unloadVideo();
        return;
    }

    const std::string stored = projectinfo::VideoProjectInfo::Get(*au3).resolvedPath(project->path().toStdString());
    if (stored.empty()) {
        unloadVideo();
        return;
    }

    loadVideo(stored);
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
    return decoder.open(path.toStdString()) && decoder.isOpen();
}
