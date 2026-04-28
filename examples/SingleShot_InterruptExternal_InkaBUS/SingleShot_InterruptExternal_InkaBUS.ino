/*
 * SingleShot_InterruptExternal_InkaBUS.ino
 *
 * Example sketch demonstrating external ISR-driven mode usage of the MMJ_InkaAnalogInputs library
 * with InkaBUS (MikroBUS) differential analog input module.
 *
 * This example shows how to:
 * - Initialize the ADS1115 ADC with external interrupt handling
 * - Use ISR (Interrupt Service Routine) for conversion completion signaling
 * - Alternate between differential channels in a round-robin fashion
 * - Implement non-blocking analog acquisition for RTOS or time-critical applications
 *
 * Hardware Requirements:
 * - ESP32 or compatible Arduino board
 * - InkaBUS analog input module (MikroBUS layout)
 * - Jumper J_SHUNT closed for current mode (0-24 mA)
 * - ADS1115 ALRT/RDY pin connected to GPIO 14
 * - I2C connections: SDA to GPIO 32, SCL to GPIO 33
 *
 * Wiring Notes:
 * - GPIO 14 connected to ADS1115 ALRT/RDY for external interrupt
 * - GPIO 15 used as strapping pin workaround
 * - Custom I2C pins to avoid ESP32 default pin conflicts
 *
 * Operation:
 * - ISR sets flag when conversion completes
 * - Main loop processes result and starts next conversion
 * - Channels alternate: 0 (AIN0-AIN1) → 1 (AIN2-AIN3) → 0...
 * - No blocking delays in main loop (ideal for multitasking)
 */

#include <Wire.h>
#include "MMJ_InkaBUS_AnalogInputs.h"

// Pin definitions
#define IRQ_PIN       14  // GPIO connected to ADS1115 ALRT/RDY
#define STRAPPING_PIN 15  // ESP32 strapping pin workaround

// Global variables for channel cycling and ISR flag
uint8_t current_ch = 0;           // Current channel being read (0 or 1)
volatile bool conv_ready = false; // ISR flag: true when conversion complete

// Interrupt Service Routine (ISR) for conversion ready signal
// Marked IRAM_ATTR to place in fast IRAM memory (ESP32 specific)
void IRAM_ATTR isr_conv_ready() {
    conv_ready = true;  // Set flag for main loop to process
}

void setup() {
    // Initialize serial for debugging output
    Serial.begin(115200);
    delay(1000);

    // ESP32 strapping pin workaround
    pinMode(STRAPPING_PIN, OUTPUT);
    digitalWrite(STRAPPING_PIN, LOW);

    // Initialize I2C with custom pins
    Wire.begin(32, 33, 100000);  // SDA=32, SCL=33, 100kHz

    // Initialize ADS1115 for external interrupt mode
    // - SINGLE mode: one conversion per trigger
    // - RATE_8SPS: slow rate for demonstration (lowest noise)
    // - IRQ_EXTERNAL: non-blocking, ISR-driven
    if (!InkaBUS_AnalogInputInit(INKA_AI_I2C_ADDRESS_0,
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

    // Start first conversion on channel 0
    InkaBUS_AnalogStartConversion(ANALOG_MODE_CURRENT, current_ch);

    Serial.println("External interrupt mode initialized");
    Serial.println("Alternating between differential channels 0 and 1...");
}

void loop() {
    // Check if ISR has signaled conversion completion
    if (conv_ready) {
        conv_ready = false;  // Clear flag immediately

        // Read the conversion result (no I2C config needed)
        int16_t raw = InkaBUS_AnalogReadResult();

        // Error checking
        if (raw == INKA_AI_ERROR) {
            Serial.println("Read error!");
        } else {
            // Display result with channel number
            Serial.printf("CH%u: %d\n", current_ch, raw);
        }

        // Alternate to next channel (0 → 1 → 0...)
        current_ch = (current_ch + 1) % 2;

        // Start conversion on next channel
        // This is non-blocking; ISR will signal when ready
        InkaBUS_AnalogStartConversion(ANALOG_MODE_CURRENT, current_ch);
    }

    // Main loop remains free for other tasks
    // No delay() needed - timing controlled by ADC conversion rate
}
