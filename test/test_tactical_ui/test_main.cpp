#include <unity.h>
#include "ui/TacticalTheme.h"
#include "ui/HeaderWidget.h"
#include "ui/MenuListView.h"
#include "ui/SpectrumScannerView.h"
#include "ui/PacketInspectorView.h"

void test_tactical_colors() {
    TEST_ASSERT_EQUAL_HEX16(0x0000, TacticalColor::PureBlack);
    TEST_ASSERT_EQUAL_HEX16(0x18E3, TacticalColor::DarkCharcoal);
    TEST_ASSERT_EQUAL_HEX16(0x2104, TacticalColor::Wireframe);
    TEST_ASSERT_EQUAL_HEX16(0x07FF, TacticalColor::Cyan);
    TEST_ASSERT_EQUAL_HEX16(0xFDA0, TacticalColor::AmberOrange);
    TEST_ASSERT_EQUAL_HEX16(0xF800, TacticalColor::ThreatRed);
    TEST_ASSERT_EQUAL_HEX16(0x07E0, TacticalColor::SuccessGreen);
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, TacticalColor::HighWhite);
    TEST_ASSERT_EQUAL_HEX16(0x8410, TacticalColor::MutedGrey);
}

void test_header_telemetry_defaults() {
    HeaderTelemetry tel;
    TEST_ASSERT_NULL(tel.title);
    TEST_ASSERT_EQUAL_STRING("RX", tel.rfMode);
    TEST_ASSERT_FALSE(tel.isTx);
    TEST_ASSERT_FALSE(tel.isSim);
    TEST_ASSERT_FALSE(tel.radio1Connected);
    TEST_ASSERT_FALSE(tel.radio2Connected);
    TEST_ASSERT_EQUAL_INT(-1, tel.activeChannel);
    TEST_ASSERT_EQUAL_UINT8(100, tel.batteryPercent);
}

void test_menu_contract() {
    MenuItemContract item;
    item.label = "SCANNER";
    item.iconId = 0;
    item.badgeText = "NEW";
    item.badgeColor = TacticalColor::Cyan;

    TEST_ASSERT_EQUAL_STRING("SCANNER", item.label);
    TEST_ASSERT_EQUAL_UINT8(0, item.iconId);
    TEST_ASSERT_EQUAL_STRING("NEW", item.badgeText);
    TEST_ASSERT_EQUAL_HEX16(0x07FF, item.badgeColor);
}

void test_spectrum_telemetry_bounds() {
    SpectrumScanTelemetry tel;
    TEST_ASSERT_EQUAL_INT(0, tel.minChannel);
    TEST_ASSERT_EQUAL_INT(125, tel.maxChannel);
    TEST_ASSERT_EQUAL_INT(36, tel.cursorChannel);
    TEST_ASSERT_EQUAL_STRING("LIVE", tel.traceModeName);
}

void test_packet_sniffer_telemetry() {
    PacketSnifferTelemetry tel;
    TEST_ASSERT_FALSE(tel.isRunning);
    TEST_ASSERT_EQUAL_UINT8(0, tel.channel);
    TEST_ASSERT_EQUAL_INT(2, tel.dataRateMbps);
    TEST_ASSERT_EQUAL_UINT32(0, tel.packetCount);
}

void test_rssi_segment_math() {
    // 5 segments, check threshold coverage
    const uint8_t segments = 5;
    for (uint8_t i = 0; i < segments; ++i) {
        uint8_t thresh = ((i + 1) * 100) / segments;
        TEST_ASSERT_GREATER_THAN(0, thresh);
        TEST_ASSERT_LESS_OR_EQUAL(100, thresh);
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_tactical_colors);
    RUN_TEST(test_header_telemetry_defaults);
    RUN_TEST(test_menu_contract);
    RUN_TEST(test_spectrum_telemetry_bounds);
    RUN_TEST(test_packet_sniffer_telemetry);
    RUN_TEST(test_rssi_segment_math);
    return UNITY_END();
}
