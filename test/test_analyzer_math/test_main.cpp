#include <unity.h>
#include "core/AnalyzerMath.h"
#include "core/RfEnvironmentMath.h"
#include "core/Crc32.h"
#include "core/RfrFormat.h"
#include "core/SessionFormat.h"
#include "core/SettingsValidation.h"
#include <stdio.h>
#include <string.h>

void test_percent_clamping() {
    TEST_ASSERT_EQUAL_UINT8(0, AnalyzerMath::clampPercent(-1));
    TEST_ASSERT_EQUAL_UINT8(42, AnalyzerMath::clampPercent(42));
    TEST_ASSERT_EQUAL_UINT8(100, AnalyzerMath::clampPercent(130));
}

void test_exponential_average() {
    TEST_ASSERT_EQUAL_UINT8(25, AnalyzerMath::ema(0, 100));
    TEST_ASSERT_EQUAL_UINT8(62, AnalyzerMath::ema(50, 100));
}

void test_baseline_delta_never_underflows() {
    TEST_ASSERT_EQUAL_UINT8(0, AnalyzerMath::deltaAboveBaseline(30, 50));
    TEST_ASSERT_EQUAL_UINT8(35, AnalyzerMath::deltaAboveBaseline(75, 40));
}

void test_confidence_rewards_samples_receivers_and_baseline() {
    const uint8_t shallow = AnalyzerMath::observationConfidence(12, 1, false);
    const uint8_t deep = AnalyzerMath::observationConfidence(60, 2, true);
    TEST_ASSERT_GREATER_THAN(shallow, deep);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(100, deep);
}

void test_event_run_uses_hysteresis() {
    uint8_t run = AnalyzerMath::nextEventRun(0, 65, 60, 10);
    TEST_ASSERT_EQUAL_UINT8(1, run);
    run = AnalyzerMath::nextEventRun(run, 55, 60, 10);
    TEST_ASSERT_EQUAL_UINT8(1, run);
    run = AnalyzerMath::nextEventRun(run, 50, 60, 10);
    TEST_ASSERT_EQUAL_UINT8(0, run);
}
void test_environment_occupancy_and_ema(){TEST_ASSERT_EQUAL_UINT8(25,RfEnvironmentMath::percent(1,4));TEST_ASSERT_EQUAL_UINT8(0,RfEnvironmentMath::percent(3,0));TEST_ASSERT_EQUAL_UINT8(63,RfEnvironmentMath::ema(50,100,25));}
void test_environment_score(){TEST_ASSERT_EQUAL_UINT8(52,RfEnvironmentMath::interferenceScore(70,50,30,40));TEST_ASSERT_EQUAL_UINT8(3,RfEnvironmentMath::classifyScore(74));}
void test_environment_mapping(){TEST_ASSERT_EQUAL_UINT16(2442,RfEnvironmentMath::frequencyMHz(42));TEST_ASSERT_EQUAL_INT8(7,RfEnvironmentMath::wifiChannelForMHz(2442));TEST_ASSERT_EQUAL_INT8(11,RfEnvironmentMath::zigbeeChannelForMHz(2405));TEST_ASSERT_TRUE(RfEnvironmentMath::protocolRegions(2426)&2);}
void test_ring_and_delta(){TEST_ASSERT_EQUAL_UINT8(0,RfEnvironmentMath::ringNext(31,32));TEST_ASSERT_EQUAL_INT16(53,RfEnvironmentMath::signedDelta(21,74));}
void test_validation_and_burst(){TEST_ASSERT_TRUE(RfEnvironmentMath::validWindowSeconds(10));TEST_ASSERT_FALSE(RfEnvironmentMath::validWindowSeconds(7));TEST_ASSERT_TRUE(RfEnvironmentMath::validRange(0,125));TEST_ASSERT_FALSE(RfEnvironmentMath::validRange(80,20));TEST_ASSERT_EQUAL_UINT8(2,RfEnvironmentMath::burstSeverity(55));}

void test_crc32_known_vector() {
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926UL, Crc32::calculate("123456789", 9));
}

void test_rfr_metadata_versions_and_bounds() {
    TEST_ASSERT_TRUE(RfrFormat::isV1("RFS1"));
    TEST_ASSERT_TRUE(RfrFormat::isV2("RFS2"));
    TEST_ASSERT_TRUE(RfrFormat::validMetadata(433920000UL, 20, 8192));
    TEST_ASSERT_FALSE(RfrFormat::validMetadata(433920000UL, 1, 8192));
    TEST_ASSERT_FALSE(RfrFormat::validMetadata(100000000UL, 20, 8192));
}

void test_session_parser_accepts_complete_row() {
    char line[700];
    int used = snprintf(line, sizeof(line), "S,100,7,42,80,90,0,1,2");
    for (int i = 0; i < 126; ++i) used += snprintf(line + used, sizeof(line) - used, ",%d", i % 101);
    SessionFormat::Sweep sweep;
    TEST_ASSERT_TRUE(SessionFormat::parseSweepLine(line, sweep));
    TEST_ASSERT_EQUAL_UINT8(42, sweep.peakChannel);
    TEST_ASSERT_EQUAL_UINT8(100, sweep.levels[100]);
}

void test_session_parser_rejects_truncated_invalid_and_overflow_rows() {
    SessionFormat::Sweep sweep;
    TEST_ASSERT_FALSE(SessionFormat::parseSweepLine("S,1,2,3,4,5,0,0,0,1,2", sweep));
    TEST_ASSERT_FALSE(SessionFormat::parseSweepLine("S,42949672960,2,3,4,5,0,0,0", sweep));
    TEST_ASSERT_FALSE(SessionFormat::parseSweepLine("E,1,2", sweep));
}

void test_persistence_validation_defaults_and_canonicalizes() {
    TEST_ASSERT_EQUAL_UINT8(2, SettingsValidation::enumOrDefault(9, 4, 2));
    TEST_ASSERT_EQUAL_UINT8(3, SettingsValidation::enumOrDefault(3, 4, 2));
    TEST_ASSERT_EQUAL_UINT8(1, SettingsValidation::replayRepeats(2));
    TEST_ASSERT_EQUAL_UINT8(3, SettingsValidation::replayRepeats(4));
    TEST_ASSERT_EQUAL_UINT8(5, SettingsValidation::replayRepeats(99));
    const uint8_t good[] = {0, 42, 125};
    const uint8_t bad[] = {0, 126};
    TEST_ASSERT_TRUE(SettingsValidation::validChannels(good, 3, 125));
    TEST_ASSERT_FALSE(SettingsValidation::validChannels(bad, 2, 125));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_percent_clamping);
    RUN_TEST(test_exponential_average);
    RUN_TEST(test_baseline_delta_never_underflows);
    RUN_TEST(test_confidence_rewards_samples_receivers_and_baseline);
    RUN_TEST(test_event_run_uses_hysteresis);
    RUN_TEST(test_environment_occupancy_and_ema);
    RUN_TEST(test_environment_score);
    RUN_TEST(test_environment_mapping);
    RUN_TEST(test_ring_and_delta);
    RUN_TEST(test_validation_and_burst);
    RUN_TEST(test_crc32_known_vector);
    RUN_TEST(test_rfr_metadata_versions_and_bounds);
    RUN_TEST(test_session_parser_accepts_complete_row);
    RUN_TEST(test_session_parser_rejects_truncated_invalid_and_overflow_rows);
    RUN_TEST(test_persistence_validation_defaults_and_canonicalizes);
    return UNITY_END();
}
