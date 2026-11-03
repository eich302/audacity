/*
* Audacity: A Digital Audio Editor
*/
#include "videoprojectinfo.h"

#include <filesystem>
#include <system_error>

#include "au3-project/Project.h"
#include "au3-project-file-io/ProjectFileIO.h"
#include "au3-xml/XMLAttributeValueView.h"
#include "au3-xml/XMLWriter.h"

using namespace au::video::projectinfo;

namespace fs = std::filesystem;

//! UTF-8 <-> filesystem::path helpers (u8path is deprecated and u8string() returns char8_t in C++20)
static fs::path toFsPath(const std::string& utf8)
{
    return fs::path(std::u8string(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
}

static std::string fromFsPath(const fs::path& path, bool generic = false)
{
    const std::u8string u8 = generic ? path.generic_u8string() : path.u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

static const AttachedProjectObjects::RegisteredFactory sKey
{
    [](AudacityProject&) {
        return std::make_shared<VideoProjectInfo>();
    }
};

VideoProjectInfo& VideoProjectInfo::Get(AudacityProject& project)
{
    return project.AttachedObjects::Get<VideoProjectInfo>(sKey);
}

const VideoProjectInfo& VideoProjectInfo::Get(const AudacityProject& project)
{
    return Get(const_cast<AudacityProject&>(project));
}

const std::string& VideoProjectInfo::path() const
{
    return m_path;
}

const std::string& VideoProjectInfo::relativePath() const
{
    return m_relativePath;
}

void VideoProjectInfo::setPath(const std::string& path)
{
    m_path = path;
    m_relativePath.clear();
}

void VideoProjectInfo::setStoredPaths(const std::string& path, const std::string& relativePath)
{
    m_path = path;
    m_relativePath = relativePath;
}

std::string VideoProjectInfo::resolvedPath(const std::string& projectFilePath) const
{
    std::error_code ec;
    if (!m_path.empty() && fs::exists(toFsPath(m_path), ec)) {
        return m_path;
    }

    if (!m_relativePath.empty() && !projectFilePath.empty()) {
        const fs::path abs = toFsPath(projectFilePath).parent_path() / toFsPath(m_relativePath);
        ec.clear();
        if (fs::exists(abs, ec)) {
            return fromFsPath(abs.lexically_normal());
        }
    }

    return m_path;
}

static ProjectFileIORegistry::AttributeWriterEntry entry {
    [](const AudacityProject& project, XMLWriter& xmlFile){
        const auto& info = VideoProjectInfo::Get(project);
        if (info.path().empty()) {
            return;
        }

        std::string rel = info.relativePath();
        const std::string projectFile = ProjectFileIO::Get(project).GetFileName().utf8_string();
        if (!projectFile.empty()) {
            std::error_code ec;
            const fs::path r = fs::relative(toFsPath(info.path()), toFsPath(projectFile).parent_path(), ec);
            if (!ec && !r.empty()) {
                rel = fromFsPath(r, true);
            }
        }

        xmlFile.WriteAttr(wxT("video_path"), wxString::FromUTF8(info.path().c_str()));
        if (!rel.empty()) {
            xmlFile.WriteAttr(wxT("video_path_rel"), wxString::FromUTF8(rel.c_str()));
        }
    }
};

static ProjectFileIORegistry::AttributeReaderEntries entries {
    (VideoProjectInfo & (*)(AudacityProject&)) & VideoProjectInfo::Get, {
        { "video_path", [](auto& info, auto value) {
                info.setStoredPaths(value.ToString(), info.relativePath());
            } },
        { "video_path_rel", [](auto& info, auto value) {
                info.setStoredPaths(info.path(), value.ToString());
            } }
    }
};
