/*
* Audacity: A Digital Audio Editor
*/
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <string>

#include "internal/videoservice.h"

using namespace au::video;

namespace {
bool ffmpegCliAvailable()
{
    return std::system("ffmpeg -version > /dev/null 2>&1") == 0;
}
}

class Video_VideoServiceTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (!ffmpegCliAvailable()) {
            GTEST_SKIP() << "ffmpeg CLI not found";
        }
        m_path = (std::filesystem::temp_directory_path() / "au_video_service_test.mp4").string();
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

TEST_F(Video_VideoServiceTests, LoadFrameUnload)
{
    // No ioc context is needed for load/unload/frameAt
    VideoService service(nullptr);
    EXPECT_FALSE(service.hasVideo());
    EXPECT_TRUE(service.frameAt(0.0).isNull());

    int notifications = 0;
    muse::async::Asyncable receiver;
    service.videoChanged().onNotify(&receiver, [&notifications]() { ++notifications; });

    EXPECT_FALSE(service.loadVideo("/nonexistent/video.mp4"));
    EXPECT_FALSE(service.hasVideo());

    ASSERT_TRUE(service.loadVideo(m_path));
    EXPECT_TRUE(service.hasVideo());
    EXPECT_EQ(service.videoPath().toStdString(), m_path);
    EXPECT_NEAR(service.fps(), 25.0, 0.01);
    EXPECT_NEAR(service.duration(), 2.0, 0.1);
    EXPECT_EQ(service.frameCount(), 50);

    QImage img = service.frameAt(1.0);
    EXPECT_FALSE(img.isNull());

    service.unloadVideo();
    EXPECT_FALSE(service.hasVideo());
    EXPECT_TRUE(service.videoPath().empty());
    EXPECT_TRUE(service.frameAt(1.0).isNull());
    EXPECT_EQ(service.fps(), 0.0);
    EXPECT_GE(notifications, 2);
}
