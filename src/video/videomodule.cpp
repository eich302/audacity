/*
* Audacity: A Digital Audio Editor
*/
#include "videomodule.h"

#include "framework/global/modularity/ioc.h"

#include <QQmlEngine>

#include "framework/ui/iuiengine.h"
#include "framework/rcommand/icommandsregister.h"
#include "framework/rcommand/icommandsstate.h"

#include "internal/videoservice.h"
#include "internal/videoexportcontroller.h"
#include "internal/videocommandsregister.h"
#include "internal/videocommandsstate.h"
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

void VideoModule::resolveImports()
{
    auto cr = globalIoc()->resolve<muse::rcommand::ICommandsRegister>(mname);
    if (cr) {
        cr->reg(std::make_shared<VideoCommandsRegister>());
    }
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

void VideoContext::resolveImports()
{
    auto cs = ioc()->resolve<muse::rcommand::ICommandsState>(mname);
    if (cs) {
        cs->reg(std::make_shared<VideoCommandsState>(iocContext()));
    }
}

void VideoContext::onInit(const muse::IApplication::RunMode& mode)
{
    if (mode != muse::IApplication::RunMode::GuiApp) {
        return;
    }

    m_service->init();

    m_exportController = std::make_shared<VideoExportController>(iocContext());
    m_exportController->init();

    auto uiEngine = ioc()->resolve<muse::ui::IUiEngine>(mname);
    if (uiEngine && uiEngine->qmlEngine()) {
        //! NOTE: the engine takes ownership of the provider
        uiEngine->qmlEngine()->addImageProvider("video", new VideoFrameProvider());
    }
}

void VideoContext::onDeinit()
{
    if (m_exportController) {
        m_exportController->deinit();
    }
    m_service->deinit();
}
