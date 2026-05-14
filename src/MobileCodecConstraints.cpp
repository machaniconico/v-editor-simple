#include "MobileCodecConstraints.h"

#include <array>

namespace mobile {

// ===========================================================================
// H.264 — Annex A Table A-1 (excerpt)
// MaxMBPS values are exact; MaxBR shown for High profile (cpbBrVclFactor=1500).
// ===========================================================================
namespace h264 {

namespace {
struct Row {
    Level level;
    int   maxMBPS;   // macroblocks/sec
    int   maxFS;     // macroblocks/frame
    int   maxBRkbps; // High profile
};

// Source: ITU-T H.264 (06/2019) Table A-1.
constexpr std::array<Row, 19> kTable = {{
    { Level::L1,      1485,    99,        80 },
    { Level::L1_1,    3000,   396,       240 },
    { Level::L1_2,    6000,   396,       480 },
    { Level::L1_3,   11880,   396,       960 },
    { Level::L2,     11880,   396,      2400 },
    { Level::L2_1,   19800,   792,      4800 },
    { Level::L2_2,   20250,  1620,      4800 },
    { Level::L3,     40500,  1620,     12000 },
    { Level::L3_1,  108000,  3600,     16800 },
    { Level::L3_2,  216000,  5120,     24000 },
    { Level::L4,    245760,  8192,     30000 },
    { Level::L4_1,  245760,  8192,     75000 },
    { Level::L4_2,  522240,  8704,     75000 },
    { Level::L5,    589824, 22080,    202500 },
    { Level::L5_1,  983040, 36864,    301500 },
    { Level::L5_2, 2073600, 36864,    301500 },
    { Level::L6,   4177920,139264,   360000 },
    { Level::L6_1, 8355840,139264,   720000 },
    { Level::L6_2,16711680,139264,  1200000 }
}};
} // namespace

MaxConstraints constraintsFor(Level level)
{
    for (const auto& row : kTable) {
        if (row.level == level) return { row.maxMBPS, row.maxFS, row.maxBRkbps };
    }
    return { 0, 0, 0 };
}

Level requiredLevelFor(int width, int height, int fps, int bitrateKbps)
{
    if (width <= 0 || height <= 0 || fps <= 0) return Level::L6_2;

    const int mbW = (width  + 15) / 16;
    const int mbH = (height + 15) / 16;
    const int fs  = mbW * mbH;
    const int mbps = fs * fps;

    for (const auto& row : kTable) {
        if (fs <= row.maxFS && mbps <= row.maxMBPS && bitrateKbps <= row.maxBRkbps) {
            return row.level;
        }
    }
    return Level::L6_2;
}

bool isCompatible(int width, int height, int fps, int bitrateKbps, Level level)
{
    const auto c = constraintsFor(level);
    if (width <= 0 || height <= 0 || fps <= 0) return false;
    const int mbW = (width  + 15) / 16;
    const int mbH = (height + 15) / 16;
    const int fs  = mbW * mbH;
    const int mbps = fs * fps;
    return fs <= c.maxMacroblocksPerFrame
        && mbps <= c.maxMacroblocksPerSecond
        && bitrateKbps <= c.maxBitrateKbps;
}

QString levelToString(Level level)
{
    switch (level) {
        case Level::L1:    return QStringLiteral("L1");
        case Level::L1_1:  return QStringLiteral("L1_1");
        case Level::L1_2:  return QStringLiteral("L1_2");
        case Level::L1_3:  return QStringLiteral("L1_3");
        case Level::L2:    return QStringLiteral("L2");
        case Level::L2_1:  return QStringLiteral("L2_1");
        case Level::L2_2:  return QStringLiteral("L2_2");
        case Level::L3:    return QStringLiteral("L3");
        case Level::L3_1:  return QStringLiteral("L3_1");
        case Level::L3_2:  return QStringLiteral("L3_2");
        case Level::L4:    return QStringLiteral("L4");
        case Level::L4_1:  return QStringLiteral("L4_1");
        case Level::L4_2:  return QStringLiteral("L4_2");
        case Level::L5:    return QStringLiteral("L5");
        case Level::L5_1:  return QStringLiteral("L5_1");
        case Level::L5_2:  return QStringLiteral("L5_2");
        case Level::L6:    return QStringLiteral("L6");
        case Level::L6_1:  return QStringLiteral("L6_1");
        case Level::L6_2:  return QStringLiteral("L6_2");
    }
    return QStringLiteral("?");
}

QString profileToString(Profile profile)
{
    switch (profile) {
        case Profile::Baseline: return QStringLiteral("Baseline");
        case Profile::Main:     return QStringLiteral("Main");
        case Profile::High:     return QStringLiteral("High");
    }
    return QStringLiteral("?");
}

} // namespace h264

// ===========================================================================
// HEVC — Annex A Table A.6 / A.7 (excerpt)
// MaxLumaSr (samples/sec), MaxLumaPs (samples/frame). Tier Main vs High has
// distinct MaxBR; values follow Table A.7 (kbps).
// ===========================================================================
namespace hevc {

namespace {
struct Row {
    Level level;
    long long maxLumaSr;    // samples/sec
    int       maxLumaPs;    // samples/frame
    int       maxBRMainKbps;
    int       maxBRHighKbps;
};

// Source: ITU-T H.265 (08/2021) Tables A.6 + A.7.
// (High Tier undefined for L1..L3.1 → 0 sentinel.)
constexpr std::array<Row, 13> kTable = {{
    { Level::L1,        552'960LL,    36'864,     128,        0 },
    { Level::L2,      3'686'400LL,   122'880,    1500,        0 },
    { Level::L2_1,    7'372'800LL,   245'760,    3000,        0 },
    { Level::L3,     16'588'800LL,   552'960,    6000,        0 },
    { Level::L3_1,   33'177'600LL,   983'040,   10000,        0 },
    { Level::L4,     66'846'720LL, 2'228'224,   12000,    30000 },
    { Level::L4_1,  133'693'440LL, 2'228'224,   20000,    50000 },
    { Level::L5,    267'386'880LL, 8'912'896,   25000,   100000 },
    { Level::L5_1,  534'773'760LL, 8'912'896,   40000,   160000 },
    { Level::L5_2, 1'069'547'520LL, 8'912'896,   60000,   240000 },
    { Level::L6,   1'069'547'520LL,35'651'584,   60000,   240000 },
    { Level::L6_1, 2'139'095'040LL,35'651'584,  120000,   480000 },
    { Level::L6_2, 4'278'190'080LL,35'651'584,  240000,   800000 }
}};
} // namespace

MaxConstraints constraintsFor(Level level, Tier tier)
{
    for (const auto& row : kTable) {
        if (row.level == level) {
            const int br = (tier == Tier::High) ? row.maxBRHighKbps : row.maxBRMainKbps;
            return { row.maxLumaSr, row.maxLumaPs, br };
        }
    }
    return { 0LL, 0, 0 };
}

Level requiredLevelFor(int width, int height, int fps, int bitrateKbps, Tier tier)
{
    if (width <= 0 || height <= 0 || fps <= 0) return Level::L6_2;
    const int       lumaPs = width * height;
    const long long lumaSr = static_cast<long long>(lumaPs) * fps;

    for (const auto& row : kTable) {
        const int br = (tier == Tier::High) ? row.maxBRHighKbps : row.maxBRMainKbps;
        if (br == 0) continue; // tier not defined at this level
        if (lumaPs <= row.maxLumaPs && lumaSr <= row.maxLumaSr && bitrateKbps <= br) {
            return row.level;
        }
    }
    return Level::L6_2;
}

bool isCompatible(int width, int height, int fps, int bitrateKbps, Level level, Tier tier)
{
    const auto c = constraintsFor(level, tier);
    if (c.maxBitrateKbps == 0) return false;
    if (width <= 0 || height <= 0 || fps <= 0) return false;
    const int       lumaPs = width * height;
    const long long lumaSr = static_cast<long long>(lumaPs) * fps;
    return lumaPs <= c.maxLumaSamplesPerFrame
        && lumaSr <= c.maxLumaSamplesPerSecond
        && bitrateKbps <= c.maxBitrateKbps;
}

int bitDepthFromProfile(Profile profile)
{
    switch (profile) {
        case Profile::Main:               return 8;
        case Profile::Main10:             return 10;
        case Profile::Main10StillPicture: return 10;
    }
    return 8;
}

QString levelToString(Level level)
{
    switch (level) {
        case Level::L1:    return QStringLiteral("L1");
        case Level::L2:    return QStringLiteral("L2");
        case Level::L2_1:  return QStringLiteral("L2_1");
        case Level::L3:    return QStringLiteral("L3");
        case Level::L3_1:  return QStringLiteral("L3_1");
        case Level::L4:    return QStringLiteral("L4");
        case Level::L4_1:  return QStringLiteral("L4_1");
        case Level::L5:    return QStringLiteral("L5");
        case Level::L5_1:  return QStringLiteral("L5_1");
        case Level::L5_2:  return QStringLiteral("L5_2");
        case Level::L6:    return QStringLiteral("L6");
        case Level::L6_1:  return QStringLiteral("L6_1");
        case Level::L6_2:  return QStringLiteral("L6_2");
    }
    return QStringLiteral("?");
}

QString tierToString(Tier tier)
{
    switch (tier) {
        case Tier::Main: return QStringLiteral("Main");
        case Tier::High: return QStringLiteral("High");
    }
    return QStringLiteral("?");
}

QString profileToString(Profile profile)
{
    switch (profile) {
        case Profile::Main:               return QStringLiteral("Main");
        case Profile::Main10:             return QStringLiteral("Main10");
        case Profile::Main10StillPicture: return QStringLiteral("Main10StillPicture");
    }
    return QStringLiteral("?");
}

} // namespace hevc

} // namespace mobile
