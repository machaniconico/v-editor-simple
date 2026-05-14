#pragma once
#include <QString>

namespace mobile {

// ===========================================================================
// H.264 / AVC — ITU-T Rec. H.264 Annex A
// ===========================================================================
namespace h264 {

enum class Profile {
    Baseline,
    Main,
    High
};

enum class Level {
    L1, L1_1, L1_2, L1_3,
    L2, L2_1, L2_2,
    L3, L3_1, L3_2,
    L4, L4_1, L4_2,
    L5, L5_1, L5_2,
    L6, L6_1, L6_2
};

struct MaxConstraints {
    int maxMacroblocksPerSecond; // MaxMBPS  (16x16 MB / second)
    int maxMacroblocksPerFrame;  // MaxFS    (16x16 MB / frame)
    int maxBitrateKbps;          // MaxBR for High profile (kbps)
};

MaxConstraints constraintsFor(Level level);
Level          requiredLevelFor(int width, int height, int fps, int bitrateKbps);
bool           isCompatible(int width, int height, int fps, int bitrateKbps, Level level);
QString        levelToString(Level level);
QString        profileToString(Profile profile);

} // namespace h264

// ===========================================================================
// HEVC / H.265 — ITU-T Rec. H.265 Annex A
// ===========================================================================
namespace hevc {

enum class Profile {
    Main,
    Main10,
    Main10StillPicture
};

enum class Tier {
    Main,
    High
};

enum class Level {
    L1,
    L2, L2_1,
    L3, L3_1,
    L4, L4_1,
    L5, L5_1, L5_2,
    L6, L6_1, L6_2
};

struct MaxConstraints {
    long long maxLumaSamplesPerSecond; // MaxLumaSr
    int       maxLumaSamplesPerFrame;  // MaxLumaPs
    int       maxBitrateKbps;          // tier-dependent
};

MaxConstraints constraintsFor(Level level, Tier tier);
Level          requiredLevelFor(int width, int height, int fps, int bitrateKbps, Tier tier);
bool           isCompatible(int width, int height, int fps, int bitrateKbps, Level level, Tier tier);
int            bitDepthFromProfile(Profile profile);
QString        levelToString(Level level);
QString        tierToString(Tier tier);
QString        profileToString(Profile profile);

} // namespace hevc

} // namespace mobile
