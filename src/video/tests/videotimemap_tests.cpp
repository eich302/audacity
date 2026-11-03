/*
* Audacity: A Digital Audio Editor
*/
#include <gtest/gtest.h>

#include "internal/videotimemap.h"

using namespace au::video;

namespace {
constexpr double TOL = 1e-9;
}

TEST(Video_VideoTimeMapTests, IdentityLookup)
{
    const VideoTimeMap map = VideoTimeMap::identity(10.0);
    EXPECT_FALSE(map.empty());
    EXPECT_NEAR(*map.sourceTimeAt(0.0), 0.0, TOL);
    EXPECT_NEAR(*map.sourceTimeAt(4.25), 4.25, TOL);
    EXPECT_FALSE(map.sourceTimeAt(10.0).has_value());
    EXPECT_FALSE(map.sourceTimeAt(-1.0).has_value());
    EXPECT_TRUE(VideoTimeMap::identity(0.0).empty());
}

TEST(Video_VideoTimeMapTests, EraseAtStart)
{
    VideoTimeMap map = VideoTimeMap::identity(10.0);
    map.erase(0.0, 3.0);
    EXPECT_NEAR(*map.sourceTimeAt(0.0), 3.0, TOL);
    EXPECT_NEAR(*map.sourceTimeAt(6.5), 9.5, TOL);
    EXPECT_FALSE(map.sourceTimeAt(7.0).has_value());
}

TEST(Video_VideoTimeMapTests, EraseInMiddle)
{
    VideoTimeMap map = VideoTimeMap::identity(10.0);
    map.erase(4.0, 6.0);
    EXPECT_NEAR(*map.sourceTimeAt(3.0), 3.0, TOL);
    EXPECT_NEAR(*map.sourceTimeAt(4.0), 6.0, TOL);
    EXPECT_NEAR(*map.sourceTimeAt(7.5), 9.5, TOL);
    EXPECT_FALSE(map.sourceTimeAt(8.0).has_value());
}

TEST(Video_VideoTimeMapTests, EraseAtEnd)
{
    VideoTimeMap map = VideoTimeMap::identity(10.0);
    map.erase(8.0, 10.0);
    EXPECT_NEAR(*map.sourceTimeAt(7.9), 7.9, TOL);
    EXPECT_FALSE(map.sourceTimeAt(8.0).has_value());
}

TEST(Video_VideoTimeMapTests, EraseSpanningSegments)
{
    VideoTimeMap map = VideoTimeMap::identity(10.0);
    map.erase(2.0, 3.0); // segments [0,2)->0, [2,9)->3
    map.erase(5.0, 6.0); // [0,2)->0, [2,5)->3, [5,8)->7
    map.erase(1.0, 6.5); // spans all three
    ASSERT_EQ(map.segments().size(), 2u);
    EXPECT_NEAR(*map.sourceTimeAt(0.5), 0.5, TOL);
    EXPECT_NEAR(*map.sourceTimeAt(1.0), 8.5, TOL); // source 7 + (6.5 - 5)
    EXPECT_NEAR(*map.sourceTimeAt(2.4), 9.9, TOL);
    EXPECT_FALSE(map.sourceTimeAt(2.5).has_value());
}

TEST(Video_VideoTimeMapTests, EraseEverything)
{
    VideoTimeMap map = VideoTimeMap::identity(5.0);
    map.erase(0.0, 5.0);
    EXPECT_TRUE(map.empty());
}

TEST(Video_VideoTimeMapTests, InsertSilence)
{
    VideoTimeMap map = VideoTimeMap::identity(10.0);
    map.insert(4.0, 2.0, nullptr);
    EXPECT_NEAR(*map.sourceTimeAt(3.9), 3.9, TOL);
    EXPECT_FALSE(map.sourceTimeAt(4.0).has_value());
    EXPECT_FALSE(map.sourceTimeAt(5.9).has_value());
    EXPECT_NEAR(*map.sourceTimeAt(6.0), 4.0, TOL);
    EXPECT_NEAR(*map.sourceTimeAt(11.9), 9.9, TOL);
    EXPECT_FALSE(map.sourceTimeAt(12.0).has_value());
}

TEST(Video_VideoTimeMapTests, InsertAtStartAndEnd)
{
    VideoTimeMap map = VideoTimeMap::identity(4.0);
    map.insert(0.0, 1.0, nullptr);
    EXPECT_FALSE(map.sourceTimeAt(0.5).has_value());
    EXPECT_NEAR(*map.sourceTimeAt(1.0), 0.0, TOL);

    map.insert(5.0, 1.0, nullptr);
    EXPECT_NEAR(*map.sourceTimeAt(4.9), 3.9, TOL);
    EXPECT_FALSE(map.sourceTimeAt(5.5).has_value());
}

TEST(Video_VideoTimeMapTests, InsertMappedSourceScaled)
{
    VideoTimeMap src = VideoTimeMap::identity(2.0);
    src.erase(0.0, 1.0); // [0,1) -> 1.0

    VideoTimeMap map = VideoTimeMap::identity(10.0);
    map.insert(5.0, 2.0, &src); // source total 1.0 scaled to 2.0
    EXPECT_NEAR(*map.sourceTimeAt(4.0), 4.0, TOL);
    EXPECT_NEAR(*map.sourceTimeAt(5.0), 1.0, TOL);
    EXPECT_NEAR(*map.sourceTimeAt(6.0), 2.0, TOL); // source offsets are not scaled
    EXPECT_NEAR(*map.sourceTimeAt(7.0), 5.0, TOL);
}

TEST(Video_VideoTimeMapTests, SerializeParseRoundTrip)
{
    VideoTimeMap map = VideoTimeMap::identity(10.123456789012345);
    map.erase(1.1, 2.3);
    map.insert(3.0, 0.7, nullptr);

    const std::string text = map.serialize();
    const VideoTimeMap parsed = VideoTimeMap::parse(text);
    EXPECT_EQ(parsed.serialize(), text);
    ASSERT_EQ(parsed.segments().size(), map.segments().size());
    for (size_t i = 0; i < map.segments().size(); ++i) {
        EXPECT_EQ(parsed.segments()[i].seqStart, map.segments()[i].seqStart);
        EXPECT_EQ(parsed.segments()[i].seqEnd, map.segments()[i].seqEnd);
        EXPECT_EQ(parsed.segments()[i].srcStart, map.segments()[i].srcStart);
    }
}

TEST(Video_VideoTimeMapTests, ParseMalformedGivesEmpty)
{
    EXPECT_TRUE(VideoTimeMap::parse("").empty());
    EXPECT_TRUE(VideoTimeMap::parse("garbage").empty());
    EXPECT_TRUE(VideoTimeMap::parse("0,1").empty());
    EXPECT_FALSE(VideoTimeMap::parse("0,1,-").empty());
}
