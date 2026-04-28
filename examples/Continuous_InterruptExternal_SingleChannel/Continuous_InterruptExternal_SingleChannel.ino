/*
 * Continuous_InterruptExternal_SingleChannel.ino
 *
 * Example sketch demonstrating continuous conversion mode with external interrupt
 * using the MMJ_InkaAnalogInputs library with InkaBUS (MikroBUS) module.
 *
 * This example shows how to:
 * - Initialize the ADS1115 in continuous conversion mode
 * - Configure continuous sampling on a single differential channel
 * - Use external ISR for high-frequency data acquisition
 * - Read continuous ADC results without restarting conversions
 *
 * Hardware Requirements:
 * - ESP32 or compatible Arduino board
 * - InkaBUS analog input module (MikroBUS layout)
 * - Jumper J_SHUNT closed for current mode (0-24 mA)
 * - ADS1115 ALRT/RDY pin connected to GPIO 14
 * - I2C connections: SDA to GPIO 32, SCL to GPIO 33
 *
 * Wiring Notes:
 * - GPIO 14 connected to ADS1115 ALRT/RDY for conversion ready signal
 * - GPIO 15 used as ESP32 strapping pin workaround
 * - Channel 1 selected (AIN2-AIN3 differential pair)
 *
 * Operation:
 * - ADS1115 continuously converts at RATE_8SPS (8 samples/second)
 * - ISR signals each completed conversion
 * - Main loop reads results as fast as conversions complete
 * - Ideal for single-channel high-frequency monitoring
 *
 * Note: In continuous mode, only one channel can be active.
 * To change channels, call InkaBUS_AnalogConfigureContinuous() again.
 */

#include <Wire.h>
#include "MMJ_InkaBUS_AnalogInputs.h"

// Pin definitions
#define IRQ_PIN       14  // GPIO connected to ADS1115 ALRT/RDY
#define STRAPPING_PIN 15  // ESP32 strapping pin workaround

// ISR flag for conversion ready
volatile bool ready = false;

// Interrupt Service Routine for conversion completion
void IRAM_ATTR isr_ready() {
    ready = true;  // Signal main loop that data is available
}

void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    delay(1000);

    // ESP32 strapping pin workaround
    pinMode(STRAPPING_PIN, OUTPUT);
    digitalWrite(STRAPPING_PIN, LOW);

    // Initialize I2C with custom pins to avoid conflicts
    Wire.begin(32, 33, 100000);  // SDA=32, SCL=33, 100kHz

    // Initialize ADS1115 for continuous mode with external interrupts
    // - CONTINUOUS: free-running conversions at selected rate
    // - RATE_8SPS: 8 samples per second (slowest, lowest noise)
    // - IRQ_EXTERNAL: ISR-driven, non-blocking
    if (!InkaBUS_AnalogInputInit(INKA_AI_I2C_ADDRESS_0,
                                 CONTINUOUS,
                                 RATE_8SPS,
                                 IRQ_PIN,
                                 IRQ_EXTERNAL)) {
        Serial.println("ADS1115 initialization failed!");
        while (1);
    }

    // Configure continuous conversion on differential channel 1 (AIN2-AIN3)
    // This sets up the ADC for free-running mode on the specified channel
    if (!InkaBUS_AnalogConfigureContinuous(ANALOG_MODE_CURRENT, 1)) {
        Serial.println("Continuous mode configuration failed!");
        while (1);
    }

    // Attach external interrupt to ALRT/RDY pin
    // FALLING edge triggers when each conversion completes
    attachInterrupt(digitalPinToInterrupt(IRQ_PIN), isr_ready, FALLING);

    Serial.println("Continuous mode initialized on channel 1 (AIN2-AIN3)");
    Serial.println("ADC converting at 8 SPS, ISR signaling each sample...");
}

void loop() {
    // Check if ISR has signaled a completed conversion
    if (ready) {
        ready = false;  // Clear flag

        // Read the latest conversion result
        // In continuous mode, no configuration writes needed between reads
        int16_t raw = InkaBUS_AnalogReadResult();

        // Error checking
        if (raw == INKA_AI_ERROR) {
            Serial.println("Read error!");
        } else {
            // Print raw ADC value
            // For 0-24 mA signals, expect values in 0-30720 range
            Serial.println(raw);

            // Optional: Add timestamp or scaling
            // Serial.printf("Time: %lu ms, Raw: %d\n", millis(), raw);
            // float mA = InkaBUS_AnalogReadScaled(raw, 0.0f, 24.0f, 0, 30720);
            // Serial.printf("%.2f mA\n", mA);
        }
    }

    // Main loop can perform other tasks
    // Conversion timing is controlled by ADC sample rate (8 SPS = ~125ms intervals)
}