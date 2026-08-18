# Embedded ADAS Alert Pipeline

Bare-metal peripheral bring-up on an STM32F411RE (NUCLEO-F411RE, ARM Cortex-M4), building toward a driver stack for an ADAS-style alert system: GPIO, UART, I2C-driven OLED display, and SPI, brought up and verified incrementally with a dated engineering log rather than all at once.

## Problem

ADAS (advanced driver-assistance) alerting needs a microcontroller that can reliably read sensors/inputs, drive a display, and communicate over multiple buses at once, with each peripheral verified in isolation before they're combined. This project builds that base peripheral layer from scratch on the HAL, one bus at a time, with pass/fail verification at every step rather than assuming a working configuration.

## Approach

Each peripheral was brought up independently and validated against real hardware before moving to the next:
1. **GPIO** — onboard LED toggled via `HAL_GPIO_TogglePin` to confirm the build/flash/debug toolchain works end-to-end.
2. **UART** — serial output over USART2, triggered by a button press, verified against a live PuTTY terminal.
3. **I2C + display** — an SSD1306 OLED driven over I2C1, used as a visual verification target for later stages (not just a demo — it's the pass/fail readout for the SPI test below).
4. **SPI** — a loopback test (TX wired directly to RX) with a 4-byte pattern, verified byte-for-byte with `memcmp` and the result displayed on the OLED and echoed over UART.
5. **Register-level rewrite** — the GPIO step was redone bypassing the HAL entirely, driving `RCC`/`GPIOA` registers directly, to build an actual understanding of what the HAL abstracts away rather than treating it as a black box.

## Results

- Onboard LED (PA5) blink verified at a 500 ms interval; button (B1, PC13) triggers both an LED toggle and a UART message, confirmed live in PuTTY.
- I2C1 (PB6/PB7) driving the SSD1306 OLED confirmed working via a frame-by-frame animated render, validating I2C timing and display refresh:

  ![OLED demo](gif/oled_demo.gif)

- SPI loopback (4-byte pattern, `HAL_SPI_TransmitReceive`) confirmed **PASS** on both the OLED readout and UART hex dump — including tracking down a real bug along the way (raw binary bytes were being sent over UART instead of formatted ASCII hex).
- Same PA5 blink reproduced with direct register access (`RCC->AHB1ENR`, `GPIOA->MODER`, `GPIOA->ODR`) instead of `HAL_GPIO_TogglePin`, confirming the two are equivalent at the hardware level and building a working mental model of clock-gating and the MODER 2-bits-per-pin layout.

## Tech Stack

C, STM32 HAL + direct register access, STM32CubeIDE/CubeMX, NUCLEO-F411RE (STM32F411RET6, ARM Cortex-M4), SSD1306 OLED, PuTTY, ST-LINK/GDB.

## How to Run

1. Open the project in STM32CubeIDE (or regenerate via `Blink_test.ioc` in CubeMX if peripheral config needs to change).
2. Build and flash to a NUCLEO-F411RE over ST-LINK.
3. Open a serial terminal (PuTTY/Tera Term) at 115200 baud on the ST-LINK VCP to see UART output.
4. Press the user button (B1) to trigger the UART message + LED toggle; the OLED shows the SPI loopback result once wired up (MOSI/MISO jumpered for loopback).

## Development Log

### Day 1: Blink + Toolchain Sanity Check
- LED (LD2) on PA5 toggled via `HAL_GPIO_TogglePin`
- Verified build/flash pipeline via STM32CubeIDE + ST-LINK
- Confirmed onboard LED blinks at 500ms interval

### Day 2: UART + PuTTY for serial output
- UART message output via `HAL_UART_Transmit` when Button (B1) pressed on PC13 via `HAL_GPIO_ReadPin`
- Also toggled LD2 on button press; verified serial output on PuTTY

### Day 3: I2C + SSD1306 GIF animation
- Enabled I2C1 (PB6=SCL, PB7=SDA)
- Integrated the SSD1306 OLED driver library (HAL-based)
- Frame-by-frame animated GIF rendered to the OLED buffer to validate I2C communication, timing, and display refresh logic

*(Day 4 was a review/rest day — no code changes.)*

### Day 5: SPI Communication
- Enabled SPI loopback test (MOSI jumpered to MISO)
- Encountered CubeMX pin conflicts (alternate SPI pins required Morpho connector access)
- Debugged an `FF FF FF FF` RX issue — traced to NSS/CS signal handling
- Implemented `HAL_SPI_TransmitReceive` with a 4-byte loopback pattern
- Verified pass/fail via `memcmp`, displayed on the SSD1306 OLED
- Fixed a UART hex-formatting bug: raw binary bytes were being sent directly instead of formatted ASCII hex strings
- Result: SPI loopback confirmed PASS on both OLED and UART (PuTTY)

### Days 6-7: Register-Level Deep Dive
- Rewrote the PA5 blink using direct register access instead of HAL:
  - `RCC->AHB1ENR |= (1 << 0);` — enable the GPIOA clock
  - `GPIOA->MODER` cleared then set to `01` for pin 5 (output mode)
  - `GPIOA->ODR ^= (1 << 5);` — toggle the output directly
- Traced through `HAL_GPIO_Init()`'s own source to see exactly which registers it touches, using the HAL itself as a register-level reference
- Takeaway that generalizes beyond GPIO: always check the peripheral's clock-enable bit first — most "nothing happens" bugs on a fresh peripheral trace back to a clock that was never gated on

## What's Next

- **CAN bus (next up):** wire an MCP2515 CAN controller to this board over the already-validated SPI1 pins, send a real CAN frame, and receive it on a second microcontroller — the actual automotive-relevant protocol this "ADAS alert" framing has been building toward
- Move the alert logic into a proper multi-task structure (FreeRTOS) instead of a single polling loop, once a second board is talking to this one over CAN
- Extend the SPI/I2C bus work into an actual sensor driver (e.g. an IMU or distance sensor) as a real ADAS input, rather than the loopback test standing in for one
