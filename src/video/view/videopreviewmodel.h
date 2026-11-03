/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

#include <QImage>
#include <QObject>
#include <QString>

#include "framework/global/async/asyncable.h"
#include "framework/global/modularity/ioc.h"
#include "framework/rcommand/icommanddispatcher.h"

#include "context/iglobalcontext.h"

#include "../ivideoservice.h"

namespace au::video {
class VideoPreviewModel : public QObject, public muse::async::Asyncable, public muse::Contextable
{
    Q_OBJECT

    Q_PROPERTY(bool hasVideo READ hasVideo NOTIFY hasVideoChanged FINAL)
    Q_PROPERTY(double currentTime READ currentTime WRITE setTime NOTIFY currentTimeChanged FINAL)
    Q_PROPERTY(int frameId READ frameId NOTIFY frameIdChanged FINAL)
    Q_PROPERTY(double fps READ fps NOTIFY videoInfoChanged FINAL)
    Q_PROPERTY(int frameIndex READ frameIndex NOTIFY currentTimeChanged FINAL)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY videoInfoChanged FINAL)
    Q_PROPERTY(QString timeText READ timeText NOTIFY timeTextChanged FINAL)

    muse::ContextInject<IVideoService> videoService{ this };
    muse::ContextInject<au::context::IGlobalContext> globalContext{ this };
    muse::ContextInject<muse::rcommand::ICommandDispatcher> commandDispatcher{ this };

public:
    explicit VideoPreviewModel(QObject* parent = nullptr);
    ~VideoPreviewModel() override;

    Q_INVOKABLE void load();
    Q_INVOKABLE void setTime(double secs);
    Q_INVOKABLE void stepFrame(int delta);

    bool hasVideo() const;
    double currentTime() const;
    int frameId() const;
    double fps() const;
    int frameIndex() const;
    int frameCount() const;
    QString timeText() const;

signals:
    void hasVideoChanged();
    void currentTimeChanged();
    void frameIdChanged();
    void videoInfoChanged();
    void timeTextChanged();

private:
    void onVideoChanged();
    void subscribeToPlayback();
    void requestFrame(int frameIndex);
    void workerLoop();
    void onFrameDecoded(int64_t requestId, const QImage& frame);
    int frameIndexFor(double secs) const;
    //! Source frame index for a project time, -1 if there is no video at that position
    int sourceFrameIndexAt(double projectSecs) const;

    bool m_hasVideo = false;
    double m_currentTime = 0.0;
    int m_frameId = 0;
    double m_fps = 0.0;
    int m_frameCount = 0;

    int m_requestedIndex = -2; // source frame index, -1: no video at the position, -2: nothing requested yet
    int m_sourceIndex = -1;

    struct Request {
        std::shared_ptr<IVideoService> service;
        double secs = 0.0;
        int64_t id = 0;
    };

    std::thread m_worker;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_quit = false;
    bool m_hasPending = false;
    Request m_pending;
    int64_t m_nextRequestId = 0;
};
}
