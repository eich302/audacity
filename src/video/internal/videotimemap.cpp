/*
* Audacity: A Digital Audio Editor
*/
#include "videotimemap.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

using namespace au::video;

namespace {
constexpr double EPS = 1e-9;

std::string formatDouble(double v)
{
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return buf;
}

bool parseDouble(const std::string& s, double& out)
{
    if (s.empty()) {
        return false;
    }
    char* end = nullptr;
    out = std::strtod(s.c_str(), &end);
    return end && *end == '\0';
}
}

VideoTimeMap VideoTimeMap::identity(double duration)
{
    VideoTimeMap map;
    if (duration > EPS) {
        map.m_segments.push_back({ 0.0, duration, 0.0 });
    }
    return map;
}

bool VideoTimeMap::empty() const
{
    return m_segments.empty();
}

const std::vector<VideoTimeMap::Segment>& VideoTimeMap::segments() const
{
    return m_segments;
}

void VideoTimeMap::splitAt(double t)
{
    for (size_t i = 0; i < m_segments.size(); ++i) {
        Segment& s = m_segments[i];
        if (t > s.seqStart + EPS && t < s.seqEnd - EPS) {
            Segment tail;
            tail.seqStart = t;
            tail.seqEnd = s.seqEnd;
            if (s.srcStart) {
                tail.srcStart = *s.srcStart + (t - s.seqStart);
            }
            s.seqEnd = t;
            m_segments.insert(m_segments.begin() + i + 1, tail);
            return;
        }
    }
}

void VideoTimeMap::erase(double t0, double t1)
{
    if (t1 - t0 <= 0.0 || m_segments.empty()) {
        return;
    }

    splitAt(t0);
    splitAt(t1);

    const double len = t1 - t0;
    std::vector<Segment> result;
    result.reserve(m_segments.size());
    for (const Segment& s : m_segments) {
        if (s.seqStart >= t0 - EPS && s.seqEnd <= t1 + EPS) {
            continue; // removed
        }
        Segment copy = s;
        if (copy.seqStart >= t1 - EPS) {
            copy.seqStart -= len;
            copy.seqEnd -= len;
        }
        result.push_back(copy);
    }
    m_segments = std::move(result);
}

void VideoTimeMap::insert(double t, double length, const VideoTimeMap* source)
{
    if (length <= 0.0) {
        return;
    }

    splitAt(t);

    std::vector<Segment> result;
    result.reserve(m_segments.size() + 2);

    bool inserted = false;
    auto doInsert = [&]() {
        inserted = true;
        if (source && !source->empty()) {
            const double srcTotal = source->m_segments.back().seqEnd - source->m_segments.front().seqStart;
            const double base = source->m_segments.front().seqStart;
            const double scale = srcTotal > EPS ? length / srcTotal : 1.0;
            for (const Segment& s : source->m_segments) {
                Segment copy;
                copy.seqStart = t + (s.seqStart - base) * scale;
                copy.seqEnd = t + (s.seqEnd - base) * scale;
                copy.srcStart = s.srcStart;
                result.push_back(copy);
            }
        } else {
            result.push_back({ t, t + length, std::nullopt });
        }
    };

    for (const Segment& s : m_segments) {
        if (!inserted && s.seqStart >= t - EPS) {
            doInsert();
        }
        Segment copy = s;
        if (inserted) {
            copy.seqStart += length;
            copy.seqEnd += length;
        }
        result.push_back(copy);
    }
    if (!inserted) {
        doInsert();
    }
    m_segments = std::move(result);
}

std::optional<double> VideoTimeMap::sourceTimeAt(double seqT) const
{
    for (const Segment& s : m_segments) {
        if (seqT >= s.seqStart - EPS && seqT < s.seqEnd - EPS) {
            if (!s.srcStart) {
                return std::nullopt;
            }
            return *s.srcStart + std::max(0.0, seqT - s.seqStart);
        }
    }
    return std::nullopt;
}

std::string VideoTimeMap::serialize() const
{
    std::string out;
    for (const Segment& s : m_segments) {
        if (!out.empty()) {
            out += ';';
        }
        out += formatDouble(s.seqStart) + ',' + formatDouble(s.seqEnd) + ',';
        out += s.srcStart ? formatDouble(*s.srcStart) : std::string("-");
    }
    return out;
}

VideoTimeMap VideoTimeMap::parse(const std::string& str)
{
    VideoTimeMap map;

    size_t pos = 0;
    while (pos < str.size()) {
        size_t end = str.find(';', pos);
        if (end == std::string::npos) {
            end = str.size();
        }
        const std::string item = str.substr(pos, end - pos);
        pos = end + 1;

        const size_t c1 = item.find(',');
        const size_t c2 = c1 == std::string::npos ? std::string::npos : item.find(',', c1 + 1);
        if (c2 == std::string::npos) {
            return VideoTimeMap(); // malformed
        }

        Segment seg;
        const std::string src = item.substr(c2 + 1);
        if (!parseDouble(item.substr(0, c1), seg.seqStart)
            || !parseDouble(item.substr(c1 + 1, c2 - c1 - 1), seg.seqEnd)) {
            return VideoTimeMap();
        }
        if (src != "-") {
            double v = 0.0;
            if (!parseDouble(src, v)) {
                return VideoTimeMap();
            }
            seg.srcStart = v;
        }
        map.m_segments.push_back(seg);
    }

    return map;
}
