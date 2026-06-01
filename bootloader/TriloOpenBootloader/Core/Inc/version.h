/**
 * @file version.h
 * @brief Firmware version information for TriloOpenBootloader variants
 * 
 * Semantic versioning format: vX.Y.Z-BL-<VARIANT>
 * 
 * The firmware version is automatically injected at build time from:
 *   1. CMake -DFIRMWARE_VERSION argument (for CI/CD releases with semantic tags)
 *   2. Git short hash ± "-dirty" suffix (for development builds)
 *   3. Fallback to VERSION_STRING if git unavailable
 */

#ifndef VERSION_H
#define VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

/* FIRMWARE_VERSION is injected by cmake/version.cmake at compile time */
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION    "unknown"
#endif

/**
 * Variant-specific semantic version string
 * Format: <FIRMWARE_VERSION>-BL-<VARIANT>
 * Examples: "1.0.0-BL-J1_CONTROL", "a1b2c3d-dirty-BL-TOOL_CONTROL"
 */
#if defined(BLD_DEFAULT)
char semanticVersion[32] = FIRMWARE_VERSION "-BL-DEFAULT";
#elif defined(BLD_TOOL_CONTROL)
char semanticVersion[32] = FIRMWARE_VERSION "-BL-TOOL_CONTROL";
#elif defined(BLD_J3_CONTROL)
char semanticVersion[32] = FIRMWARE_VERSION "-BL-J3_CONTROL";
#elif defined(BLD_J2_CONTROL)
char semanticVersion[32] = FIRMWARE_VERSION "-BL-J2_CONTROL";
#elif defined(BLD_J1_CONTROL)
char semanticVersion[32] = FIRMWARE_VERSION "-BL-J1_CONTROL";
#elif defined(BLD_LED)
char semanticVersion[32] = FIRMWARE_VERSION "-BL-LED";
#elif defined(BLD_DECK_SLOT)
char semanticVersion[32] = FIRMWARE_VERSION "-BL-DECK_SLOT";
#elif defined(BLD_INTERMODULARITY)
char semanticVersion[32] = FIRMWARE_VERSION "-BL-IM";
#else
#error "Must define one of: [BLD_DEFAULT, BLD_TOOL_CONTROL, BLD_J3_CONTROL, BLD_J2_CONTROL, BLD_J1_CONTROL, BLD_LED, BLD_DECK_SLOT, BLD_INTERMODULARITY]"
#endif

#ifdef __cplusplus
}
#endif

#endif /* VERSION_H */
