#include "EffectPreset.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>

// ===== EffectPreset serialisation =====

QJsonObject EffectPreset::toJson() const
{
    QJsonObject obj;
    obj["name"]        = name;
    obj["description"] = description;
    obj["category"]    = category;
    obj["author"]      = author;
    obj["isBuiltIn"]   = isBuiltIn;
    obj["createdAt"]   = createdAt.toString(Qt::ISODate);
    obj["modifiedAt"]  = modifiedAt.toString(Qt::ISODate);

    obj["colorCorrection"] = PresetLibrary::instance().colorCorrectionToJson(colorCorrection);

    QJsonArray arr;
    for (const auto &e : effects)
        arr.append(PresetLibrary::instance().videoEffectToJson(e));
    obj["effects"] = arr;

    return obj;
}

EffectPreset EffectPreset::fromJson(const QJsonObject &obj)
{
    EffectPreset p;
    p.name        = obj["name"].toString();
    p.description = obj["description"].toString();
    p.category    = obj["category"].toString();
    p.author      = obj["author"].toString();
    p.isBuiltIn   = obj["isBuiltIn"].toBool(false);
    p.createdAt   = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
    p.modifiedAt  = QDateTime::fromString(obj["modifiedAt"].toString(), Qt::ISODate);

    p.colorCorrection = PresetLibrary::instance().colorCorrectionFromJson(
        obj["colorCorrection"].toObject());

    const QJsonArray arr = obj["effects"].toArray();
    for (const auto &v : arr)
        p.effects.append(PresetLibrary::instance().videoEffectFromJson(v.toObject()));

    return p;
}

// ===== PresetLibrary =====

PresetLibrary &PresetLibrary::instance()
{
    static PresetLibrary lib;
    return lib;
}

PresetLibrary::PresetLibrary()
{
    registerBuiltins();
    loadLibrary();   // merge any user presets saved on disk
}

// --- Query ---

QVector<EffectPreset> PresetLibrary::allPresets() const
{
    return m_presets;
}

QVector<EffectPreset> PresetLibrary::presetsByCategory(const QString &category) const
{
    QVector<EffectPreset> result;
    for (const auto &p : m_presets)
        if (p.category == category) result.append(p);
    return result;
}

EffectPreset PresetLibrary::findByName(const QString &name) const
{
    for (const auto &p : m_presets)
        if (p.name == name) return p;
    return EffectPreset{};
}

QStringList PresetLibrary::categories() const
{
    QStringList cats;
    for (const auto &p : m_presets)
        if (!cats.contains(p.category)) cats.append(p.category);
    return cats;
}

// --- Mutate ---

bool PresetLibrary::addPreset(const EffectPreset &preset)
{
    // Reject duplicates
    for (const auto &p : m_presets)
        if (p.name == preset.name) return false;

    m_presets.append(preset);
    return true;
}

bool PresetLibrary::removePreset(const QString &name)
{
    for (int i = 0; i < m_presets.size(); ++i) {
        if (m_presets[i].name == name) {
            if (m_presets[i].isBuiltIn) return false;   // cannot remove factory presets
            m_presets.removeAt(i);
            return true;
        }
    }
    return false;
}

bool PresetLibrary::updatePreset(const QString &name, const EffectPreset &preset)
{
    for (auto &p : m_presets) {
        if (p.name == name) {
            if (p.isBuiltIn) return false;   // cannot modify factory presets
            p = preset;
            p.modifiedAt = QDateTime::currentDateTime();
            return true;
        }
    }
    return false;
}

// --- Import / Export ---

bool PresetLibrary::importPreset(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError) return false;

    EffectPreset preset = EffectPreset::fromJson(doc.object());
    if (preset.name.isEmpty()) return false;

    preset.isBuiltIn = false;
    return addPreset(preset);
}

bool PresetLibrary::exportPreset(const QString &name, const QString &filePath) const
{
    EffectPreset preset = findByName(name);
    if (preset.name.isEmpty()) return false;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;

    QJsonDocument doc(preset.toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

// --- Persist library ---

QString PresetLibrary::libraryPath()
{
    QString dir = QDir::homePath() + "/.veditor";
    QDir().mkpath(dir);
    return dir + "/presets.json";
}

bool PresetLibrary::saveLibrary() const
{
    QJsonArray arr;
    for (const auto &p : m_presets)
        arr.append(p.toJson());

    QFile file(libraryPath());
    if (!file.open(QIODevice::WriteOnly)) return false;

    QJsonDocument doc(arr);
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

bool PresetLibrary::loadLibrary()
{
    QFile file(libraryPath());
    if (!file.exists()) return true;       // first run — nothing to load
    if (!file.open(QIODevice::ReadOnly)) return false;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError) return false;

    const QJsonArray arr = doc.array();
    for (const auto &v : arr) {
        EffectPreset preset = EffectPreset::fromJson(v.toObject());
        if (preset.name.isEmpty()) continue;

        // Skip if a built-in with the same name already exists
        bool exists = false;
        for (const auto &p : m_presets) {
            if (p.name == preset.name) { exists = true; break; }
        }
        if (!exists)
            m_presets.append(preset);
    }
    return true;
}

// --- Apply ---

QPair<ColorCorrection, QVector<VideoEffect>>
PresetLibrary::applyPreset(const QString &presetName) const
{
    EffectPreset preset = findByName(presetName);
    return { preset.colorCorrection, preset.effects };
}

// ===== JSON helpers =====

QJsonObject PresetLibrary::colorCorrectionToJson(const ColorCorrection &cc)
{
    QJsonObject obj;
    obj["brightness"]  = cc.brightness;
    obj["contrast"]    = cc.contrast;
    obj["saturation"]  = cc.saturation;
    obj["hue"]         = cc.hue;
    obj["temperature"] = cc.temperature;
    obj["tint"]        = cc.tint;
    obj["gamma"]       = cc.gamma;
    obj["highlights"]  = cc.highlights;
    obj["shadows"]     = cc.shadows;
    obj["exposure"]    = cc.exposure;
    return obj;
}

ColorCorrection PresetLibrary::colorCorrectionFromJson(const QJsonObject &obj)
{
    ColorCorrection cc;
    cc.brightness  = obj["brightness"].toDouble(0.0);
    cc.contrast    = obj["contrast"].toDouble(0.0);
    cc.saturation  = obj["saturation"].toDouble(0.0);
    cc.hue         = obj["hue"].toDouble(0.0);
    cc.temperature = obj["temperature"].toDouble(0.0);
    cc.tint        = obj["tint"].toDouble(0.0);
    cc.gamma       = obj["gamma"].toDouble(1.0);
    cc.highlights  = obj["highlights"].toDouble(0.0);
    cc.shadows     = obj["shadows"].toDouble(0.0);
    cc.exposure    = obj["exposure"].toDouble(0.0);
    return cc;
}

QJsonObject PresetLibrary::videoEffectToJson(const VideoEffect &ve)
{
    QJsonObject obj;
    obj["type"]    = VideoEffect::typeName(ve.type);
    obj["enabled"] = ve.enabled;
    obj["param1"]  = ve.param1;
    obj["param2"]  = ve.param2;
    obj["param3"]  = ve.param3;
    if (ve.type == VideoEffectType::ChromaKey) {
        obj["keyColorR"] = ve.keyColor.red();
        obj["keyColorG"] = ve.keyColor.green();
        obj["keyColorB"] = ve.keyColor.blue();
    }
    return obj;
}

VideoEffect PresetLibrary::videoEffectFromJson(const QJsonObject &obj)
{
    VideoEffect ve;
    ve.enabled = obj["enabled"].toBool(true);
    ve.param1  = obj["param1"].toDouble(0.0);
    ve.param2  = obj["param2"].toDouble(0.0);
    ve.param3  = obj["param3"].toDouble(0.0);

    // Map type name back to enum
    QString typeName = obj["type"].toString();
    if      (typeName == "Blur")      ve.type = VideoEffectType::Blur;
    else if (typeName == "Sharpen")   ve.type = VideoEffectType::Sharpen;
    else if (typeName == "Mosaic")    ve.type = VideoEffectType::Mosaic;
    else if (typeName == "ChromaKey") ve.type = VideoEffectType::ChromaKey;
    else if (typeName == "Vignette")  ve.type = VideoEffectType::Vignette;
    else if (typeName == "Sepia")     ve.type = VideoEffectType::Sepia;
    else if (typeName == "Grayscale") ve.type = VideoEffectType::Grayscale;
    else if (typeName == "Invert")    ve.type = VideoEffectType::Invert;
    else if (typeName == "Noise")     ve.type = VideoEffectType::Noise;
    else                              ve.type = VideoEffectType::None;

    if (ve.type == VideoEffectType::ChromaKey) {
        ve.keyColor = QColor(
            obj["keyColorR"].toInt(0),
            obj["keyColorG"].toInt(255),
            obj["keyColorB"].toInt(0));
    }
    return ve;
}

// ===== Built-in presets =====

void PresetLibrary::registerBuiltins()
{
    QDateTime now = QDateTime::currentDateTime();

    // --- Cinematic Warm ---
    {
        EffectPreset p;
        p.name        = "Cinematic Warm";
        p.description = "Warm temperature with slight contrast boost and vignette";
        p.category    = "Cinematic";
        p.author      = "v-editor";
        p.isBuiltIn   = true;
        p.createdAt   = now;
        p.modifiedAt  = now;

        p.colorCorrection.temperature = 30.0;
        p.colorCorrection.contrast    = 10.0;
        p.colorCorrection.saturation  = 5.0;
        p.colorCorrection.shadows     = -10.0;

        p.effects.append(VideoEffect::createVignette(0.3, 0.85));

        m_presets.append(p);
    }

    // --- Film Noir ---
    {
        EffectPreset p;
        p.name        = "Film Noir";
        p.description = "Classic grayscale with high contrast and vignette";
        p.category    = "Black & White";
        p.author      = "v-editor";
        p.isBuiltIn   = true;
        p.createdAt   = now;
        p.modifiedAt  = now;

        p.colorCorrection.contrast   = 40.0;
        p.colorCorrection.brightness = -5.0;
        p.colorCorrection.shadows    = -20.0;

        p.effects.append(VideoEffect::createGrayscale());
        p.effects.append(VideoEffect::createVignette(0.5, 0.75));

        m_presets.append(p);
    }

    // --- Vintage ---
    {
        EffectPreset p;
        p.name        = "Vintage";
        p.description = "Sepia tones with reduced saturation and slight warm shift";
        p.category    = "Vintage";
        p.author      = "v-editor";
        p.isBuiltIn   = true;
        p.createdAt   = now;
        p.modifiedAt  = now;

        p.colorCorrection.saturation  = -30.0;
        p.colorCorrection.temperature = 15.0;
        p.colorCorrection.contrast    = 5.0;
        p.colorCorrection.gamma       = 1.1;

        p.effects.append(VideoEffect::createSepia(0.6));

        m_presets.append(p);
    }

    // --- Cool Tone ---
    {
        EffectPreset p;
        p.name        = "Cool Tone";
        p.description = "Cool temperature with slight blue tint and sharpen";
        p.category    = "Color";
        p.author      = "v-editor";
        p.isBuiltIn   = true;
        p.createdAt   = now;
        p.modifiedAt  = now;

        p.colorCorrection.temperature = -25.0;
        p.colorCorrection.tint        = -10.0;
        p.colorCorrection.contrast    = 5.0;
        p.colorCorrection.saturation  = 10.0;

        p.effects.append(VideoEffect::createSharpen(1.0));

        m_presets.append(p);
    }

    // --- HDR Look ---
    {
        EffectPreset p;
        p.name        = "HDR Look";
        p.description = "Boosted highlights, shadows, saturation and contrast for an HDR look";
        p.category    = "Cinematic";
        p.author      = "v-editor";
        p.isBuiltIn   = true;
        p.createdAt   = now;
        p.modifiedAt  = now;

        p.colorCorrection.highlights = 30.0;
        p.colorCorrection.shadows    = 30.0;
        p.colorCorrection.saturation = 25.0;
        p.colorCorrection.contrast   = 20.0;
        p.colorCorrection.exposure   = 0.2;

        m_presets.append(p);
    }

    // --- Dreamy ---
    {
        EffectPreset p;
        p.name        = "Dreamy";
        p.description = "Soft glow with warm tint and reduced contrast";
        p.category    = "Stylize";
        p.author      = "v-editor";
        p.isBuiltIn   = true;
        p.createdAt   = now;
        p.modifiedAt  = now;

        p.colorCorrection.contrast    = -15.0;
        p.colorCorrection.temperature = 15.0;
        p.colorCorrection.brightness  = 5.0;
        p.colorCorrection.saturation  = -10.0;
        p.colorCorrection.gamma       = 1.15;

        p.effects.append(VideoEffect::createBlur(3.0));

        m_presets.append(p);
    }

    // --- High Contrast B&W ---
    {
        EffectPreset p;
        p.name        = "High Contrast B&W";
        p.description = "Grayscale with high contrast and sharpen";
        p.category    = "Black & White";
        p.author      = "v-editor";
        p.isBuiltIn   = true;
        p.createdAt   = now;
        p.modifiedAt  = now;

        p.colorCorrection.contrast   = 50.0;
        p.colorCorrection.brightness = 5.0;

        p.effects.append(VideoEffect::createGrayscale());
        p.effects.append(VideoEffect::createSharpen(2.0));

        m_presets.append(p);
    }

    // --- Sunset ---
    {
        EffectPreset p;
        p.name        = "Sunset";
        p.description = "Warm temperature with boosted shadows and orange hue shift";
        p.category    = "Color";
        p.author      = "v-editor";
        p.isBuiltIn   = true;
        p.createdAt   = now;
        p.modifiedAt  = now;

        p.colorCorrection.temperature = 40.0;
        p.colorCorrection.shadows     = 20.0;
        p.colorCorrection.hue         = 15.0;
        p.colorCorrection.saturation  = 15.0;
        p.colorCorrection.tint        = 10.0;

        m_presets.append(p);
    }
}
