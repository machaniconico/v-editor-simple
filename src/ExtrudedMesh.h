#pragma once

#include <QFont>
#include <QPolygonF>
#include <QVector>
#include <QVector3D>

/*  ExtrudedMesh — pure geometry module for 3D text/shape extrusion.
 *
 *  Normalisation convention (glyphContours):
 *    - The bounding-box centre is translated to (0,0).
 *    - The result is scaled so that the tallest glyph extent (max Y - min Y) = 1.0.
 *    - If the height is zero (degenerate font), no scaling is applied.
 *
 *  Triangulation convention (triangulateRing):
 *    - Centroid-fan triangulation: connect every edge to the ring centroid.
 *    - This works correctly for convex rings. For concave glyph outlines the
 *      result may contain self-intersecting triangles; a full ear-clipping
 *      triangulator is a future enhancement. Documented limitation.
 *
 *  Normal convention (buildExtrudedMesh):
 *    - Cap normals are smoothed (averaged per-vertex) when smoothCapNormals=true.
 *    - Side-wall normals are outward-facing edge normals (flat per-face).
 *    - All returned normals are unit-length (within 1e-3). Degenerate triangles
 *      produce (0,0,0). No NaN/Inf for valid inputs.
 *
 *  Z-range:
 *    - Front cap surface sits at z = 0.
 *    - Bevel steps (if any) descend from z = 0 toward z = -depth within the
 *      same [-depth, 0] envelope.
 *    - Back cap surface sits at z = -depth.
 *    - All vertices stay within [-depth, 0].
 */

namespace mesh3d {

struct ExtrudeParams {
    double depth          = 0.2;
    double bevelDepth     = 0.02;
    double bevelWidth     = 0.02;
    int    bevelSegments  = 2;
    bool   smoothCapNormals = true;
};

struct TriMesh {
    QVector<QVector3D> positions;
    QVector<QVector3D> normals;
    QVector<quint32>   indices;
};

/* Extract glyph outlines from a QFont as closed 2D polygons.
 * Returns polygons normalised: centred at origin, max height = 1.0. */
QVector<QPolygonF> glyphContours(const QString &text, const QFont &font, double flatness = 0.5);

/* Centroid-fan triangulation of a single closed ring.
 * Returns flat triplet indices relative to the input ring vertices. */
QVector<quint32> triangulateRing(const QPolygonF &ring);

/* Build an extruded TriMesh from 2D contours.
 * Produces front cap, back cap, side walls, and optional bevel. */
TriMesh buildExtrudedMesh(const QVector<QPolygonF> &contours, const ExtrudeParams &params);

} // namespace mesh3d
