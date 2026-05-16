#include "TrackMatteBake.h"

#include "MaskSystem.h"

#include <QSet>
#include <QtGlobal>
#include <atomic>

namespace trackmatte {

static std::atomic_bool g_appliedMatte{false};
static const bool g_observeAppliedMatte =
    !qEnvironmentVariableIsEmpty("VEDITOR_TRACKMATTE_SELFTEST");

bool selftestAppliedMatte()
{
    return g_appliedMatte.load(std::memory_order_acquire);
}

void selftestReset()
{
    g_appliedMatte.store(false, std::memory_order_release);
}

static bool isValidMatteSource(int layerIndex, int matteIndex, int layerCount)
{
    return matteIndex >= 0
        && matteIndex < layerCount
        && matteIndex != layerIndex;
}

QImage composite(const QVector<CompositeLayer>& layers,
                 const QVector<QImage>& layerImages,
                 QSize canvasSize)
{
    QImage canvas(canvasSize, QImage::Format_ARGB32);
    canvas.fill(Qt::transparent);

    const int layerCount = static_cast<int>(layers.size());
    const int imageCount = static_cast<int>(layerImages.size());
    QSet<int> matteSourceIndices;
    for (int i = 0; i < layerCount; ++i) {
        const CompositeLayer &layer = layers[i];
        if (layer.matteType == TrackMatteType::None)
            continue;

        const int matteIndex = layer.matteSourceLayerIndex;
        if (!isValidMatteSource(i, matteIndex, layerCount))
            continue;

        matteSourceIndices.insert(matteIndex);
    }

    auto imageAt = [&layerImages, imageCount](int index) -> QImage {
        if (index < 0 || index >= imageCount)
            return QImage();
        return layerImages[index];
    };

    for (int i = 0; i < layerCount; ++i) {
        const CompositeLayer &layer = layers[i];

        if (!layer.visible)
            continue;

        if (matteSourceIndices.contains(i))
            continue;

        QImage layerImage = imageAt(i);
        if (layerImage.isNull())
            continue;

        if (layer.matteType != TrackMatteType::None) {
            const int matteIndex = layer.matteSourceLayerIndex;
            if (isValidMatteSource(i, matteIndex, layerCount)) {
                const QImage matteImage = imageAt(matteIndex);
                if (!matteImage.isNull()) {
                    layerImage = MaskSystem::applyTrackMatte(
                        layerImage, matteImage, layer.matteType);
                    if (g_observeAppliedMatte)
                        g_appliedMatte.store(true, std::memory_order_release);
                }
            }
        }

        canvas = LayerCompositor::blendImages(
            canvas, layerImage, layer.blendMode, layer.opacity);
    }

    return canvas;
}

} // namespace trackmatte
