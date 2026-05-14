#include "MobileDeviceProfile.h"

namespace mobile {

// ---------------------------------------------------------------------------
// DevicePresets — built once, returned by value each call.
// Resolution / codec / bitrate caps approximated from public Apple/Google specs
// (Apple Developer "Supported Media Formats" tables, Google Android CDD).
// ---------------------------------------------------------------------------
QList<DeviceProfile> allDevices()
{
    static const QList<DeviceProfile> kDevices = {
        // ----- iOS phones -----
        {
            QStringLiteral("iphone_15_pro"),
            QStringLiteral("iPhone 15 Pro"),
            Category::iOSPhone,
            QSize(3840, 2160),
            60,
            QStringLiteral("hevc"),
            120'000,
            true,
            QStringLiteral("P3"),
            QStringLiteral("L5_2"),
            QStringLiteral("L6"),
            QStringLiteral("High"),
            96'000,
            true,
            320
        },
        {
            QStringLiteral("iphone_14"),
            QStringLiteral("iPhone 14"),
            Category::iOSPhone,
            QSize(3840, 2160),
            60,
            QStringLiteral("hevc"),
            100'000,
            true,
            QStringLiteral("P3"),
            QStringLiteral("L5_1"),
            QStringLiteral("L6"),
            QStringLiteral("Main"),
            48'000,
            false,
            256
        },

        // ----- iOS tablet -----
        {
            QStringLiteral("ipad_pro_m2"),
            QStringLiteral("iPad Pro (M2)"),
            Category::iOSTablet,
            QSize(3840, 2160),
            60,
            QStringLiteral("hevc"),
            120'000,
            true,
            QStringLiteral("P3"),
            QStringLiteral("L5_2"),
            QStringLiteral("L6"),
            QStringLiteral("High"),
            96'000,
            true,
            320
        },

        // ----- Android phones -----
        {
            QStringLiteral("galaxy_s24"),
            QStringLiteral("Samsung Galaxy S24"),
            Category::AndroidPhone,
            QSize(3840, 2160),
            60,
            QStringLiteral("hevc"),
            100'000,
            true,
            QStringLiteral("P3"),
            QStringLiteral("L5_1"),
            QStringLiteral("L6"),
            QStringLiteral("Main"),
            96'000,
            true,
            320
        },
        {
            QStringLiteral("galaxy_s22"),
            QStringLiteral("Samsung Galaxy S22"),
            Category::AndroidPhone,
            QSize(3840, 2160),
            60,
            QStringLiteral("hevc"),
            80'000,
            true,
            QStringLiteral("P3"),
            QStringLiteral("L5_1"),
            QStringLiteral("L5_1"),
            QStringLiteral("Main"),
            48'000,
            false,
            256
        },
        {
            QStringLiteral("pixel_8"),
            QStringLiteral("Google Pixel 8"),
            Category::AndroidPhone,
            QSize(3840, 2160),
            60,
            QStringLiteral("hevc"),
            100'000,
            true,
            QStringLiteral("P3"),
            QStringLiteral("L5_1"),
            QStringLiteral("L5_1"),
            QStringLiteral("Main"),
            48'000,
            false,
            256
        },
        {
            QStringLiteral("pixel_6"),
            QStringLiteral("Google Pixel 6"),
            Category::AndroidPhone,
            QSize(3840, 2160),
            60,
            QStringLiteral("hevc"),
            72'000,
            true,
            QStringLiteral("P3"),
            QStringLiteral("L5_1"),
            QStringLiteral("L5_1"),
            QStringLiteral("Main"),
            48'000,
            false,
            192
        },

        // ----- Android tablet -----
        {
            QStringLiteral("galaxy_tab_s9"),
            QStringLiteral("Samsung Galaxy Tab S9"),
            Category::AndroidTablet,
            QSize(3840, 2160),
            60,
            QStringLiteral("hevc"),
            100'000,
            true,
            QStringLiteral("P3"),
            QStringLiteral("L5_1"),
            QStringLiteral("L6"),
            QStringLiteral("Main"),
            96'000,
            true,
            320
        },

        // ----- Generic fallbacks -----
        {
            QStringLiteral("generic_ios"),
            QStringLiteral("Generic iOS Device"),
            Category::Generic,
            QSize(1920, 1080),
            30,
            QStringLiteral("h264"),
            20'000,
            false,
            QStringLiteral("sRGB"),
            QStringLiteral("L4_2"),
            QStringLiteral("L5"),
            QStringLiteral("Main"),
            48'000,
            false,
            192
        },
        {
            QStringLiteral("generic_android"),
            QStringLiteral("Generic Android Device"),
            Category::Generic,
            QSize(1920, 1080),
            30,
            QStringLiteral("h264"),
            20'000,
            false,
            QStringLiteral("sRGB"),
            QStringLiteral("L4_2"),
            QStringLiteral("L5"),
            QStringLiteral("Main"),
            48'000,
            false,
            192
        }
    };
    return kDevices;
}

DeviceProfile deviceById(const QString& id)
{
    const auto list = allDevices();
    for (const auto& dev : list) {
        if (dev.id == id) return dev;
    }
    // Return generic_android as safe default if not found.
    for (const auto& dev : list) {
        if (dev.id == QStringLiteral("generic_android")) return dev;
    }
    return {};
}

QStringList deviceIds()
{
    QStringList ids;
    const auto list = allDevices();
    ids.reserve(list.size());
    for (const auto& dev : list) ids << dev.id;
    return ids;
}

QString categoryDisplayName(Category c)
{
    switch (c) {
        case Category::iOSPhone:     return QStringLiteral("iOS Phone");
        case Category::iOSTablet:    return QStringLiteral("iOS Tablet");
        case Category::AndroidPhone: return QStringLiteral("Android Phone");
        case Category::AndroidTablet:return QStringLiteral("Android Tablet");
        case Category::Generic:      return QStringLiteral("Generic");
    }
    return QStringLiteral("Unknown");
}

} // namespace mobile
