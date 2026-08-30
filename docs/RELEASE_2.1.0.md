# RF Suite v2.1.0

Version 2.1.0 introduces a comprehensive UI/UX overhaul across all screens,
standardized 3-chip navigation and action footers, status bar safety boundaries,
font rendering fixes, robust persistence validation, and expanded native testing.

## Highlights

### 1. UI/UX Consistency Overhaul
- **Standardized 3-Chip Footer Hierarchy**:
  - **Left Chip**: Navigation / Range adjustment (`U/D ...`, `U/D SEL`, `U/D BND`, `U/D CH`, `U/D FREQ`, `U/D MOVE`).
  - **Middle Chip**: Primary Action / Execution / Toggle (`A LIVE`, `A FREEZE`, `A OPEN`, `A CHANGE`, `A RECORD`, `A RUN`).
  - **Right Chip**: Back / Exit / Cancel (`B BACK`, `B MAIN`, `B PAGE`, `B LIST`).
- **Footer Chip Overflow Prevention**:
  - Clamped all chip labels to the 47–49 px boundary (maximum 7 ASCII characters).
  - Replaced long overflow strings (`HOLD B WATCH`, `A RETRY SD`, `HOLD A TEST`, `B UP/BACK`) with clear, standardized labels.
  - Added internal clipping and safety clamping in `drawFooterChip()` ensuring `textX >= x + 1` to prevent negative X rendering.
- **GLCD 7-bit ASCII Font Integrity**:
  - Removed all non-ASCII multi-byte UTF-8 glyphs (`›`, `↑`, `↓`) that caused font corruption and string width calculation errors on Adafruit GFX ST7735 displays.
- **Header Safety Boundaries**:
  - Title strings are constrained to safe limits (maximum 14 characters) preventing collisions with top-right status bar indicators (`x=102..159`).
  - Added automatic ellipsis truncation (`~`) in `drawModernHeader()`.
- **Standardized Empty States**:
  - Centered empty state messages with unified `"A ..."` and `"B BACK"` action hints across all tools and file managers.

### 2. Core & Persistence Enhancements
- **Settings & Persistence Validation**:
  - Comprehensive parameter validation, canonicalization, and fallback defaults in `SettingsStore`.
  - Robust non-volatile storage commit scheduling with schema verification.
- **Storage & Diagnostics**:
  - Improved SD card vs. LittleFS fallback detection and diagnostic reporting.
  - Benchmarking and storage health status cards.
  - Persistent event log viewer with expandable details.

### 3. Testing & Stability
- **Expanded Native Unit Test Suite**:
  - 15 comprehensive unit test cases in `test_analyzer_math` covering RF math, exponential moving averages, confidence scoring, ring buffers, CRC32, session serialization/parsing, and persistence validation.
- **Clean Builds**:
  - 100% warning-free and error-free compilation on ESP32-S3 Arduino framework.

## Compatibility
- Backward-compatible with NVS Schema 6 settings and existing `.rfr` / `.sub` capture files.
- All hardware pinouts and driver interfaces (nRF24L01+, CC1101, ST7735, SD card) remain identical.

## Build Profiles
- `analyzer`: Default receive-only handheld firmware profile.
- `authorized_rf_lab`: Controlled lab test profile with transmission enabled for authorized RF environments.
- `native`: Host-side unit test suite.
