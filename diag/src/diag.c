/* diag.c - see diag.h. Portable, no hardware, no dynamic memory. */
#include "diag.h"
#include <string.h>

/* UDS service IDs */
#define SID_SESSION_CONTROL 0x10u
#define SID_READ_DID        0x22u
#define SID_READ_DTC        0x19u
#define SID_CLEAR_DTC       0x14u
#define UDS_POSITIVE        0x40u   /* added to SID for a positive response */
#define UDS_NEG_RESPONSE    0x7Fu
#define NRC_SERVICE_NOT_SUPPORTED 0x11u
#define NRC_REQUEST_OUT_OF_RANGE  0x31u

void diag_init(diag_ecu_t *ecu)
{
    memset(ecu, 0, sizeof(*ecu));
    strncpy(ecu->vin, "NIKHILADASECU00001", DIAG_VIN_LEN);
    ecu->vin[DIAG_VIN_LEN] = '\0';
    ecu->sensor_cm = 120u;
    ecu->dtc_count = 0u;
}

int diag_read_did(const diag_ecu_t *ecu, uint16_t did, uint8_t *out, size_t cap)
{
    if (did == DIAG_DID_VIN) {
        size_t n = strlen(ecu->vin);
        if (n > cap) return -1;
        memcpy(out, ecu->vin, n);
        return (int)n;
    }
    if (did == DIAG_DID_SENSOR) {
        if (cap < 2u) return -1;
        out[0] = (uint8_t)(ecu->sensor_cm >> 8);
        out[1] = (uint8_t)(ecu->sensor_cm & 0xFFu);
        return 2;
    }
    return -1;
}

/* Find a DTC slot, or -1. */
static int find_dtc(const diag_ecu_t *ecu, uint32_t dtc)
{
    for (uint8_t i = 0; i < ecu->dtc_count; i++) {
        if (ecu->dtcs[i].dtc == dtc) return (int)i;
    }
    return -1;
}

void diag_update_sensor(diag_ecu_t *ecu, uint16_t value_cm)
{
    ecu->sensor_cm = value_cm;
    if (value_cm < DIAG_SENSOR_MIN_CM || value_cm > DIAG_SENSOR_MAX_CM) {
        int idx = find_dtc(ecu, DIAG_SENSOR_FAULT_DTC);
        if (idx < 0) {
            if (ecu->dtc_count >= DIAG_MAX_DTCS) return;  /* store full */
            idx = ecu->dtc_count++;
            ecu->dtcs[idx].dtc = DIAG_SENSOR_FAULT_DTC;
            ecu->dtcs[idx].status = 0u;
        }
        ecu->dtcs[idx].status |= (DIAG_DTC_TEST_FAILED | DIAG_DTC_CONFIRMED);
    }
}

uint8_t diag_dtc_count(const diag_ecu_t *ecu) { return ecu->dtc_count; }

uint8_t diag_dtc_status(const diag_ecu_t *ecu, uint32_t dtc)
{
    int idx = find_dtc(ecu, dtc);
    return (idx < 0) ? 0u : ecu->dtcs[idx].status;
}

void diag_clear_dtcs(diag_ecu_t *ecu) { ecu->dtc_count = 0u; }

void diag_snapshot(const diag_ecu_t *ecu, diag_ecu_t *snap_dst) { *snap_dst = *ecu; }
void diag_restore(diag_ecu_t *ecu, const diag_ecu_t *snap_src)  { *ecu = *snap_src; }

static int nrc(uint8_t sid, uint8_t code, uint8_t *out, size_t cap)
{
    if (cap < 3u) return -1;
    out[0] = UDS_NEG_RESPONSE; out[1] = sid; out[2] = code;
    return 3;
}

int diag_handle_request(diag_ecu_t *ecu,
                        const uint8_t *req, size_t req_len,
                        uint8_t *out, size_t out_cap)
{
    if (req_len == 0u) return -1;
    uint8_t sid = req[0];

    switch (sid) {
    case SID_SESSION_CONTROL: {
        uint8_t sub = (req_len > 1u) ? req[1] : 0x01u;
        if (out_cap < 6u) return -1;
        out[0] = sid + UDS_POSITIVE;
        out[1] = sub;
        /* UDS-2020: P2_server_max (ms) + P2*_server_max (10 ms units) */
        out[2] = 0x00u; out[3] = 0x32u;   /* 50 ms */
        out[4] = 0x01u; out[5] = 0xF4u;   /* 500 * 10 ms = 5 s */
        return 6;
    }
    case SID_READ_DID: {
        if (req_len < 3u) return nrc(sid, NRC_REQUEST_OUT_OF_RANGE, out, out_cap);
        uint16_t did = (uint16_t)((req[1] << 8) | req[2]);
        if (out_cap < 3u) return -1;
        int n = diag_read_did(ecu, did, out + 3, out_cap - 3u);
        if (n < 0) return nrc(sid, NRC_REQUEST_OUT_OF_RANGE, out, out_cap);
        out[0] = sid + UDS_POSITIVE;
        out[1] = req[1];
        out[2] = req[2];
        return 3 + n;
    }
    case SID_READ_DTC: {
        /* sub 0x02: report DTCs by status mask */
        if (out_cap < 3u) return -1;
        out[0] = sid + UDS_POSITIVE;
        out[1] = 0x02u;
        out[2] = 0xFFu;  /* status availability mask (simplified) */
        size_t pos = 3u;
        for (uint8_t i = 0; i < ecu->dtc_count; i++) {
            if (pos + 4u > out_cap) return -1;
            uint32_t d = ecu->dtcs[i].dtc;
            out[pos++] = (uint8_t)(d >> 16);
            out[pos++] = (uint8_t)(d >> 8);
            out[pos++] = (uint8_t)(d & 0xFFu);
            out[pos++] = ecu->dtcs[i].status;
        }
        return (int)pos;
    }
    case SID_CLEAR_DTC:
        diag_clear_dtcs(ecu);
        if (out_cap < 1u) return -1;
        out[0] = sid + UDS_POSITIVE;
        return 1;
    default:
        return nrc(sid, NRC_SERVICE_NOT_SUPPORTED, out, out_cap);
    }
}
