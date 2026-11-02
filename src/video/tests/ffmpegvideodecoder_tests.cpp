/*
* Audacity: A Digital Audio Editor
*/
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <string>

#include "internal/ffmpegvideodecoder.h"

using namespace au::video;

namespace {
bool ffmpegCliAvailable()
{
    return std::system("ffmpeg -version > /dev/null 2>&1") == 0;
}
}

class Video_FfmpegVideoDecoderTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (!ffmpegCliAvailable()) {
            GTEST_SKIP() << "ffmpeg CLI not found";
        }
        m_path = (std::filesystem::temp_directory_path() / "au_video_decoder_test.mp4").string();
        const std::string cmd = "ffmpeg -y -loglevel error -f lavfi -i testsrc=duration=2:size=160x120:rate=25 "
                                "-pix_fmt yuv420p -g 10 \"" + m_path + "\"";
        if (std::system(cmd.c_str()) != 0 || !std::filesystem::exists(m_path)) {
            GTEST_SKIP() << "could not generate test video";
        }
    }

    void TearDown() override
    {
        if (!m_path.empty()) {
            std::error_code ec;
            std::filesystem::remove(m_path, ec);
        }
    }

    std::string m_path;
};

TEST_F(Video_FfmpegVideoDecoderTests, OpenFailsOnMissingFile)
{
    FfmpegVideoDecoder d;
    EXPECT_FALSE(d.open("/nonexistent/file.mp4"));
    EXPECT_FALSE(d.isOpen());
    EXPECT_TRUE(d.frameAt(0.0).isNull());
}

TEST_F(Video_FfmpegVideoDecoderTests, Properties)
{
    FfmpegVideoDecoder d;
    ASSERT_TRUE(d.open(m_path));
    EXPECT_TRUE(d.isOpen());
    EXPECT_DOUBLE_EQ(d.fps(), 25.0);
    EXPECT_NEAR(static_cast<double>(d.frameCount()), 50.0, 2.0);
    EXPECT_NEAR(d.duration(), 2.0, 0.1);
    EXPECT_LE(d.frameSize().width(), 160);
    EXPECT_LE(d.frameSize().height(), 120);
    d.close();
    EXPECT_FALSE(d.isOpen());
}

TEST_F(Video_FfmpegVideoDecoderTests, FrameAt)
{
    FfmpegVideoDecoder d;
    ASSERT_TRUE(d.open(m_path));

    const QImage a = d.frameAt(0.0);
    ASSERT_FALSE(a.isNull());
    EXPECT_LE(a.width(), 160);
    EXPECT_LE(a.height(), 120);

    const QImage b = d.frameAt(1.0);
    ASSERT_FALSE(b.isNull());
    EXPECT_NE(a, b);

    const QImage c = d.frameAt(1.0);
    EXPECT_EQ(b, c);

    // Sequential stepping and a backward seek
    EXPECT_FALSE(d.frameAt(1.04).isNull());
    EXPECT_FALSE(d.frameAt(1.2).isNull());
    EXPECT_EQ(d.frameAt(0.0), a);
}

TEST_F(Video_FfmpegVideoDecoderTests, OutOfRangeDoesNotCrash)
{
    FfmpegVideoDecoder d;
    ASSERT_TRUE(d.open(m_path));
    EXPECT_FALSE(d.frameAt(-1.0).isNull());
    EXPECT_FALSE(d.frameAt(100.0).isNull());
}
