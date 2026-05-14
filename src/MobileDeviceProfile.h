#pragma once
#include <QString>
#include <QSize>
#include <QList>
#include <QStringList>

namespace mobile {

enum class Category {
    iOSPhone,
    iOSTablet,
    AndroidPhone,
    AndroidTablet,
    Generic
};

struct DeviceProfile {
    QString  id;
    QString  displayName;
    Category category;
    QSize    maxResolution;
    int      maxFrameRate;
    QString  preferredCodec;       // "h264" | "hevc" | "av1"
    int      maxVideoBitrateKbps;
    bool     supportsHdr;
    QString  colorPrimaries;       // "sRGB" | "P3" | "BT.2020"
    QString  maxH264Level;         // "L4_2", "L5_1", etc.
    QString  maxHevcLevel;         // "L5_1", "L6", etc.
    QString  maxHevcTier;          // "Main" | "High"
    int      maxAudioSampleRateHz; // 48000, 96000, 192000
    bool     hiResAudio;
    int      maxAudioBitrateKbps;
};

QList<DeviceProfile> allDevices();
DeviceProfile        deviceById(const QString& id);
QStringList          deviceIds();
QString              categoryDisplayName(Category c);

} // namespace mobile
