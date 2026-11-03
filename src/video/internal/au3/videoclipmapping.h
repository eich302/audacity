/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include "au3-wave-track/WaveClip.h"

#include "../videotimemap.h"

namespace au::video {
//! Per-clip attachment: maps the clip's sequence time (seconds = sequence sample / rate, unstretched)
//! to the time in the source video. Copied together with the clip (undo, split, copy/paste)
//! and updated when sequence samples are deleted or inserted.
struct VideoClipMapping final : WaveClipListener
{
    static VideoClipMapping& Get(WaveClip& clip);
    static const VideoClipMapping* Find(const WaveClip& clip);

    std::unique_ptr<WaveClipListener> Clone() const override;

    void MarkChanged() noexcept override;
    void Invalidate() override;

    void WriteXMLAttributes(XMLWriter& writer) const override;
    bool HandleXMLAttribute(const std::string_view& attr, const XMLAttributeValueView& valueView) override;

    void OnSequenceDeleted(double seqT0, double seqT1) override;
    void OnSequenceInserted(double seqT, double length, const WaveClip* source) override;

    VideoTimeMap map;
};
}
