#pragma once

#include <cstdint>

namespace ui::radar {

constexpr int kSize = 240;
constexpr int kCenterX = kSize / 2;
constexpr int kCenterY = kSize / 2;

/** Outermost grid ring (inside edge labels). */
constexpr int kGridOuterRadius = 107;

/** N: offset from top edge (top_center, negative = up). */
constexpr int kCardinalNorthOffsetY = -1;
/** S: offset from bottom edge (bottom_center, positive = down). */
constexpr int kCardinalSouthOffsetY = 3;

/** Gap between scale label right edge and outer ring on the east spoke (px). */
constexpr int kScaleGapFromOuterRing = 6;

/** Target cap height (px) for N/S/E/W. */
constexpr int kCardinalLabelHeightPx = 14;
/** Scale label is this many px shorter than cardinals. */
constexpr int kScaleBelowCardinalPx = 3;

constexpr int kRingCount = 4;

/** Shared grid stroke: drawWideLine half-width (~2 px total); rings use the same px count. */
constexpr float kGridStrokeHalfWidth = 1.0f;

constexpr int kCenterDotRadius = 2;

/** Filled aircraft symbol (nose triangle). */
constexpr int kAircraftNoseLenPx = 8;
constexpr int kAircraftTailLenPx = 3;
constexpr int kAircraftTailHalfPx = 4;
/** Track vector: ground distance covered in this many seconds at current gs. */
constexpr float kAircraftTrackHorizonSec = 60.0f;
/** Minimum visible vector when gs > 0 (px). */
constexpr int kAircraftSpeedLineMinPx = 2;
/** Track line length uses this outer_km, not the active range preset. */
constexpr float kAircraftTrackRefOuterKm = 13.3f;
/** Shorter than full 60 s horizon at ref scale; ×1.5 length boost applied. */
constexpr float kAircraftTrackLengthScale = 1.5f / 5.0f;
/** drawWideLine half-width for speed vectors (~2 px total). */
constexpr float kAircraftTrackLineHalfWidth = 1.0f;

constexpr float kRunwayLineWidthPx = 2.0f;
constexpr float kRunwayLineHalfWidth = kRunwayLineWidthPx * 0.5f;
constexpr int kRunwayLabelHeightPx = kCardinalLabelHeightPx;
constexpr int kRunwayLabelGapPx = 3;
/** Gap from triangle edge to tag block (px). */
constexpr int kAircraftLabelGapPx = 1;
/** Keep symbol centroid inside outer ring by at least this inset (px). */
constexpr int kAircraftInsideRingInsetPx =
    kAircraftNoseLenPx + kAircraftTailHalfPx + 1;

/** Beyond-ring traffic: bearing cues on screen rim (correct direction, fixed radius). */
constexpr int kBeyondRingDotRadiusPx = 4;
constexpr int kBeyondRingScreenMarginPx = 2;
/** Target cap height (px) for aircraft tags (bold, slightly above scale label). */
constexpr int kAircraftTagLabelHeightPx = 13;

/** RGB565 palette targets (Catppuccin Mocha). */
constexpr uint8_t kBgR = 30;
constexpr uint8_t kBgG = 30;
constexpr uint8_t kBgB = 46;
constexpr uint8_t kGridR = 88;
constexpr uint8_t kGridG = 91;
constexpr uint8_t kGridB = 112;
constexpr uint8_t kAircraftR = 243;
constexpr uint8_t kAircraftG = 139;
constexpr uint8_t kAircraftB = 168;
constexpr uint8_t kHeliR = 137;
constexpr uint8_t kHeliG = 220;
constexpr uint8_t kHeliB = 235;
constexpr uint8_t kTrackR = 203;
constexpr uint8_t kTrackG = 166;
constexpr uint8_t kTrackB = 247;
constexpr uint8_t kTagTypeR = 249;
constexpr uint8_t kTagTypeG = 226;
constexpr uint8_t kTagTypeB = 175;
constexpr uint8_t kTagAltR = 116;
constexpr uint8_t kTagAltG = 199;
constexpr uint8_t kTagAltB = 236;
constexpr uint8_t kRunwayR = 108;
constexpr uint8_t kRunwayG = 112;
constexpr uint8_t kRunwayB = 134;
constexpr uint8_t kRunwayLabelR = 166;
constexpr uint8_t kRunwayLabelG = 173;
constexpr uint8_t kRunwayLabelB = 200;

extern uint16_t kColorBackground;
extern uint16_t kColorGrid;
extern uint16_t kColorLabel;
extern uint16_t kColorCenter;
extern uint16_t kColorAircraft;
extern uint16_t kColorHelicopter;
extern uint16_t kColorTrackVector;
extern uint16_t kColorTagType;
extern uint16_t kColorTagAltitude;
extern uint16_t kColorRunway;
extern uint16_t kColorRunwayLabel;

}  // namespace ui::radar
