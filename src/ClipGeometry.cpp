#include "ClipGeometry.h"

#include <QPainter>

namespace clipgeom {

QTransform resolveTransform(const ClipTransform& t, QSize canvasSize)
{
    // Anchor = LAYER CENTER. The source is canvas-sized; its center (W/2,H/2)
    // is mapped to the placement point (canvas-center + normalized offset).
    // Scale and rotation pivot about the layer center.
    // videoDx/Dy are NORMALIZED fractions of canvas width/height (not pixels).
    //
    // Qt post-multiplies: the trailing translate() is the FIRST op applied
    // to a point, so the sequence below reads back-to-front:
    //   (1) un-center the canvas-sized source: shift (0,0) by (-W/2,-H/2)
    //   (2) scale about the now-centered origin
    //   (3) rotate about the now-centered origin
    //   (4) translate center to placement point
    //
    // Identity check (scale=1,dx=dy=rot=0):
    //   source (0,0) -> (+W/2,+H/2) -> scale(1) -> rot(0) -> (0,0)  [fills canvas]
    //   source (W,H) -> (+W/2,+H/2) -> scale(1) -> rot(0) -> (W,H)  [fills canvas]
    const double canvasW = canvasSize.width();
    const double canvasH = canvasSize.height();

    QTransform xf;
    xf.translate(canvasW * 0.5 + t.videoDx * canvasW,
                 canvasH * 0.5 + t.videoDy * canvasH); // (4) placement point
    xf.rotate(t.rotationDeg);                           // (3) pivot = placement
    xf.scale(t.videoScale, t.videoScale);               // (2) scale about placement
    xf.translate(-canvasW * 0.5, -canvasH * 0.5);       // (1) un-center source
    return xf;
}

QImage renderLayer(const QImage& src, const ClipTransform& t,
                   QSize canvasSize, bool smooth)
{
    // Self-scale: resolveTransform un-centers by canvasSize, so src MUST be
    // canvas-sized for the math to hold. Scale here defensively so any
    // native-resolution input (e.g. 1920x1080 into a 640x360 canvas) is
    // correct without requiring callers to pre-scale.
    const QImage scaled = (src.size() != canvasSize)
        ? src.scaled(canvasSize, Qt::IgnoreAspectRatio,
                     smooth ? Qt::SmoothTransformation : Qt::FastTransformation)
        : src;

    QImage canvas(canvasSize, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);

    QPainter p(&canvas);
    p.setRenderHint(QPainter::SmoothPixmapTransform, smooth);
    p.setRenderHint(QPainter::Antialiasing, smooth);
    p.setTransform(resolveTransform(t, canvasSize));
    p.drawImage(0, 0, scaled);
    p.end();

    return canvas;
}

} // namespace clipgeom
