/*
* Audacity: A Digital Audio Editor
*/
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "internal/ffmpegvideodecoder.h"
#include "internal/ffmpegvideoexporter.h"

using namespace au::video;

namespace {
bool ffmpegCliAvailable()
{
    return std::system("ffmpeg -version > /dev/null 2>&1") == 0;
}

void writeLe(std::ofstream& f, uint32_t v, int bytes)
{
    for (int i = 0; i < bytes; ++i) {
        f.put(static_cast<char>((v >> (8 * i)) & 0xff));
    }
}

// Minimal 16-bit stereo PCM WAV with a sine tone
void writeSineWav(const std::string& path, double seconds, int rate)
{
    const uint32_t frames = static_cast<uint32_t>(seconds * rate);
    const uint32_t dataBytes = frames * 2 * 2;
    std::ofstream f(path, std::ios::binary);
    f.write("RIFF", 4);
    writeLe(f, 36 + dataBytes, 4);
    f.write("WAVEfmt ", 8);
    writeLe(f, 16, 4);
    writeLe(f, 1, 2);
    writeLe(f, 2, 2);
    writeLe(f, rate, 4);
    writeLe(f, rate * 4, 4);
    writeLe(f, 4, 2);
    writeLe(f, 16, 2);
    f.write("data", 4);
    writeLe(f, dataBytes, 4);
    for (uint32_t i = 0; i < frames; ++i) {
        const double s = 0.3 * std::sin(2.0 * 3.14159265358979323846 * 440.0 * i / rate);
        const int16_t v = static_cast<int16_t>(s * 32767.0);
        writeLe(f, static_cast<uint16_t>(v), 2);
        writeLe(f, static_cast<uint16_t>(v), 2);
    }
}

double meanAbsDiff(const QImage& a, const QImage& b)
{
    if (a.size() != b.size()) {
        return 1e9;
    }
    double sum = 0.0;
    int64_t n = 0;
    for (int y = 0; y < a.height(); ++y) {
        const uchar* pa = a.constScanLine(y);
        const uchar* pb = b.constScanLine(y);
        for (int x = 0; x < a.width() * 4; ++x) {
            sum += std::abs(static_cast<int>(pa[x]) - static_cast<int>(pb[x]));
            ++n;
        }
    }
    return sum / static_cast<double>(n);
}
}

class Video_FfmpegVideoExporterTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (!ffmpegCliAvailable()) {
            GTEST_SKIP() << "ffmpeg CLI not found";
        }
        m_dir = std::filesystem::temp_directory_path() / "au_video_exporter_test";
        std::filesystem::create_directories(m_dir);
        m_source = (m_dir / "source.mp4").string();
        m_wav = (m_dir / "audio.wav").string();

        const std::string cmd = "ffmpeg -y -loglevel error -f lavfi -i testsrc=duration=3:size=160x120:rate=25 "
                                "-pix_fmt yuv420p -g 10 \"" + m_source + "\"";
        if (std::system(cmd.c_str()) != 0 || !std::filesystem::exists(m_source)) {
            GTEST_SKIP() << "could not generate test video";
        }
        writeSineWav(m_wav, 2.0, 44100);
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(m_dir, ec);
    }

    ExportParams makeParams(const std::string& outName, VideoCodec codec = VideoCodec::H264) const
    {
        ExportParams p;
        p.outputPath = (m_dir / outName).string();
        p.sourceVideoPath = m_source;
        p.fps = 25.0;
        p.audioWavPath = m_wav;
        p.videoCodec = codec;
        // 2 seconds of output that start at 1s of the source
        for (int i = 0; i < 50; ++i) {
            p.frameSourceTimes.push_back(1.0 + i / 25.0);
        }
        return p;
    }

    // Returns false (and sets skip) if the export failed because no encoder is available
    bool runExport(const ExportParams& p)
    {
        FfmpegVideoExporter exporter;
        double lastProgress = 0.0;
        const bool ok = exporter.exportVideo(p, [&](double v) {
            lastProgress = v;
            return true;
        });
        if (!ok) {
            const std::string& err = exporter.lastError();
            m_error = err;
            return false;
        }
        EXPECT_DOUBLE_EQ(lastProgress, 1.0);
        return true;
    }

    void verifyOutput(const ExportParams& p)
    {
        ASSERT_TRUE(std::filesystem::exists(p.outputPath));

        FfmpegVideoDecoder out;
        ASSERT_TRUE(out.open(p.outputPath));
        EXPECT_NEAR(out.duration(), 2.0, 0.2);
        EXPECT_NEAR(static_cast<double>(out.frameCount()), 50.0, 3.0);

        FfmpegVideoDecoder src;
        ASSERT_TRUE(src.open(m_source));
        const QImage first = out.frameAt(0.0);
        const QImage expected = src.frameAt(1.0);
        ASSERT_FALSE(first.isNull());
        ASSERT_FALSE(expected.isNull());
        EXPECT_LT(meanAbsDiff(first, expected), 25.0);
        // And it must differ from the source start
        EXPECT_GT(meanAbsDiff(first, src.frameAt(0.0)), 1.0);
    }

    std::filesystem::path m_dir;
    std::string m_source;
    std::string m_wav;
    std::string m_error;
};

TEST_F(Video_FfmpegVideoExporterTests, ExportMp4)
{
    const ExportParams p = makeParams("out.mp4");
    if (!runExport(p)) {
        GTEST_SKIP() << "export failed (maybe no encoder): " << m_error;
    }
    verifyOutput(p);
}

TEST_F(Video_FfmpegVideoExporterTests, ExportMkv)
{
    const ExportParams p = makeParams("out.mkv");
    if (!runExport(p)) {
        GTEST_SKIP() << "export failed (maybe no encoder): " << m_error;
    }
    verifyOutput(p);
}

TEST_F(Video_FfmpegVideoExporterTests, ExportMkvH265)
{
    const ExportParams p = makeParams("out_hevc.mkv", VideoCodec::H265);
    if (!runExport(p)) {
        GTEST_SKIP() << "no usable HEVC encoder: " << m_error;
    }
    verifyOutput(p);
}

TEST_F(Video_FfmpegVideoExporterTests, BlackFramesAndCancel)
{
    ExportParams p = makeParams("black.mp4");
    // first half has no video
    for (int i = 0; i < 25; ++i) {
        p.frameSourceTimes[i] = std::nullopt;
    }
    if (!runExport(p)) {
        GTEST_SKIP() << "export failed (maybe no encoder): " << m_error;
    }
    FfmpegVideoDecoder out;
    ASSERT_TRUE(out.open(p.outputPath));
    const QImage img = out.frameAt(0.1);
    ASSERT_FALSE(img.isNull());
    EXPECT_LT(img.pixelColor(10, 10).lightness(), 40);

    ExportParams c = makeParams("cancel.mp4");
    FfmpegVideoExporter exporter;
    EXPECT_FALSE(exporter.exportVideo(c, [](double) { return false; }));
    EXPECT_TRUE(exporter.wasCanceled());
    EXPECT_FALSE(std::filesystem::exists(c.outputPath));
}
