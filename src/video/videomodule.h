/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include "modularity/imodulesetup.h"

namespace au::video {
class VideoModule : public muse::modularity::IModuleSetup
{
public:
    std::string moduleName() const override;
};
}
