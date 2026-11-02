/*
* Audacity: A Digital Audio Editor
*/
#include "videomodule.h"

using namespace au::video;

static const std::string mname("video");

std::string VideoModule::moduleName() const
{
    return mname;
}
