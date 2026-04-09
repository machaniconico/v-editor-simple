#include "WarpDistortion.h"

#include <algorithm>
#include <cmath>

// ============================================================
//  Enum <-> String helpers (file-local)
// ============================================================

static QString warpTypeToString(WarpType t)
{
    switch (t) {
    case WarpType::MeshWarp:  return QStringLiteral("mesh_warp");
    case WarpType::PuppetPin: return QStringLiteral("puppet_pin");
    case WarpType::Bulge:     return QStringLiteral("bulge");
    case WarpType::Pinch:     return QStringLiteral("pinch");
    case WarpType::Twirl:     return QStringLiteral("twirl");
    case WarpType::Wave:      return QStringLiteral("wave");
    case WarpType::Ripple:    return QStringLiteral("ripple");
    case WarpType::Spherize:  return QStringLiteral("spherize");
    case WarpType::Fisheye:   return QStringLiteral("fisheye");
    }
    return QStringLiteral("mesh_warp");
}

static WarpType warpTypeFromString(const QString &s)
{
    if (s == QLatin1String("puppet_pin")) return WarpType::PuppetPin;
    if (s == QLatin1String("bulge"))      return WarpType::Bulge;
    if (s == QLatin1String("pinch"))      return WarpType::Pinch;
    if (s == QLatin1String("twirl"))      return WarpType::Twirl;
    if (s == QLatin1String("wave"))       return WarpType::Wave;
    if (s == QLatin1String("ripple"))     return WarpType::Ripple;
    if (s == QLatin1String("spherize"))   return WarpType::Spherize;
    if (s == QLatin1String("fisheye"))    return WarpType::Fisheye;
    return WarpType::MeshWarp;
}

// ============================================================
//  WarpPin serialisation
// ============================================================

QJsonObject WarpPin::toJson() const
{
    QJsonObject obj;
    QJsonObject orig;
    orig["x"] = originalPos.x();
    orig["y"] = originalPos.y();
    obj["originalPos"] = orig;

    QJsonObject def;
    def["x"] = deformedPos.x();
    def["y"] = deformedPos.y();
    obj["deformedPos"] = def;

    obj["radius"]    = radius;
    obj["stiffness"] = stiffness;
    return obj;
}

WarpPin WarpPin::fromJson(const QJsonObject &obj)
{
    WarpPin pin;
    QJsonObject orig = obj["originalPos"].toObject();
    pin.originalPos = QPointF(orig["x"].toDouble(), orig["y"].toDouble());

    QJsonObject def = obj["deformedPos"].toObject();
    pin.deformedPos = QPointF(def["x"].toDouble(), def["y"].toDouble());

    pin.radius    = obj["radius"].toDouble(100.0);
    pin.stiffness = obj["stiffness"].toDouble(0.5);
    return pin;
}

// ============================================================
//  MeshGrid serialisation
// ============================================================

QJsonObject MeshGrid::toJson() const
{
    QJsonObject obj;
    obj["rows"] = rows;
    obj["cols"] = cols;

    QJsonArray rowsArr;
    for (const auto &row : controlPoints) {
        QJsonArray colsArr;
        for (const auto &pt : row) {
            QJsonObject p;
            p["x"] = pt.x();
            p["y"] = pt.y();
            colsArr.append(p);
        }
        rowsArr.append(colsArr);
    }
    obj["controlPoints"] = rowsArr;
    return obj;
}

MeshGrid MeshGrid::fromJson(const QJsonObject &obj)
{
    MeshGrid grid;
    grid.rows = obj["rows"].toInt(4);
    grid.cols = obj["cols"].toInt(4);

    QJsonArray rowsArr = obj["controlPoints"].toArray();
    grid.controlPoints.reserve(rowsArr.size());
    for (const auto &rowVal : rowsArr) {
        QJsonArray colsArr = rowVal.toArray();
        QVector<QPointF> row;
        row.reserve(colsArr.size());
        for (const auto &ptVal : colsArr) {
            QJsonObject p = ptVal.toObject();
            row.append(QPointF(p["x"].toDouble(), p["y"].toDouble()));
        }
        grid.controlPoints.append(row);
    }
    return grid;
}

// ============================================================
//  WarpConfig serialisation
// ============================================================

QJsonObject WarpConfig::toJson() const
{
    QJsonObject obj;
    obj["type"] = warpTypeToString(type);

    // MeshWarp
    obj["meshGrid"] = meshGrid.toJson();

    // PuppetPin
    QJsonArray pinsArr;
    for (const auto &pin : pins) {
        pinsArr.append(pin.toJson());
    }
    obj["pins"] = pinsArr;

    // Parametric
    QJsonObject c;
    c["x"] = center.x();
    c["y"] = center.y();
    obj["center"] = c;

    obj["radius"]    = radius;
    obj["amount"]    = amount;
    obj["angle"]     = angle;
    obj["amplitude"] = amplitude;
    obj["frequency"] = frequency;
    obj["phase"]     = phase;

    return obj;
}

WarpConfig WarpConfig::fromJson(const QJsonObject &obj)
{
    WarpConfig cfg;
    cfg.type = warpTypeFromString(obj["type"].toString());

    // MeshWarp
    cfg.meshGrid = MeshGrid::fromJson(obj["meshGrid"].toObject());

    // PuppetPin
    QJsonArray pinsArr = obj["pins"].toArray();
    cfg.pins.reserve(pinsArr.size());
    for (const auto &val : pinsArr) {
        cfg.pins.append(WarpPin::fromJson(val.toObject()));
    }

    // Parametric
    QJsonObject c = obj["center"].toObject();
    cfg.center = QPointF(c["x"].toDouble(0.5), c["y"].toDouble(0.5));

    cfg.radius    = obj["radius"].toDouble(0.5);
    cfg.amount    = obj["amount"].toDouble(0.5);
    cfg.angle     = obj["angle"].toDouble(90.0);
    cfg.amplitude = obj["amplitude"].toDouble(10.0);
    cfg.frequency = obj["frequency"].toDouble(0.05);
    cfg.phase     = obj["phase"].toDouble(0.0);

    return cfg;
}

// ============================================================
//  Bilinear sampling
// ============================================================

QRgb WarpDistortion::bilinearSample(const QImage &image, double x, double y)
{
    const int w = image.width();
    const int h = image.height();

    // Clamp to valid range
    x = std::clamp(x, 0.0, static_cast<double>(w - 1));
    y = std::clamp(y, 0.0, static_cast<double>(h - 1));

    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(x0 + 1, w - 1);
    const int y1 = std::min(y0 + 1, h - 1);

    const double fx = x - x0;
    const double fy = y - y0;

    const QRgb c00 = image.pixel(x0, y0);
    const QRgb c10 = image.pixel(x1, y0);
    const QRgb c01 = image.pixel(x0, y1);
    const QRgb c11 = image.pixel(x1, y1);

    auto lerp = [](double a, double b, double t) -> double {
        return a + (b - a) * t;
    };

    const int r = static_cast<int>(lerp(
        lerp(qRed(c00), qRed(c10), fx),
        lerp(qRed(c01), qRed(c11), fx), fy));
    const int g = static_cast<int>(lerp(
        lerp(qGreen(c00), qGreen(c10), fx),
        lerp(qGreen(c01), qGreen(c11), fx), fy));
    const int b = static_cast<int>(lerp(
        lerp(qBlue(c00), qBlue(c10), fx),
        lerp(qBlue(c01), qBlue(c11), fx), fy));
    const int a = static_cast<int>(lerp(
        lerp(qAlpha(c00), qAlpha(c10), fx),
        lerp(qAlpha(c01), qAlpha(c11), fx), fy));

    return qRgba(r, g, b, a);
}

// ============================================================
//  High-level dispatch
// ============================================================

QImage WarpDistortion::applyWarp(const QImage &image, const WarpConfig &config)
{
    if (image.isNull()) return image;

    const int w = image.width();
    const int h = image.height();

    switch (config.type) {
    case WarpType::MeshWarp:
        return applyMeshWarp(image, config.meshGrid);

    case WarpType::PuppetPin:
        return applyPuppetPin(image, config.pins);

    case WarpType::Bulge: {
        QPointF c(config.center.x() * w, config.center.y() * h);
        double r = config.radius * std::min(w, h);
        return applyBulge(image, c, r, config.amount);
    }
    case WarpType::Pinch: {
        QPointF c(config.center.x() * w, config.center.y() * h);
        double r = config.radius * std::min(w, h);
        return applyPinch(image, c, r, config.amount);
    }
    case WarpType::Twirl: {
        QPointF c(config.center.x() * w, config.center.y() * h);
        double r = config.radius * std::min(w, h);
        return applyTwirl(image, c, r, config.angle);
    }
    case WarpType::Wave:
        return applyWave(image, config.amplitude, config.frequency, config.phase);

    case WarpType::Ripple: {
        QPointF c(config.center.x() * w, config.center.y() * h);
        return applyRipple(image, c, config.amplitude, config.frequency);
    }
    case WarpType::Spherize: {
        QPointF c(config.center.x() * w, config.center.y() * h);
        double r = config.radius * std::min(w, h);
        return applySpherize(image, c, r, config.amount);
    }
    case WarpType::Fisheye: {
        QPointF c(config.center.x() * w, config.center.y() * h);
        double r = config.radius * std::min(w, h);
        return applyFisheye(image, c, r, config.amount);
    }
    }

    return image;
}

// ============================================================
//  MeshWarp
// ============================================================

QImage WarpDistortion::applyMeshWarp(const QImage &image, const MeshGrid &mesh)
{
    if (image.isNull()) return image;

    const int w = image.width();
    const int h = image.height();

    // Need at least 2x2 grid
    if (mesh.rows < 2 || mesh.cols < 2) return image;
    if (mesh.controlPoints.size() < mesh.rows) return image;

    QImage src = image.convertToFormat(QImage::Format_ARGB32);
    QImage dst(w, h, QImage::Format_ARGB32);
    dst.fill(Qt::transparent);

    // Cell size in original (uniform) space
    const double cellW = static_cast<double>(w) / (mesh.cols - 1);
    const double cellH = static_cast<double>(h) / (mesh.rows - 1);

    for (int py = 0; py < h; ++py) {
        auto *scanline = reinterpret_cast<QRgb *>(dst.scanLine(py));

        for (int px = 0; px < w; ++px) {
            // Which cell does this output pixel belong to?
            double gx = px / cellW;
            double gy = py / cellH;

            int ci = static_cast<int>(std::floor(gx));
            int ri = static_cast<int>(std::floor(gy));

            ci = std::clamp(ci, 0, mesh.cols - 2);
            ri = std::clamp(ri, 0, mesh.rows - 2);

            // Local coordinates within cell (0-1)
            double u = gx - ci;
            double v = gy - ri;

            // Four corners of the deformed cell
            const QPointF &p00 = mesh.controlPoints[ri][ci];
            const QPointF &p10 = mesh.controlPoints[ri][ci + 1];
            const QPointF &p01 = mesh.controlPoints[ri + 1][ci];
            const QPointF &p11 = mesh.controlPoints[ri + 1][ci + 1];

            // Bilinear interpolation within the deformed quad -> source coordinate
            double srcX = (1 - v) * ((1 - u) * p00.x() + u * p10.x())
                        + v       * ((1 - u) * p01.x() + u * p11.x());
            double srcY = (1 - v) * ((1 - u) * p00.y() + u * p10.y())
                        + v       * ((1 - u) * p01.y() + u * p11.y());

            if (srcX >= 0 && srcX < w && srcY >= 0 && srcY < h) {
                scanline[px] = bilinearSample(src, srcX, srcY);
            }
        }
    }

    return dst;
}

// ============================================================
//  PuppetPin
// ============================================================

QImage WarpDistortion::applyPuppetPin(const QImage &image, const QVector<WarpPin> &pins)
{
    if (image.isNull() || pins.isEmpty()) return image;

    const int w = image.width();
    const int h = image.height();

    QImage src = image.convertToFormat(QImage::Format_ARGB32);
    QImage dst(w, h, QImage::Format_ARGB32);
    dst.fill(Qt::transparent);

    for (int py = 0; py < h; ++py) {
        auto *scanline = reinterpret_cast<QRgb *>(dst.scanLine(py));

        for (int px = 0; px < w; ++px) {
            double totalWeight = 0.0;
            double dispX = 0.0;
            double dispY = 0.0;

            for (const auto &pin : pins) {
                double dx = px - pin.originalPos.x();
                double dy = py - pin.originalPos.y();
                double dist2 = dx * dx + dy * dy;

                // Outside influence radius — skip
                double r2 = pin.radius * pin.radius;
                if (dist2 > r2) continue;

                // Weight: inverse-square distance, scaled by stiffness
                double weight;
                if (dist2 < 1.0) {
                    weight = 1e6;  // very close to pin center
                } else {
                    weight = 1.0 / dist2;
                }

                // Smooth falloff at influence boundary
                double distNorm = std::sqrt(dist2) / pin.radius;
                double falloff = 1.0 - distNorm * distNorm;
                falloff = std::max(falloff, 0.0);
                weight *= falloff * (0.5 + 0.5 * pin.stiffness);

                double pinDispX = pin.deformedPos.x() - pin.originalPos.x();
                double pinDispY = pin.deformedPos.y() - pin.originalPos.y();

                dispX += weight * pinDispX;
                dispY += weight * pinDispY;
                totalWeight += weight;
            }

            double srcX = px;
            double srcY = py;

            if (totalWeight > 0.0) {
                srcX -= dispX / totalWeight;
                srcY -= dispY / totalWeight;
            }

            if (srcX >= 0 && srcX < w && srcY >= 0 && srcY < h) {
                scanline[px] = bilinearSample(src, srcX, srcY);
            }
        }
    }

    return dst;
}

// ============================================================
//  Bulge — radial outward displacement
// ============================================================

QImage WarpDistortion::applyBulge(const QImage &image, QPointF center,
                                  double radius, double amount)
{
    if (image.isNull()) return image;

    const int w = image.width();
    const int h = image.height();

    QImage src = image.convertToFormat(QImage::Format_ARGB32);
    QImage dst(w, h, QImage::Format_ARGB32);
    dst.fill(Qt::transparent);

    for (int py = 0; py < h; ++py) {
        auto *scanline = reinterpret_cast<QRgb *>(dst.scanLine(py));

        for (int px = 0; px < w; ++px) {
            double dx = px - center.x();
            double dy = py - center.y();
            double dist = std::sqrt(dx * dx + dy * dy);

            if (dist >= radius || dist < 0.001) {
                scanline[px] = src.pixel(px, py);
                continue;
            }

            // Bulge: remap source distance inward so output appears expanded
            double r = dist / radius;
            double scale = std::pow(r, amount - 1.0);
            double srcX = center.x() + dx * scale;
            double srcY = center.y() + dy * scale;

            scanline[px] = bilinearSample(src, srcX, srcY);
        }
    }

    return dst;
}

// ============================================================
//  Pinch — radial inward displacement
// ============================================================

QImage WarpDistortion::applyPinch(const QImage &image, QPointF center,
                                  double radius, double amount)
{
    if (image.isNull()) return image;

    const int w = image.width();
    const int h = image.height();

    QImage src = image.convertToFormat(QImage::Format_ARGB32);
    QImage dst(w, h, QImage::Format_ARGB32);
    dst.fill(Qt::transparent);

    for (int py = 0; py < h; ++py) {
        auto *scanline = reinterpret_cast<QRgb *>(dst.scanLine(py));

        for (int px = 0; px < w; ++px) {
            double dx = px - center.x();
            double dy = py - center.y();
            double dist = std::sqrt(dx * dx + dy * dy);

            if (dist >= radius || dist < 0.001) {
                scanline[px] = src.pixel(px, py);
                continue;
            }

            // Pinch: pull source outward (inverse of bulge)
            double r = dist / radius;
            double power = 1.0 / std::max(amount, 0.01);
            double srcDist = radius * std::pow(r, power);

            double srcX = center.x() + (dx / dist) * srcDist;
            double srcY = center.y() + (dy / dist) * srcDist;

            scanline[px] = bilinearSample(src, srcX, srcY);
        }
    }

    return dst;
}

// ============================================================
//  Twirl — rotation decreasing with distance from center
// ============================================================

QImage WarpDistortion::applyTwirl(const QImage &image, QPointF center,
                                  double radius, double angle)
{
    if (image.isNull()) return image;

    const int w = image.width();
    const int h = image.height();
    const double angleRad = angle * M_PI / 180.0;

    QImage src = image.convertToFormat(QImage::Format_ARGB32);
    QImage dst(w, h, QImage::Format_ARGB32);
    dst.fill(Qt::transparent);

    for (int py = 0; py < h; ++py) {
        auto *scanline = reinterpret_cast<QRgb *>(dst.scanLine(py));

        for (int px = 0; px < w; ++px) {
            double dx = px - center.x();
            double dy = py - center.y();
            double dist = std::sqrt(dx * dx + dy * dy);

            if (dist >= radius) {
                scanline[px] = src.pixel(px, py);
                continue;
            }

            // Twist angle falls off smoothly with distance
            double factor = 1.0 - (dist / radius);
            factor = factor * factor;  // quadratic falloff
            double twist = angleRad * factor;

            double cosT = std::cos(twist);
            double sinT = std::sin(twist);

            double srcX = center.x() + dx * cosT - dy * sinT;
            double srcY = center.y() + dx * sinT + dy * cosT;

            scanline[px] = bilinearSample(src, srcX, srcY);
        }
    }

    return dst;
}

// ============================================================
//  Wave — sinusoidal Y offset based on X
// ============================================================

QImage WarpDistortion::applyWave(const QImage &image, double amplitude,
                                 double frequency, double phase)
{
    if (image.isNull()) return image;

    const int w = image.width();
    const int h = image.height();

    QImage src = image.convertToFormat(QImage::Format_ARGB32);
    QImage dst(w, h, QImage::Format_ARGB32);
    dst.fill(Qt::transparent);

    for (int py = 0; py < h; ++py) {
        auto *scanline = reinterpret_cast<QRgb *>(dst.scanLine(py));

        for (int px = 0; px < w; ++px) {
            double srcX = px;
            double srcY = py - amplitude * std::sin(px * frequency + phase);

            if (srcX >= 0 && srcX < w && srcY >= 0 && srcY < h) {
                scanline[px] = bilinearSample(src, srcX, srcY);
            }
        }
    }

    return dst;
}

// ============================================================
//  Ripple — radial sinusoidal displacement from center
// ============================================================

QImage WarpDistortion::applyRipple(const QImage &image, QPointF center,
                                   double amplitude, double frequency)
{
    if (image.isNull()) return image;

    const int w = image.width();
    const int h = image.height();

    QImage src = image.convertToFormat(QImage::Format_ARGB32);
    QImage dst(w, h, QImage::Format_ARGB32);
    dst.fill(Qt::transparent);

    for (int py = 0; py < h; ++py) {
        auto *scanline = reinterpret_cast<QRgb *>(dst.scanLine(py));

        for (int px = 0; px < w; ++px) {
            double dx = px - center.x();
            double dy = py - center.y();
            double dist = std::sqrt(dx * dx + dy * dy);

            if (dist < 0.001) {
                scanline[px] = src.pixel(px, py);
                continue;
            }

            double offset = amplitude * std::sin(dist * frequency);
            double srcX = px + (dx / dist) * offset;
            double srcY = py + (dy / dist) * offset;

            if (srcX >= 0 && srcX < w && srcY >= 0 && srcY < h) {
                scanline[px] = bilinearSample(src, srcX, srcY);
            }
        }
    }

    return dst;
}

// ============================================================
//  Spherize — spherical mapping within radius
// ============================================================

QImage WarpDistortion::applySpherize(const QImage &image, QPointF center,
                                     double radius, double amount)
{
    if (image.isNull()) return image;

    const int w = image.width();
    const int h = image.height();

    QImage src = image.convertToFormat(QImage::Format_ARGB32);
    QImage dst(w, h, QImage::Format_ARGB32);
    dst.fill(Qt::transparent);

    for (int py = 0; py < h; ++py) {
        auto *scanline = reinterpret_cast<QRgb *>(dst.scanLine(py));

        for (int px = 0; px < w; ++px) {
            double dx = px - center.x();
            double dy = py - center.y();
            double dist = std::sqrt(dx * dx + dy * dy);

            if (dist >= radius) {
                scanline[px] = src.pixel(px, py);
                continue;
            }

            // Normalized distance
            double rn = dist / radius;

            // Spherize: map flat disk to sphere surface
            double newR = rn;
            if (rn > 0.0) {
                // Apply spherical refraction
                double theta = std::asin(std::clamp(rn, 0.0, 1.0));
                newR = theta / (M_PI / 2.0);
                // Blend between original and spherized based on amount
                newR = rn + (newR - rn) * amount;
            }

            double scale = (dist > 0.001) ? (newR * radius / dist) : 1.0;
            double srcX = center.x() + dx * scale;
            double srcY = center.y() + dy * scale;

            scanline[px] = bilinearSample(src, srcX, srcY);
        }
    }

    return dst;
}

// ============================================================
//  Fisheye — barrel distortion within radius
// ============================================================

QImage WarpDistortion::applyFisheye(const QImage &image, QPointF center,
                                    double radius, double amount)
{
    if (image.isNull()) return image;

    const int w = image.width();
    const int h = image.height();

    QImage src = image.convertToFormat(QImage::Format_ARGB32);
    QImage dst(w, h, QImage::Format_ARGB32);
    dst.fill(Qt::transparent);

    for (int py = 0; py < h; ++py) {
        auto *scanline = reinterpret_cast<QRgb *>(dst.scanLine(py));

        for (int px = 0; px < w; ++px) {
            double dx = px - center.x();
            double dy = py - center.y();
            double dist = std::sqrt(dx * dx + dy * dy);

            if (dist >= radius) {
                scanline[px] = src.pixel(px, py);
                continue;
            }

            // Fisheye: strong barrel distortion
            double rn = dist / radius;
            double power = std::max(amount, 0.01);
            double newR = std::pow(rn, power) * radius;

            double srcX, srcY;
            if (dist > 0.001) {
                srcX = center.x() + (dx / dist) * newR;
                srcY = center.y() + (dy / dist) * newR;
            } else {
                srcX = center.x();
                srcY = center.y();
            }

            scanline[px] = bilinearSample(src, srcX, srcY);
        }
    }

    return dst;
}

// ============================================================
//  Utility: default mesh
// ============================================================

MeshGrid WarpDistortion::createDefaultMesh(const QSize &imageSize, int rows, int cols)
{
    MeshGrid grid;
    grid.rows = std::max(rows, 2);
    grid.cols = std::max(cols, 2);
    grid.controlPoints.resize(grid.rows);

    for (int r = 0; r < grid.rows; ++r) {
        grid.controlPoints[r].resize(grid.cols);
        for (int c = 0; c < grid.cols; ++c) {
            double x = static_cast<double>(c) / (grid.cols - 1) * imageSize.width();
            double y = static_cast<double>(r) / (grid.rows - 1) * imageSize.height();
            grid.controlPoints[r][c] = QPointF(x, y);
        }
    }

    return grid;
}
