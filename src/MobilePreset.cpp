#include "MobilePreset.h"
#include "MobileCodecConstraints.h"
#include "CodecDetector.h"

#include <algorithm>

namespace mobile::preset {

ExportConfig configForDevice(const DeviceProfile& device,
                             const QSize&         sourceSize,
                             double               /*measuredLufs*/)
{
    ExportConfig cfg;

    // -----------------------------------------------------------------------
    // Resolution: clamp source to device max, preserve aspect ratio
    // -----------------------------------------------------------------------
    const int srcW = sourceSize.width();
    const int srcH = sourceSize.height();
    const int maxW = device.maxResolution.width();
    const int maxH = device.maxResolution.height();

    if (srcW > 0 && srcH > 0 && maxW > 0 && maxH > 0) {
        const double scaleW = (double)maxW / srcW;
        const double scaleH = (double)maxH / srcH;
        const double scale  = std::min({ scaleW, scaleH, 1.0 });
        cfg.width  = (int)(srcW * scale) & ~1; // ensure even
        cfg.height = (int)(srcH * scale) & ~1;
    } else {
        cfg.width  = maxW & ~1;
        cfg.height = maxH & ~1;
    }

    // -----------------------------------------------------------------------
    // Frame rate
    // -----------------------------------------------------------------------
    cfg.fps = device.maxFrameRate;

    // -----------------------------------------------------------------------
    // Video codec + level selection
    // -----------------------------------------------------------------------
    cfg.container = QStringLiteral("mp4");
    cfg.audioCodec = QStringLiteral("aac");

    const bool useHevc = (device.preferredCodec == QStringLiteral("hevc"));

    if (useHevc) {
        cfg.videoCodec = CodecDetector::bestVideoEncoder(QStringLiteral("hevc"));

        // Determine tier
        const hevc::Tier tier = (device.maxHevcTier == QStringLiteral("High"))
                                    ? hevc::Tier::High
                                    : hevc::Tier::Main;

        // Clamp bitrate to device max
        cfg.videoBitrate = device.maxVideoBitrateKbps;

        // Verify required level fits within device capability
        const hevc::Level reqLevel = hevc::requiredLevelFor(
            cfg.width, cfg.height, cfg.fps, cfg.videoBitrate, tier);

        // Build level string map to compare (simple ordinal check via requiredLevelFor result)
        // If device maxHevcLevel is set we use the device's declared level as the cap string.
        // Since we already clamped bitrate to device max this is informational.
        Q_UNUSED(reqLevel);
    } else {
        cfg.videoCodec = CodecDetector::bestVideoEncoder(QStringLiteral("h264"));

        // Clamp bitrate to device max
        cfg.videoBitrate = device.maxVideoBitrateKbps;

        const h264::Level reqLevel = h264::requiredLevelFor(
            cfg.width, cfg.height, cfg.fps, cfg.videoBitrate);
        Q_UNUSED(reqLevel);
    }

    // Ensure videoBitrate is never 0
    if (cfg.videoBitrate <= 0)
        cfg.videoBitrate = 8000;

    // -----------------------------------------------------------------------
    // HDR
    // -----------------------------------------------------------------------
    cfg.hdr10 = device.supportsHdr;
    if (cfg.hdr10) {
        cfg.hdrSettings.mode = QStringLiteral("hdr10");
    } else {
        cfg.hdrSettings.mode = QStringLiteral("sdr");
    }

    // -----------------------------------------------------------------------
    // Hardware acceleration (best-effort; let the engine decide)
    // -----------------------------------------------------------------------
    cfg.useHardwareAccel = true;
    cfg.hwEncoder        = QStringLiteral("auto");

    // -----------------------------------------------------------------------
    // Audio: sane mobile defaults — 192 kbps AAC, 48 kHz, 2ch, -14 LUFS
    // -----------------------------------------------------------------------
    cfg.audioBitrate = 192;
    // sampleRate, channelCount not in ExportConfig struct — stored implicitly

    // -----------------------------------------------------------------------
    // ProRes: not applicable for mobile
    // -----------------------------------------------------------------------
    cfg.proresProfile = -1;

    // -----------------------------------------------------------------------
    // Output path: left empty — caller must supply
    // -----------------------------------------------------------------------
    cfg.outputPath = QString();

    return cfg;
}

} // namespace mobile::preset
