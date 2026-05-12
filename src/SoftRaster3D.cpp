#include "SoftRaster3D.h"

#include <algorithm>
#include <cmath>
#include <cstdlib> // std::abs(int) (Bresenham line)
#include <limits>

namespace softras {

namespace {
// Self-contained PI constant — do not rely on M_PI (not portable: MSVC only
// defines it when _USE_MATH_DEFINES is set before <cmath>).
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg2Rad = kPi / 180.0;
} // namespace

// ---------------------------------------------------------------------------
// Mat4 — column-major: m[col*4 + row].
// ---------------------------------------------------------------------------

Mat4::Mat4() {
    for (int i = 0; i < 16; ++i)
        m[i] = 0.0f;
}

Mat4 Mat4::identity() {
    Mat4 r;
    r.m[0] = 1.0f;
    r.m[5] = 1.0f;
    r.m[10] = 1.0f;
    r.m[15] = 1.0f;
    return r;
}

Mat4 Mat4::perspective(double fovYdeg, double aspect, double zNear, double zFar) {
    Mat4 r; // all zeros
    if (aspect == 0.0)
        aspect = 1.0;
    const double fovYrad = fovYdeg * kDeg2Rad;
    const double f = 1.0 / std::tan(fovYrad * 0.5);
    const double nf = 1.0 / (zNear - zFar);

    r.at(0, 0) = static_cast<float>(f / aspect);
    r.at(1, 1) = static_cast<float>(f);
    r.at(2, 2) = static_cast<float>((zFar + zNear) * nf);
    r.at(3, 2) = -1.0f;
    r.at(2, 3) = static_cast<float>(2.0 * zFar * zNear * nf);
    // r.at(3,3) stays 0
    return r;
}

Mat4 Mat4::lookAt(QVector3D eye, QVector3D center, QVector3D up) {
    QVector3D f = center - eye;
    if (f.lengthSquared() <= 0.0f)
        f = QVector3D(0.0f, 0.0f, -1.0f);
    f.normalize();

    QVector3D upn = up;
    if (upn.lengthSquared() <= 0.0f)
        upn = QVector3D(0.0f, 1.0f, 0.0f);
    upn.normalize();

    QVector3D s = QVector3D::crossProduct(f, upn);
    if (s.lengthSquared() <= 1e-12f) {
        // f and up are parallel — pick an arbitrary perpendicular.
        QVector3D alt = (std::fabs(f.x()) < 0.9f) ? QVector3D(1.0f, 0.0f, 0.0f)
                                                  : QVector3D(0.0f, 1.0f, 0.0f);
        s = QVector3D::crossProduct(f, alt);
    }
    s.normalize();
    const QVector3D u = QVector3D::crossProduct(s, f);

    Mat4 r = Mat4::identity();
    r.at(0, 0) = s.x();
    r.at(0, 1) = s.y();
    r.at(0, 2) = s.z();
    r.at(1, 0) = u.x();
    r.at(1, 1) = u.y();
    r.at(1, 2) = u.z();
    r.at(2, 0) = -f.x();
    r.at(2, 1) = -f.y();
    r.at(2, 2) = -f.z();
    r.at(0, 3) = -QVector3D::dotProduct(s, eye);
    r.at(1, 3) = -QVector3D::dotProduct(u, eye);
    r.at(2, 3) = QVector3D::dotProduct(f, eye);
    return r;
}

Mat4 Mat4::translate(QVector3D t) {
    Mat4 r = Mat4::identity();
    r.at(0, 3) = t.x();
    r.at(1, 3) = t.y();
    r.at(2, 3) = t.z();
    return r;
}

Mat4 Mat4::scale(QVector3D s) {
    Mat4 r = Mat4::identity();
    r.at(0, 0) = s.x();
    r.at(1, 1) = s.y();
    r.at(2, 2) = s.z();
    return r;
}

Mat4 Mat4::rotateX(double deg) {
    const double a = deg * kDeg2Rad;
    const float c = static_cast<float>(std::cos(a));
    const float s = static_cast<float>(std::sin(a));
    Mat4 r = Mat4::identity();
    r.at(1, 1) = c;
    r.at(1, 2) = -s;
    r.at(2, 1) = s;
    r.at(2, 2) = c;
    return r;
}

Mat4 Mat4::rotateY(double deg) {
    const double a = deg * kDeg2Rad;
    const float c = static_cast<float>(std::cos(a));
    const float s = static_cast<float>(std::sin(a));
    Mat4 r = Mat4::identity();
    r.at(0, 0) = c;
    r.at(0, 2) = s;
    r.at(2, 0) = -s;
    r.at(2, 2) = c;
    return r;
}

Mat4 Mat4::rotateZ(double deg) {
    const double a = deg * kDeg2Rad;
    const float c = static_cast<float>(std::cos(a));
    const float s = static_cast<float>(std::sin(a));
    Mat4 r = Mat4::identity();
    r.at(0, 0) = c;
    r.at(0, 1) = -s;
    r.at(1, 0) = s;
    r.at(1, 1) = c;
    return r;
}

Mat4 Mat4::operator*(const Mat4 &rhs) const {
    Mat4 r; // zeros
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k)
                sum += at(row, k) * rhs.at(k, col);
            r.at(row, col) = sum;
        }
    }
    return r;
}

QVector4D Mat4::operator*(const QVector4D &v) const {
    const float x = at(0, 0) * v.x() + at(0, 1) * v.y() + at(0, 2) * v.z() + at(0, 3) * v.w();
    const float y = at(1, 0) * v.x() + at(1, 1) * v.y() + at(1, 2) * v.z() + at(1, 3) * v.w();
    const float z = at(2, 0) * v.x() + at(2, 1) * v.y() + at(2, 2) * v.z() + at(2, 3) * v.w();
    const float w = at(3, 0) * v.x() + at(3, 1) * v.y() + at(3, 2) * v.z() + at(3, 3) * v.w();
    return QVector4D(x, y, z, w);
}

// ---------------------------------------------------------------------------
// Rasterizer internals.
// ---------------------------------------------------------------------------

namespace {

inline int clampToByte(double v) {
    if (v < 0.0)
        return 0;
    if (v > 255.0)
        return 255;
    return static_cast<int>(v + 0.5);
}

inline bool finiteVec4(const QVector4D &v) {
    return std::isfinite(v.x()) && std::isfinite(v.y()) && std::isfinite(v.z()) &&
           std::isfinite(v.w());
}

// Extract the upper-left 3x3 (rotation/scale part) of the model matrix and use
// it to transform a normal. For pure rotations this is exact; for the (rare)
// scaled case it is an acceptable approximation for shading purposes.
inline QVector3D transformNormal(const Mat4 &model, const QVector3D &n) {
    const float x = model.at(0, 0) * n.x() + model.at(0, 1) * n.y() + model.at(0, 2) * n.z();
    const float y = model.at(1, 0) * n.x() + model.at(1, 1) * n.y() + model.at(1, 2) * n.z();
    const float z = model.at(2, 0) * n.x() + model.at(2, 1) * n.y() + model.at(2, 2) * n.z();
    return QVector3D(x, y, z);
}

// 2D edge function (signed area * 2 of the triangle a,b,c).
inline double edge(double ax, double ay, double bx, double by, double cx, double cy) {
    return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
}

// Top-left fill rule (for our screen winding: front-facing triangles have a
// POSITIVE signed area, so a "left" edge is walked downward in Y-down screen
// space). An edge is "top-left" — and therefore keeps pixels lying exactly on
// it — if it is a top edge (horizontal, going left: dy == 0 && dx < 0) or a
// left edge (going downward: dy > 0). Every other edge nudges on-edge pixels
// out, so exactly one of two triangles sharing an edge wins each edge pixel.
inline bool isTopLeft(double dx, double dy) {
    return (dy > 0.0) || (dy == 0.0 && dx < 0.0);
}

inline quint32 packArgb(int a, int r, int g, int b) {
    return (static_cast<quint32>(a) << 24) | (static_cast<quint32>(r) << 16) |
           (static_cast<quint32>(g) << 8) | static_cast<quint32>(b);
}

struct ScreenVertex {
    double sx = 0.0;       // screen x (pixels, sub-pixel)
    double sy = 0.0;       // screen y (pixels, sub-pixel)
    double ndcZ = 0.0;     // depth in NDC [-1,1]
    QVector3D worldNormal; // model-rotated vertex normal (for shading)
};

void drawLineArgb(QImage &img, int x0, int y0, int x1, int y1, quint32 argb) {
    const int w = img.width();
    const int h = img.height();
    // Clamp endpoints to a small margin around the image so the iteration count
    // is bounded by ~max(w,h) even for wildly out-of-frame projected vertices.
    // (On-screen segments are unaffected by this clamp.)
    const auto clampTo = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };
    x0 = clampTo(x0, -2, w + 1);
    x1 = clampTo(x1, -2, w + 1);
    y0 = clampTo(y0, -2, h + 1);
    y1 = clampTo(y1, -2, h + 1);
    int dx = std::abs(x1 - x0);
    int dy = -std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        if (x0 >= 0 && x0 < w && y0 >= 0 && y0 < h) {
            reinterpret_cast<quint32 *>(img.scanLine(y0))[x0] = argb;
        }
        if (x0 == x1 && y0 == y1)
            break;
        const int e2 = 2 * err;
        if (e2 >= dy) {
            if (x0 == x1)
                break;
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            if (y0 == y1)
                break;
            err += dx;
            y0 += sy;
        }
    }
}

} // namespace

// ---------------------------------------------------------------------------
// render
// ---------------------------------------------------------------------------

QImage render(const mesh3d::TriMesh &mesh, const Mat4 &model, const Mat4 &view,
              const Mat4 &proj, QSize outSize, const RenderParams &p) {
    int w = outSize.width();
    int h = outSize.height();
    if (w < 1)
        w = 1;
    if (h < 1)
        h = 1;

    QImage img(w, h, QImage::Format_ARGB32);
    const quint32 bgArgb = packArgb(p.background.alpha(), p.background.red(),
                                    p.background.green(), p.background.blue());
    img.fill(bgArgb);

    // Per-pixel depth buffer (NDC z; smaller == nearer). Init to +inf.
    const qsizetype pixCount = static_cast<qsizetype>(w) * static_cast<qsizetype>(h);
    QVector<float> zbuf(pixCount, std::numeric_limits<float>::infinity());

    const Mat4 mvp = proj * (view * model);

    // Light direction (toward-surface-from-light is -lightDir).
    QVector3D L = -p.lightDir;
    if (L.lengthSquared() <= 0.0f)
        L = QVector3D(0.0f, 0.0f, 1.0f);
    L.normalize();

    const double frontR = p.frontColor.red();
    const double frontG = p.frontColor.green();
    const double frontB = p.frontColor.blue();
    const double sideR = p.sideColor.red();
    const double sideG = p.sideColor.green();
    const double sideB = p.sideColor.blue();
    const double ambR = p.ambient.red();
    const double ambG = p.ambient.green();
    const double ambB = p.ambient.blue();

    const qsizetype triCount = mesh.indices.size() / 3;
    const qsizetype posCount = mesh.positions.size();
    const qsizetype nrmCount = mesh.normals.size();

    for (qsizetype t = 0; t < triCount; ++t) {
        const quint32 i0 = mesh.indices[t * 3 + 0];
        const quint32 i1 = mesh.indices[t * 3 + 1];
        const quint32 i2 = mesh.indices[t * 3 + 2];
        if (static_cast<qsizetype>(i0) >= posCount || static_cast<qsizetype>(i1) >= posCount ||
            static_cast<qsizetype>(i2) >= posCount)
            continue;

        ScreenVertex sv[3];
        const quint32 idx[3] = {i0, i1, i2};
        bool triOk = true;
        for (int k = 0; k < 3; ++k) {
            const QVector3D &pos = mesh.positions[static_cast<qsizetype>(idx[k])];
            const QVector4D clip = mvp * QVector4D(pos.x(), pos.y(), pos.z(), 1.0f);
            // Reject if behind / on the camera plane (no near-plane clipping —
            // a vertex with w <= 0 would produce a flipped/garbage projection).
            if (!finiteVec4(clip) || clip.w() <= 0.0f) {
                triOk = false;
                break;
            }
            const float invW = 1.0f / clip.w();
            const float ndcX = clip.x() * invW;
            const float ndcY = clip.y() * invW;
            const float ndcZ = clip.z() * invW;
            if (!std::isfinite(ndcX) || !std::isfinite(ndcY) || !std::isfinite(ndcZ)) {
                triOk = false;
                break;
            }
            sv[k].sx = (static_cast<double>(ndcX) * 0.5 + 0.5) * static_cast<double>(w);
            sv[k].sy = (1.0 - (static_cast<double>(ndcY) * 0.5 + 0.5)) * static_cast<double>(h);
            sv[k].ndcZ = static_cast<double>(ndcZ);
            const QVector3D nrm = (static_cast<qsizetype>(idx[k]) < nrmCount)
                                      ? mesh.normals[static_cast<qsizetype>(idx[k])]
                                      : QVector3D(0.0f, 0.0f, 1.0f);
            sv[k].worldNormal = transformNormal(model, nrm);
        }
        if (!triOk)
            continue;

        // Signed area * 2 of the triangle in screen pixel space.
        //   - Model-space CCW (front-facing, +Z normal) vertices, after the
        //     Y-flip done in the screen mapping, come out with a POSITIVE value
        //     here. So area2 > 0  <=>  front-facing.  area2 < 0  <=>  back-facing.
        //   - Interior points then have all three edge functions >= 0.
        const double area2 = edge(sv[0].sx, sv[0].sy, sv[1].sx, sv[1].sy, sv[2].sx, sv[2].sy);
        if (!std::isfinite(area2) || area2 == 0.0)
            continue;
        const bool frontFacing = (area2 > 0.0);
        if (!p.wireframe && !frontFacing)
            continue; // backface cull (always on for solid fill)

        // ---- wireframe path ----
        if (p.wireframe) {
            // Color: a flat shade based on the (model-space) face normal of vtx0.
            // Choose front vs side by which axis dominates the model-space normal.
            const QVector3D mn0 = (static_cast<qsizetype>(i0) < nrmCount)
                                      ? mesh.normals[static_cast<qsizetype>(i0)]
                                      : QVector3D(0.0f, 0.0f, 1.0f);
            const double az = std::fabs(mn0.z());
            const double axy = std::max(std::fabs(mn0.x()), std::fabs(mn0.y()));
            const bool cap = (az >= axy);
            const int rr = cap ? p.frontColor.red() : p.sideColor.red();
            const int gg = cap ? p.frontColor.green() : p.sideColor.green();
            const int bb = cap ? p.frontColor.blue() : p.sideColor.blue();
            const quint32 lineArgb = packArgb(255, rr, gg, bb);
            // Clamp to a generous range before the int cast so it can never
            // overflow (Windows 'long'/'int' are 32-bit); drawLineArgb clamps
            // again to the image margin.
            const double lo = -1.0e6, hi = 1.0e6;
            const auto toI = [lo, hi](double v) {
                if (v < lo)
                    v = lo;
                if (v > hi)
                    v = hi;
                return static_cast<int>(std::lround(v));
            };
            const int x0 = toI(sv[0].sx);
            const int y0 = toI(sv[0].sy);
            const int x1 = toI(sv[1].sx);
            const int y1 = toI(sv[1].sy);
            const int x2 = toI(sv[2].sx);
            const int y2 = toI(sv[2].sy);
            drawLineArgb(img, x0, y0, x1, y1, lineArgb);
            drawLineArgb(img, x1, y1, x2, y2, lineArgb);
            drawLineArgb(img, x2, y2, x0, y0, lineArgb);
            continue;
        }

        // ---- solid raster path ----
        // Per-vertex base color selection (front/back caps vs side walls):
        //   heuristic — if |normal.z| dominates |normal.x| and |normal.y| in
        //   MODEL space, the vertex belongs to a cap (front/back) -> frontColor;
        //   otherwise it is a side wall -> sideColor. We pick the triangle's
        //   color from vertex 0 (caps/walls are not mixed within one triangle
        //   for the extruded meshes this targets).
        const QVector3D mn0 = (static_cast<qsizetype>(i0) < nrmCount)
                                  ? mesh.normals[static_cast<qsizetype>(i0)]
                                  : QVector3D(0.0f, 0.0f, 1.0f);
        const double az0 = std::fabs(mn0.z());
        const double axy0 = std::max(std::fabs(mn0.x()), std::fabs(mn0.y()));
        const bool isCap = (az0 >= axy0);
        const double baseR = isCap ? frontR : sideR;
        const double baseG = isCap ? frontG : sideG;
        const double baseB = isCap ? frontB : sideB;

        // Screen-space bounding box. Clamp as doubles to [0,w] / [0,h] FIRST so
        // the subsequent int casts can never overflow on a wildly out-of-frame
        // (but still finite) projected vertex.
        double minXd = std::min({sv[0].sx, sv[1].sx, sv[2].sx});
        double maxXd = std::max({sv[0].sx, sv[1].sx, sv[2].sx});
        double minYd = std::min({sv[0].sy, sv[1].sy, sv[2].sy});
        double maxYd = std::max({sv[0].sy, sv[1].sy, sv[2].sy});
        const double wd = static_cast<double>(w);
        const double hd = static_cast<double>(h);
        minXd = std::min(std::max(minXd, 0.0), wd);
        maxXd = std::min(std::max(maxXd, 0.0), wd);
        minYd = std::min(std::max(minYd, 0.0), hd);
        maxYd = std::min(std::max(maxYd, 0.0), hd);
        int minX = static_cast<int>(std::floor(minXd));
        int maxX = static_cast<int>(std::ceil(maxXd));
        int minY = static_cast<int>(std::floor(minYd));
        int maxY = static_cast<int>(std::ceil(maxYd));
        if (minX < 0)
            minX = 0;
        if (minY < 0)
            minY = 0;
        if (maxX > w - 1)
            maxX = w - 1;
        if (maxY > h - 1)
            maxY = h - 1;
        if (minX > maxX || minY > maxY)
            continue;

        // Front-facing triangle: area2 > 0, so interior edge-function values are
        // all >= 0. (Back-facing tris were already culled above.)
        const double invArea2 = 1.0 / area2;

        // Top-left fill rule. Edge i is opposite vertex i:
        //   edge0: v1 -> v2 ; edge1: v2 -> v0 ; edge2: v0 -> v1.
        // A pixel exactly on an edge is kept iff that edge is a "top" edge
        // (horizontal, pointing left) or a "left" edge (pointing up); on every
        // other edge it is nudged out by an epsilon so exactly one of two
        // triangles sharing the edge wins it.
        const double dx0 = sv[2].sx - sv[1].sx, dy0 = sv[2].sy - sv[1].sy;
        const double dx1 = sv[0].sx - sv[2].sx, dy1 = sv[0].sy - sv[2].sy;
        const double dx2 = sv[1].sx - sv[0].sx, dy2 = sv[1].sy - sv[0].sy;
        const double eps = 1e-7;
        const double bias0 = isTopLeft(dx0, dy0) ? 0.0 : -eps;
        const double bias1 = isTopLeft(dx1, dy1) ? 0.0 : -eps;
        const double bias2 = isTopLeft(dx2, dy2) ? 0.0 : -eps;

        for (int py = minY; py <= maxY; ++py) {
            quint32 *scan = reinterpret_cast<quint32 *>(img.scanLine(py));
            const double sampleY = static_cast<double>(py) + 0.5;
            for (int px = minX; px <= maxX; ++px) {
                const double sampleX = static_cast<double>(px) + 0.5;

                const double w0 = edge(sv[1].sx, sv[1].sy, sv[2].sx, sv[2].sy, sampleX, sampleY);
                const double w1 = edge(sv[2].sx, sv[2].sy, sv[0].sx, sv[0].sy, sampleX, sampleY);
                const double w2 = edge(sv[0].sx, sv[0].sy, sv[1].sx, sv[1].sy, sampleX, sampleY);

                // Front-facing screen triangle -> interior has all w* >= 0.
                if (!(w0 + bias0 >= 0.0 && w1 + bias1 >= 0.0 && w2 + bias2 >= 0.0))
                    continue;

                // Barycentrics (normalized by total signed area).
                const double b0 = w0 * invArea2;
                const double b1 = w1 * invArea2;
                const double b2 = w2 * invArea2;

                const double depth = b0 * sv[0].ndcZ + b1 * sv[1].ndcZ + b2 * sv[2].ndcZ;
                if (!std::isfinite(depth))
                    continue;
                // Discard fragments outside the [-1,1] NDC depth slab.
                if (depth < -1.0 || depth > 1.0)
                    continue;

                const qsizetype pi = static_cast<qsizetype>(py) * static_cast<qsizetype>(w) +
                                     static_cast<qsizetype>(px);
                if (static_cast<float>(depth) >= zbuf[pi])
                    continue; // something nearer already there
                zbuf[pi] = static_cast<float>(depth);

                // Interpolated, model-rotated normal -> Lambert term.
                QVector3D nrm = static_cast<float>(b0) * sv[0].worldNormal +
                                static_cast<float>(b1) * sv[1].worldNormal +
                                static_cast<float>(b2) * sv[2].worldNormal;
                double nl = 0.0;
                const float nLen2 = nrm.lengthSquared();
                if (nLen2 > 1e-12f) {
                    nrm /= std::sqrt(nLen2);
                    nl = static_cast<double>(QVector3D::dotProduct(nrm, L));
                    if (nl < 0.0)
                        nl = 0.0;
                }

                const int rr = clampToByte(ambR + nl * baseR);
                const int gg = clampToByte(ambG + nl * baseG);
                const int bb = clampToByte(ambB + nl * baseB);
                scan[px] = packArgb(255, rr, gg, bb);
            }
        }
    }

    return img;
}

// ---------------------------------------------------------------------------
// renderMeshAuto
// ---------------------------------------------------------------------------

QImage renderMeshAuto(const mesh3d::TriMesh &mesh, QSize outSize, double yawDeg,
                      double pitchDeg, double cameraDistance, const RenderParams &p) {
    int w = outSize.width();
    int h = outSize.height();
    if (w < 1)
        w = 1;
    if (h < 1)
        h = 1;
    const double aspect = static_cast<double>(w) / static_cast<double>(h);

    // Centroid + bounding radius of the mesh.
    QVector3D centroid(0.0f, 0.0f, 0.0f);
    const qsizetype n = mesh.positions.size();
    if (n > 0) {
        for (qsizetype i = 0; i < n; ++i)
            centroid += mesh.positions[i];
        centroid /= static_cast<float>(n);
    }
    float radius = 0.0f;
    for (qsizetype i = 0; i < n; ++i) {
        const float d2 = (mesh.positions[i] - centroid).lengthSquared();
        if (d2 > radius)
            radius = d2;
    }
    radius = std::sqrt(radius);
    if (!std::isfinite(radius) || radius <= 0.0f)
        radius = 1.0f;

    double camDist = cameraDistance;
    if (!std::isfinite(camDist) || camDist <= 0.0)
        camDist = static_cast<double>(radius) * 3.0;

    const double zFar = std::max(10.0, camDist * 4.0);
    const Mat4 proj = Mat4::perspective(45.0, aspect, 0.05, zFar);

    // Orbit direction: rotate the -Z unit vector by yaw (around Y) then pitch
    // (around X), and place the eye that far from the centroid.
    const Mat4 orbit = Mat4::rotateY(yawDeg) * Mat4::rotateX(pitchDeg);
    const QVector4D dir4 = orbit * QVector4D(0.0f, 0.0f, -1.0f, 0.0f);
    QVector3D dir(dir4.x(), dir4.y(), dir4.z());
    if (dir.lengthSquared() <= 1e-12f)
        dir = QVector3D(0.0f, 0.0f, -1.0f);
    dir.normalize();

    const QVector3D eye = centroid - dir * static_cast<float>(camDist);
    const Mat4 view = Mat4::lookAt(eye, centroid, QVector3D(0.0f, 1.0f, 0.0f));
    const Mat4 model = Mat4::identity();

    return render(mesh, model, view, proj, QSize(w, h), p);
}

} // namespace softras
