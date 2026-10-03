#pragma once

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <stdint.h>
#endif

/**
 * @file TacticalTheme.h
 * @brief High-contrast Tactical Color Tokens for ESP32-DIV UI aesthetic.
 *
 * Color specification:
 * - Background: Pure Black (0x0000)
 * - Primary Surface / Card: Dark Charcoal (0x18E3)
 * - 1px Wireframe Borders: Charcoal Wireframe (0x2104)
 * - Tactical Accents:
 *     - Cyan (0x07FF)    : Primary reticle, active focus, telemetry accent
 *     - Amber (0xFDA0)   : Warning, threshold guideline, high activity
 *     - Threat Red (0xF800): Jammer/TX active, critical fault, RF hazard
 *     - Success Green (0x07E0): Healthy radio, connected state, low activity
 * - Typography:
 *     - High-contrast White (0xFFFF) : Primary readouts, active labels
 *     - Muted Grey (0x8410)          : Subtitles, units, inactive brackets
 */

namespace TacticalColor {
    // Surface & Structure
    constexpr uint16_t PureBlack      = 0x0000;
    constexpr uint16_t DarkCharcoal   = 0x18E3;
    constexpr uint16_t Wireframe      = 0x2104;
    constexpr uint16_t WireframeLight = 0x39E7;
    constexpr uint16_t CardSurface    = 0x1082;
    constexpr uint16_t BarTrack       = 0x2104;

    // Tactical Accents
    constexpr uint16_t Cyan           = 0x07FF;
    constexpr uint16_t AmberOrange    = 0xFDA0;
    constexpr uint16_t ThreatRed      = 0xF800;
    constexpr uint16_t SuccessGreen   = 0x07E0;

    // Typography
    constexpr uint16_t HighWhite      = 0xFFFF;
    constexpr uint16_t MutedGrey      = 0x8410;
    constexpr uint16_t DarkGrey       = 0x4208;
} // namespace TacticalColor
