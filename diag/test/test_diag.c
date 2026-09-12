/* test_diag.c - Unity host unit tests for diag.c. No hardware required.
 *
 * Build + run on the host:  see diag/Makefile  ->  `make test`
 * These are the tests that would have caught the kind of bug described in the
 * project's interview notes on the desk, in milliseconds, with no board attached.
 */
#include "unity.h"
#include "diag.h"
#include <string.h>

static diag_ecu_t ecu;

void setUp(void)    { diag_init(&ecu); }
void tearDown(void) { }

/* ---- 0x22 ReadDataByIdentifier ---- */
void test_read_vin_did(void)
{
    uint8_t buf[32];
    int n = diag_read_did(&ecu, DIAG_DID_VIN, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(18, n);
    TEST_ASSERT_EQUAL_CHAR_ARRAY("NIKHILADASECU00001", buf, 18);
}

void test_read_sensor_did_is_big_endian_u16(void)
{
    uint8_t buf[4];
    ecu.sensor_cm = 300;
    int n = diag_read_did(&ecu, DIAG_DID_SENSOR, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_HEX8(0x01, buf[0]);   /* 300 = 0x012C */
    TEST_ASSERT_EQUAL_HEX8(0x2C, buf[1]);
}

void test_unknown_did_returns_negative(void)
{
    uint8_t buf[4];
    TEST_ASSERT_EQUAL_INT(-1, diag_read_did(&ecu, 0xDEAD, buf, sizeof(buf)));
}

/* ---- DEM: fault detection / DTC store ---- */
void test_in_range_sensor_raises_no_dtc(void)
{
    diag_update_sensor(&ecu, 120);
    TEST_ASSERT_EQUAL_UINT8(0, diag_dtc_count(&ecu));
}

void test_out_of_range_sensor_sets_confirmed_dtc(void)
{
    diag_update_sensor(&ecu, 999);
    uint8_t st = diag_dtc_status(&ecu, DIAG_SENSOR_FAULT_DTC);
    TEST_ASSERT_TRUE(st & DIAG_DTC_TEST_FAILED);
    TEST_ASSERT_TRUE(st & DIAG_DTC_CONFIRMED);
}

void test_repeated_fault_does_not_duplicate_dtc(void)
{
    diag_update_sensor(&ecu, 999);
    diag_update_sensor(&ecu, 1000);
    TEST_ASSERT_EQUAL_UINT8(1, diag_dtc_count(&ecu));
}

void test_clear_dtcs_empties_store(void)
{
    diag_update_sensor(&ecu, 999);
    diag_clear_dtcs(&ecu);
    TEST_ASSERT_EQUAL_UINT8(0, diag_dtc_count(&ecu));
}

/* ---- NvM: persistence across a "power cycle" ---- */
void test_dtc_survives_power_cycle_via_snapshot_restore(void)
{
    diag_ecu_t snap;
    diag_update_sensor(&ecu, 999);
    diag_snapshot(&ecu, &snap);

    diag_ecu_t rebooted;
    diag_init(&rebooted);                         /* fresh power-on, empty */
    TEST_ASSERT_EQUAL_UINT8(0, diag_dtc_count(&rebooted));
    diag_restore(&rebooted, &snap);               /* EcuM -> NvM restore */
    TEST_ASSERT_EQUAL_UINT8(1, diag_dtc_count(&rebooted));
}

/* ---- DCM: UDS request router ---- */
void test_session_control_has_uds2020_timing_bytes(void)
{
    uint8_t req[] = {0x10, 0x03};
    uint8_t out[16];
    int n = diag_handle_request(&ecu, req, sizeof(req), out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(6, n);          /* 0x50, sub, + 4 timing bytes */
    TEST_ASSERT_EQUAL_HEX8(0x50, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0x03, out[1]);
}

void test_read_did_request_returns_vin(void)
{
    uint8_t req[] = {0x22, 0xF1, 0x90};
    uint8_t out[32];
    int n = diag_handle_request(&ecu, req, sizeof(req), out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(3 + 18, n);
    TEST_ASSERT_EQUAL_HEX8(0x62, out[0]);   /* 0x22 + 0x40 */
    TEST_ASSERT_EQUAL_CHAR_ARRAY("NIKHILADASECU00001", out + 3, 18);
}

void test_unknown_service_returns_negative_response(void)
{
    uint8_t req[] = {0x3E};               /* unsupported here */
    uint8_t out[8];
    int n = diag_handle_request(&ecu, req, sizeof(req), out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(3, n);
    TEST_ASSERT_EQUAL_HEX8(0x7F, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0x3E, out[1]);
    TEST_ASSERT_EQUAL_HEX8(0x11, out[2]);   /* serviceNotSupported */
}

void test_read_dtc_request_lists_stored_dtc(void)
{
    diag_update_sensor(&ecu, 999);
    uint8_t req[] = {0x19, 0x02, 0xFF};
    uint8_t out[32];
    int n = diag_handle_request(&ecu, req, sizeof(req), out, sizeof(out));
    TEST_ASSERT_EQUAL_HEX8(0x59, out[0]);   /* 0x19 + 0x40 */
    TEST_ASSERT_EQUAL_INT(3 + 4, n);        /* header + one 4-byte DTC record */
    /* DTC 0xC10100 big-endian */
    TEST_ASSERT_EQUAL_HEX8(0xC1, out[3]);
    TEST_ASSERT_EQUAL_HEX8(0x01, out[4]);
    TEST_ASSERT_EQUAL_HEX8(0x00, out[5]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_read_vin_did);
    RUN_TEST(test_read_sensor_did_is_big_endian_u16);
    RUN_TEST(test_unknown_did_returns_negative);
    RUN_TEST(test_in_range_sensor_raises_no_dtc);
    RUN_TEST(test_out_of_range_sensor_sets_confirmed_dtc);
    RUN_TEST(test_repeated_fault_does_not_duplicate_dtc);
    RUN_TEST(test_clear_dtcs_empties_store);
    RUN_TEST(test_dtc_survives_power_cycle_via_snapshot_restore);
    RUN_TEST(test_session_control_has_uds2020_timing_bytes);
    RUN_TEST(test_read_did_request_returns_vin);
    RUN_TEST(test_unknown_service_returns_negative_response);
    RUN_TEST(test_read_dtc_request_lists_stored_dtc);
    return UNITY_END();
}
