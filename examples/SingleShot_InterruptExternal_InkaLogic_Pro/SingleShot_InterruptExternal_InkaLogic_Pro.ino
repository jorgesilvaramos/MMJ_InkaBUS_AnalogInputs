/*
 * SingleShot_InterruptExternal_InkaLogic_Pro.ino
 *
 * Example sketch demonstrating external ISR-driven mode usage of the MMJ_InkaAnalogInputs library
 * with InkaLogic Pro single-ended analog input module.
 *
 * This example shows how to:
 * - Initialize the ADS1115 ADC with external interrupt handling for InkaLogic Pro
 * - Use ISR (Interrupt Service Routine) for conversion completion signaling
 * - Alternate between all 4 single-ended channels in a round-robin fashion
 * - Implement non-blocking analog acquisition for RTOS or time-critical applications
 *
 * Hardware Requirements:
 * - ESP32 or compatible Arduino board
 * - InkaLogic Pro analog input module (4 single-ended channels: AIN0-AIN3 vs GND)
 * - Jumper J_SHUNT closed for current mode (0-24 mA) or open for voltage mode (0-10 V)
 * - ADS1115 ALRT/RDY pin connected to GPIO 34
 * - I2C connections: SDA to GPIO 21, SCL to GPIO 22 (default ESP32)
 *
 * Wiring Notes:
 * - GPIO 34 connected to ADS1115 ALRT/RDY for external interrupt
 * - Ensure proper I2C pull-up resistors (4.7kΩ recommended)
 * - Connect analog signals to single-ended inputs AIN0-AIN3
 *
 * Operation:
 * - ISR sets flag when each conversion completes
 * - Main loop processes result and starts next conversion on next channel
 * - Channels cycle: 0 (AIN0-GND) → 1 (AIN1-GND) → 2 (AIN2-GND) → 3 (AIN3-GND) → 0...
 * - No blocking delays in main loop (ideal for multitasking)
 *
 * Note: Uses slow sample rate (8 SPS) for demonstration. Increase RATE_xxxSPS for faster acquisition.
 */

#include <Wire.h>
#include "MMJ_InkaBUS_AnalogInputs.h"

// Pin definitions
#define IRQ_PIN 34  // GPIO connected to ADS1115 ALRT/RDY pin

// Global variables for channel cycling and ISR flag
uint8_t current_ch = 0;           // Current channel being read (0-3)
volatile bool conv_ready = false; // ISR flag: true when conversion complete

// Interrupt Service Routine (ISR) for conversion ready signal
// Marked IRAM_ATTR to place in fast IRAM memory (ESP32 specific)
void IRAM_ATTR isr_conv_ready() {
    conv_ready = true;  // Set flag for main loop to process
}

void setup() {
    // Initialize serial communication for debugging output
    Serial.begin(115200);
    delay(1000);

    // Initialize I2C bus with default ESP32 pins
    // Wire.begin() uses default SDA=21, SCL=22
    Wire.begin();

    // Initialize ADS1115 for external interrupt mode with InkaLogic Pro
    // - SINGLE mode: one conversion per trigger
    // - RATE_8SPS: 8 samples per second (slowest, lowest noise for demo)
    // - IRQ_PIN: GPIO for ALRT/RDY monitoring
    // - IRQ_EXTERNAL: non-blocking, ISR-driven
    if (!InkaLogic_AnalogInputInit(INKA_AI_I2C_ADDRESS_0,
                                   SINGLE,
                                   RATE_8SPS,
                                   IRQ_PIN,
                                   IRQ_EXTERNAL)) {
        Serial.println("ADS1115 initialization failed!");
        while (1);
    }

    // Attach external interrupt to ALRT/RDY pin
    // FALLING edge: pin goes LOW when conversion ready
    attachInterrupt(digitalPinToInterrupt(IRQ_PIN), isr_conv_ready, FALLING);

    // Start first conversion on channel 0 (AIN0 vs GND)
    InkaLogic_AnalogStartConversion(ANALOG_MODE_CURRENT, current_ch);

    Serial.println("External interrupt mode initialized for InkaLogic Pro");
    Serial.println("Cycling through 4 single-ended channels (0-3)...");
    Serial.println("Format: CH0: raw_value | CH1: raw_value | etc.");
}

void loop() {
    // Check if ISR has signaled conversion completion
    if (conv_ready) {
        conv_ready = false;  // Clear flag immediately

        // Read the conversion result (no I2C config needed)
        int16_t raw = InkaLogic_AnalogReadResult();

        // Error checking
        if (raw == INKA_AI_ERROR) {
            Serial.println("Read error! Check connections.");
        } else {
            // Display result with channel number
            // For 0-24 mA signals, expect values in 0-30720 range
            Serial.printf("CH%u: %d\n", current_ch, raw);
        }

        // Alternate to next channel (0 → 1 → 2 → 3 → 0...)
        current_ch = (current_ch + 1) % 4;

        // Start conversion on next channel
        // This is non-blocking; ISR will signal when ready
        InkaLogic_AnalogStartConversion(ANALOG_MODE_CURRENT, current_ch);
    }

    // Main loop remains free for other tasks
    // Conversion timing controlled by ADC sample rate (8 SPS ≈ 125ms per channel)
    // Total cycle time: 4 channels × 125ms = ~500ms per complete scan
}