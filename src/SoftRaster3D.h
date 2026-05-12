#pragma once

#include <QColor>
#include <QImage>
#include <QSize>
#include <QVector3D>
#include <QVector4D>

#include "ExtrudedMesh.h"

/*  SoftRaster3D — tiny dependency-free software triangle rasterizer.
 *
 *  No OpenGL, no third-party libraries — QtGui/QtCore only. Used to render
 *  3D extruded text / shapes (mesh3d::TriMesh) into a flat QImage.
 *
 *  Matrix convention:
 *    - Mat4 stores 16 floats in COLUMN-MAJOR order: m[col*4 + row].
 *      So m[0..3] is the first column, m[4..7] the second, etc.
 *      Element accessor at(row, col) == m[col*4 + row].
 *    - Vectors are treated as column vectors; transform = M * v.
 *    - operator* composes left-to-right in the usual math sense:
 *      (A * B) * v  ==  A * (B * v).
 *    - Coordinate handedness: right-handed (OpenGL-style) clip space;
 *      perspective() maps the view-space [-zNear, -zFar] range into NDC
 *      z ∈ [-1, 1], camera looks down -Z.
 *
 *  Image convention:
 *    - render()/renderMeshAuto() return QImage::Format_ARGB32 (straight,
 *      non-premultiplied alpha).
 *
 *  Winding / culling:
 *    - Front-facing triangles are counter-clockwise (CCW) when viewed from the
 *      +Z side in model space — this matches mesh3d::buildExtrudedMesh front
 *      caps (normal +Z). After the Y-flip applied in the screen mapping, such a
 *      triangle has a POSITIVE signed area in (Y-down) screen pixel space, so:
 *        screen signed area > 0  <=>  front-facing  (kept)
 *        screen signed area < 0  <=>  back-facing   (culled)
 *      Backface culling is always on for solid fill; wireframe draws every
 *      triangle's edges. No API knob — kept simple.
 */

namespace softras {

// 4x4 float matrix, column-major (m[col*4 + row]).
struct Mat4 {
    float m[16];

    Mat4();

    // Row/col accessor: at(row, col).
    float &at(int row, int col) { return m[col * 4 + row]; }
    float at(int row, int col) const { return m[col * 4 + row]; }

    static Mat4 identity();
    static Mat4 perspective(double fovYdeg, double aspect, double zNear, double zFar);
    static Mat4 lookAt(QVector3D eye, QVector3D center, QVector3D up);
    static Mat4 translate(QVector3D t);
    static Mat4 scale(QVector3D s);
    static Mat4 rotateX(double deg);
    static Mat4 rotateY(double deg);
    static Mat4 rotateZ(double deg);

    Mat4 operator*(const Mat4 &rhs) const;       // matrix * matrix
    QVector4D operator*(const QVector4D &v) const; // matrix * column-vector
};

struct RenderParams {
    QVector3D lightDir = QVector3D(0.3f, 0.4f, -1.0f);
    QColor frontColor = QColor(220, 200, 120);
    QColor sideColor = QColor(180, 150, 90);
    QColor ambient = QColor(40, 40, 40);
    QColor background = QColor(0, 0, 0, 0);
    bool wireframe = false;
};

/*  Render a triangle mesh.
 *
 *  Pipeline per triangle (consecutive index triplets):
 *    1. clip = proj * view * model * vec4(pos, 1)   (vertices with w <= 0,
 *       i.e. on/behind the camera, cause the whole triangle to be skipped —
 *       there is no near-plane clipping)
 *    2. perspective divide -> NDC (xy in [-1,1], z in [-1,1])
 *    3. NDC -> screen pixel coords (x: [0,w], y flipped so +Y is up: [0,h])
 *    4. backface cull by screen-space signed area (front-facing == positive)
 *    5. rasterize the screen bounding box with edge-function barycentrics and
 *       a top-left fill rule (so adjacent triangles do not double-cover edges)
 *    6. per covered pixel: interpolate depth (NDC z) for the z-buffer test
 *       (nearer == smaller NDC z), then Lambert-shade with the normals
 *       transformed by the model matrix's rotation part. Fragments outside the
 *       [-1,1] NDC depth slab are discarded.
 *
 *  Shading: litColor = ambient + max(0, dot(normalize(N), normalize(-lightDir)))
 *           * baseColor, clamped per-channel to [0,255]. baseColor = frontColor
 *           when |modelNormal.z| dominates |x| and |y| (front/back caps), else
 *           sideColor (side walls).
 *
 *  Returns a QImage::Format_ARGB32 (straight, non-premultiplied alpha) of size
 *  outSize. Pixels not covered by any triangle == p.background. Never produces
 *  NaN/Inf pixels; deterministic for fixed inputs.
 */
QImage render(const mesh3d::TriMesh &mesh, const Mat4 &model, const Mat4 &view,
              const Mat4 &proj, QSize outSize, const RenderParams &p);

/*  Convenience: orbit camera around the mesh centroid and render.
 *
 *  Builds proj = perspective(45, aspect, 0.05, max(10, cameraDistance*4)),
 *  places the eye at centroid + cameraDistance * (rotateY(yaw)*rotateX(pitch)
 *  applied to a -Z unit direction), view = lookAt(eye, centroid, {0,1,0}),
 *  model = identity().
 */
QImage renderMeshAuto(const mesh3d::TriMesh &mesh, QSize outSize, double yawDeg,
                      double pitchDeg, double cameraDistance, const RenderParams &p);

} // namespace softras
