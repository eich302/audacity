/*
* Audacity: A Digital Audio Editor
*/
#include "videopreviewmodel.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "playback/playbackcommands.h"

#include "videoframestore.h"

using namespace au::video;

VideoPreviewModel::VideoPreviewModel(QObject* parent)
    : QObject(parent), muse::Contextable(muse::iocCtxForQmlObject(this))
{
}

VideoPreviewModel::~VideoPreviewModel()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_quit = true;
        m_hasPending = false;
        m_pending = {};
    }
    m_cv.notify_all();
    if (m_worker.joinable()) {
        m_worker.join();
    }
}

void VideoPreviewModel::load()
{
    if (!videoService()) {
        return;
    }

    videoService()->videoChanged().onNotify(this, [this]() {
        onVideoChanged();
    });

    if (globalContext()) {
        globalContext()->currentProjectChanged().onNotify(this, [this]() {
            subscribeToPlayback();
        });
    }

    onVideoChanged();
    subscribeToPlayback();
}

void VideoPreviewModel::subscribeToPlayback()
{
    if (!globalContext()) {
        return;
    }

    const au::context::IPlaybackStatePtr state = globalContext()->playbackState();
    if (!state) {
        return;
    }

    //! NOTE: the position channel also fires on seek while stopped (the player updates its position in seek())
    state->playbackPositionChanged().onReceive(this, [this](muse::secs_t secs) {
        setTime(secs.raw());
    }, muse::async::Asyncable::Mode::SetReplace);

    setTime(state->playbackPosition().raw());
}

void VideoPreviewModel::onVideoChanged()
{
    const bool has = videoService()->hasVideo();
    m_fps = has ? videoService()->fps() : 0.0;
    m_frameCount = has ? static_cast<int>(videoService()->frameCount()) : 0;
    emit videoInfoChanged();

    if (m_hasVideo != has) {
        m_hasVideo = has;
        emit hasVideoChanged();
    }

    // Force a refresh, the video itself may have changed
    m_requestedIndex = -1;
    requestFrame(frameIndexFor(m_currentTime));

    emit currentTimeChanged();
    emit timeTextChanged();
}

int VideoPreviewModel::frameIndexFor(double secs) const
{
    if (m_fps <= 0.0) {
        return 0;
    }

    int idx = static_cast<int>(std::floor(secs * m_fps + 1e-6));
    if (m_frameCount > 0) {
        idx = std::min(idx, m_frameCount - 1);
    }
    return std::max(idx, 0);
}

void VideoPreviewModel::setTime(double secs)
{
    secs = std::max(0.0, secs);
    const int idx = frameIndexFor(secs);
    const bool changed = idx != m_requestedIndex;

    m_currentTime = secs;

    if (!changed) {
        return;
    }

    requestFrame(idx);
    emit currentTimeChanged();
    emit timeTextChanged();
}

void VideoPreviewModel::stepFrame(int delta)
{
    if (!commandDispatcher() || delta == 0) {
        return;
    }

    const auto& command = delta > 0 ? playback::PLAYBACK_VIDEO_STEP_FORWARD_COMMAND
                          : playback::PLAYBACK_VIDEO_STEP_BACK_COMMAND;
    for (int i = 0; i < std::abs(delta); ++i) {
        commandDispatcher()->dispatch(command);
    }
}

void VideoPreviewModel::requestFrame(int frameIndex)
{
    m_requestedIndex = frameIndex;

    if (!m_hasVideo || !videoService() || m_fps <= 0.0) {
        // Nothing to decode: clear the shown frame
        VideoFrameStore::instance().setFrame(QImage());
        ++m_frameId;
        emit frameIdChanged();
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        //! NOTE: only the latest request is kept, stale ones are dropped
        m_pending.service = videoService();
        m_pending.secs = (frameIndex + 0.5) / m_fps; // middle of the frame, robust against rounding
        m_pending.id = ++m_nextRequestId;
        m_hasPending = true;
    }

    if (!m_worker.joinable()) {
        m_worker = std::thread([this]() { workerLoop(); });
    }
    m_cv.notify_one();
}

void VideoPreviewModel::workerLoop()
{
    while (true) {
        Request req;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]() { return m_quit || m_hasPending; });
            if (m_quit) {
                return;
            }
            req = std::move(m_pending);
            m_pending = {};
            m_hasPending = false;
        }

        QImage frame = req.service ? req.service->frameAt(req.secs) : QImage();

        QMetaObject::invokeMethod(this, [this, id = req.id, frame]() {
            onFrameDecoded(id, frame);
        }, Qt::QueuedConnection);
    }
}

void VideoPreviewModel::onFrameDecoded(int64_t, const QImage& frame)
{
    VideoFrameStore::instance().setFrame(frame);
    ++m_frameId;
    emit frameIdChanged();
}

bool VideoPreviewModel::hasVideo() const
{
    return m_hasVideo;
}

double VideoPreviewModel::currentTime() const
{
    return m_currentTime;
}

int VideoPreviewModel::frameId() const
{
    return m_frameId;
}

double VideoPreviewModel::fps() const
{
    return m_fps;
}

int VideoPreviewModel::frameIndex() const
{
    return frameIndexFor(m_currentTime);
}

int VideoPreviewModel::frameCount() const
{
    return m_frameCount;
}

QString VideoPreviewModel::timeText() const
{
    const int fpsInt = std::max(1, static_cast<int>(std::lround(m_fps)));
    const int idx = frameIndex();

    const int totalSecs = static_cast<int>(std::floor(m_currentTime));
    const int hh = totalSecs / 3600;
    const int mm = (totalSecs / 60) % 60;
    const int ss = totalSecs % 60;
    const int ff = idx % fpsInt;

    return QString("%1:%2:%3.%4  (frame %5)")
           .arg(hh, 2, 10, QChar('0'))
           .arg(mm, 2, 10, QChar('0'))
           .arg(ss, 2, 10, QChar('0'))
           .arg(ff, 2, 10, QChar('0'))
           .arg(idx);
}
