\# Embedded ADAS Alert Pipeline



Board: NUCLEO-F411RE (STM32F411RET6)



\## Day 1: Blink + Toolchain Sanity Check
\- LED (LD2) on PA5 toggled via HAL_GPIO_TogglePin
\- Verified build/flash pipeline via STM32CubeIDE + ST-LINK
\- Confirmed onboard LED blinks at 500ms interval

\## Day 2 : UART + PuTTY for serial output
\- UART message output via HAL_UART_Transmit when Button (B1) press on PC13 via HAL_GPIO_ReadPin.
\- Also toggled (LD2) at button (B1) press. Verified Serial output message on PuTTY.

\## Day 3 : I2C + SSD1306 GIF animation
\- Enabled I2C1 (PB6=SCL, PB7=SDA)
\- Integrated SSD1306 OLED driver library (HAL-based)
\- Frame-by-frame animated GIF rendered to the OLED buffer to validate I2C communication, timing, and display refresh logic
