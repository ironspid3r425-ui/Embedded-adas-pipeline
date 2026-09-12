/*
 * diag.h - Portable UDS diagnostic logic for the ADAS pipeline ECU.
 *
 * Pure C99, no hardware dependencies, no dynamic memory: the same logic runs
 * in host unit tests (see test/test_diag.c) and on the STM32, where it is fed
 * by the ISO-TP / CAN receive path. This is the firmware-side mirror of the
 * Python reference in tester/ecu_logic.py, and maps to AUTOSAR DCM/DEM/NvM.
 */
#ifndef DIAG_H
#define DIAG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Data Identifiers (UDS 0x22) */
#define DIAG_DID_VIN     0xF190u   /* standard VIN DID */
#define DIAG_DID_SENSOR  0x0100u   /* front distance sensor, cm */

/* DTC status bits (a simplified slice of the ISO 14229 status byte) */
#define DIAG_DTC_TEST_FAILED  0x01u  /* bit 0: fault active now */
#define DIAG_DTC_CONFIRMED    0x08u  /* bit 3: fault stored/confirmed */

#define DIAG_SENSOR_FAULT_DTC 0xC10100u  /* "sensor out of range" */
#define DIAG_SENSOR_MIN_CM    5
#define DIAG_SENSOR_MAX_CM    400

#define DIAG_VIN_LEN   18u
#define DIAG_MAX_DTCS  8u

typedef struct {
    uint32_t dtc;      /* 3-byte DTC number, held in a u32 */
    uint8_t  status;   /* status byte */
} diag_dtc_entry_t;

typedef struct {
    char             vin[DIAG_VIN_LEN + 1];
    uint16_t         sensor_cm;
    diag_dtc_entry_t dtcs[DIAG_MAX_DTCS];
    uint8_t          dtc_count;
} diag_ecu_t;

/* Lifecycle */
void diag_init(diag_ecu_t *ecu);

/* Read a DID into out (caller supplies capacity). Returns length, or -1 if unknown. */
int diag_read_did(const diag_ecu_t *ecu, uint16_t did, uint8_t *out, size_t cap);

/* Feed a new sensor reading; raises/confirms the sensor DTC if out of range (DEM). */
void diag_update_sensor(diag_ecu_t *ecu, uint16_t value_cm);

/* DTC store access (DEM) */
uint8_t diag_dtc_count(const diag_ecu_t *ecu);
uint8_t diag_dtc_status(const diag_ecu_t *ecu, uint32_t dtc);  /* 0 if absent */
void    diag_clear_dtcs(diag_ecu_t *ecu);

/* Persistence stand-in (NvM + EcuM restore). snap buffer must hold a snapshot. */
void diag_snapshot(const diag_ecu_t *ecu, diag_ecu_t *snap_dst);
void diag_restore(diag_ecu_t *ecu, const diag_ecu_t *snap_src);

/*
 * UDS request router (DCM). Takes a request (service id + payload), writes the
 * response into out, returns response length (>=0), or -1 if out is too small.
 * Handles 0x10, 0x22, 0x19, 0x14; anything else -> negative response 0x7F.
 */
int diag_handle_request(diag_ecu_t *ecu,
                        const uint8_t *req, size_t req_len,
                        uint8_t *out, size_t out_cap);

#endif /* DIAG_H */
