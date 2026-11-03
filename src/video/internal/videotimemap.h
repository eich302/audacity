/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace au::video {
//! Maps "sequence time" of an audio clip (seconds, unstretched) to the time in the source video file.
//! Pure data structure, no dependencies on au3.
class VideoTimeMap
{
public:
    struct Segment {
        double seqStart = 0.0;
        double seqEnd = 0.0;
        std::optional<double> srcStart; // nullopt: no video here
    };

    VideoTimeMap() = default;

    static VideoTimeMap identity(double duration);

    bool empty() const;
    const std::vector<Segment>& segments() const;

    //! Removes [t0, t1) and shifts the later parts left
    void erase(double t0, double t1);

    //! Inserts `length` seconds at t, shifting the later parts right.
    //! The inserted part is filled with the segments of `source` (scaled to `length`),
    //! or with one "no video" segment if source is null or empty.
    void insert(double t, double length, const VideoTimeMap* source);

    std::optional<double> sourceTimeAt(double seqT) const;

    //! `seqStart,seqEnd,src;...` with `-` as src for "no video"
    std::string serialize() const;
    static VideoTimeMap parse(const std::string& str);

private:
    void splitAt(double t);

    std::vector<Segment> m_segments;
};
}
