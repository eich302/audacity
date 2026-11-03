/*
* Audacity: A Digital Audio Editor
*/
#include "videomodule.h"

#include "framework/global/modularity/ioc.h"

#include <QQmlEngine>

#include "framework/ui/iuiengine.h"

#include "internal/videoservice.h"
#include "view/videoframeprovider.h"
#include "view/videopreviewmodel.h"

static void video_init_qrc()
{
    Q_INIT_RESOURCE(video);
}

using namespace au::video;

static const std::string mname("video");

std::string VideoModule::moduleName() const
{
    return mname;
}

void VideoModule::registerResources()
{
    video_init_qrc();
}

void VideoModule::registerUiTypes()
{
    qmlRegisterType<VideoPreviewModel>("Audacity.Video", 1, 0, "VideoPreviewModel");
}

muse::modularity::IContextSetup* VideoModule::newContext(const muse::modularity::ContextPtr& ctx) const
{
    return new VideoContext(ctx);
}

void VideoContext::registerExports()
{
    m_service = std::make_shared<VideoService>(iocContext());

    ioc()->registerExport<IVideoService>(mname, m_service);
}

void VideoContext::onInit(const muse::IApplication::RunMode& mode)
{
    if (mode != muse::IApplication::RunMode::GuiApp) {
        return;
    }

    m_service->init();

    auto uiEngine = ioc()->resolve<muse::ui::IUiEngine>(mname);
    if (uiEngine && uiEngine->qmlEngine()) {
        //! NOTE: the engine takes ownership of the provider
        uiEngine->qmlEngine()->addImageProvider("video", new VideoFrameProvider());
    }
}

void VideoContext::onDeinit()
{
    m_service->deinit();
}
