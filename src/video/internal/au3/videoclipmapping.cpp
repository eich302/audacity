/*
* Audacity: A Digital Audio Editor
*/
#include "videoclipmapping.h"

#include "au3-xml/XMLAttributeValueView.h"
#include "au3-xml/XMLWriter.h"

using namespace au::video;

static constexpr auto VideoMap_attr = "videomap";

static WaveClip::Attachments::RegisteredFactory sKey{ [](WaveClip&) {
        return std::make_unique<VideoClipMapping>();
    } };

VideoClipMapping& VideoClipMapping::Get(WaveClip& clip)
{
    return clip.Attachments::Get<VideoClipMapping>(sKey);
}

const VideoClipMapping* VideoClipMapping::Find(const WaveClip& clip)
{
    return const_cast<WaveClip&>(clip).Attachments::Find<VideoClipMapping>(sKey);
}

std::unique_ptr<WaveClipListener> VideoClipMapping::Clone() const
{
    return std::make_unique<VideoClipMapping>(*this);
}

void VideoClipMapping::MarkChanged() noexcept
{
}

void VideoClipMapping::Invalidate()
{
}

void VideoClipMapping::WriteXMLAttributes(XMLWriter& writer) const
{
    if (map.empty()) {
        return;
    }
    writer.WriteAttr(VideoMap_attr, wxString::FromUTF8(map.serialize().c_str()));
}

bool VideoClipMapping::HandleXMLAttribute(const std::string_view& attr, const XMLAttributeValueView& valueView)
{
    if (attr == VideoMap_attr) {
        map = VideoTimeMap::parse(valueView.ToString());
        return true;
    }
    return false;
}

void VideoClipMapping::OnSequenceDeleted(double seqT0, double seqT1)
{
    map.erase(seqT0, seqT1);
}

void VideoClipMapping::OnSequenceInserted(double seqT, double length, const WaveClip* source)
{
    const VideoClipMapping* srcMapping = source ? Find(*source) : nullptr;
    if (map.empty() && (!srcMapping || srcMapping->map.empty())) {
        return; // nothing to track
    }
    map.insert(seqT, length, srcMapping ? &srcMapping->map : nullptr);
}
