/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <string>

#include "au3-registries/ClientData.h"
#include "au3-project/Project.h"

namespace au::video::projectinfo {
//! Per-project attached object, persisted as attributes of the project XML:
//! `video_path` (as given) and `video_path_rel` (relative to the project file dir, if possible).
class VideoProjectInfo : public ClientData::Base
{
public:
    static VideoProjectInfo& Get(AudacityProject& project);
    static const VideoProjectInfo& Get(const AudacityProject& project);

    const std::string& path() const;
    const std::string& relativePath() const;

    void setPath(const std::string& path);
    void setStoredPaths(const std::string& path, const std::string& relativePath);

    //! Absolute path if it exists on disk, otherwise the relative one
    //! resolved against the directory of projectFilePath. Empty if nothing is stored.
    std::string resolvedPath(const std::string& projectFilePath) const;

private:
    std::string m_path;
    std::string m_relativePath;
};
}
