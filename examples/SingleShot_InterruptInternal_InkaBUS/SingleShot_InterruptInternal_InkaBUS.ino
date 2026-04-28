/*
 * SingleShot_InterruptInternal_InkaBUS.ino
 *
 * Example sketch demonstrating internal GPIO interrupt mode usage of the MMJ_InkaAnalogInputs library
 * with InkaBUS (MikroBUS) differential analog input module.
 *
 * This example shows how to:
 * - Initialize the ADS1115 ADC with internal interrupt handling
 * - Use digitalRead() polling on ALRT/RDY pin for conversion completion
 * - Read differential analog channels in current mode
 * - Handle ESP32 strapping pin conflicts
 *
 * Hardware Requirements:
 * - ESP32 or compatible Arduino board
 * - InkaBUS analog input module (MikroBUS layout)
 * - Jumper J_SHUNT closed for current mode (0-24 mA)
 * - ADS1115 ALRT/RDY pin connected to GPIO 14
 * - I2C connections: SDA to GPIO 32, SCL to GPIO 33 (custom pins to avoid conflicts)
 *
 * Wiring Notes:
 * - GPIO 15 is used as strapping pin workaround (pulled LOW during setup)
 * - GPIO 14 connected to ADS1115 ALRT/RDY for interrupt signaling
 * - Custom I2C pins (32, 33) to avoid ESP32 default pins that may conflict
 *
 * Expected Output:
 * Raw ADC counts for differential channels printed every second.
 * Internal interrupt mode avoids repeated I2C polling during conversion wait.
 */

#include <Wire.h>
#include "MMJ_InkaBUS_AnalogInputs.h"

// Pin definitions
#define IRQ_PIN       14  // GPIO connected to ADS1115 ALRT/RDY pin
#define STRAPPING_PIN 15  // ESP32 strapping pin workaround

void setup() {
    // Initialize serial communication for debugging
    Serial.begin(115200);
    delay(1000);

    // ESP32 strapping pin workaround: GPIO 15 must be LOW during boot
    // We set it as output LOW to prevent floating state issues
    pinMode(STRAPPING_PIN, OUTPUT);
    digitalWrite(STRAPPING_PIN, LOW);

    // Initialize I2C with custom pins to avoid ESP32 default conflicts
    // ESP32 default I2C: SDA=21, SCL=22 (may conflict with other peripherals)
    Wire.begin(32, 33, 100000);  // SDA=32, SCL=33, 100kHz

    // Initialize ADS1115 for InkaBUS with internal interrupt mode
    // - SINGLE mode: one conversion per read call
    // - RATE_32SPS: 32 samples/second (faster than default 128 SPS)
    // - IRQ_PIN: GPIO for ALRT/RDY monitoring
    // - IRQ_INTERNAL: driver waits on GPIO pin (avoids I2C polling)
    if (!InkaBUS_AnalogInputInit(INKA_AI_I2C_ADDRESS_0,
                                 SINGLE,
                                 RATE_32SPS,
                                 IRQ_PIN,
                                 IRQ_INTERNAL)) {
        Serial.println("ADS1115 initialization failed!");
        while (1);
    }

    Serial.println("ADS1115 initialized with internal interrupt mode");
    Serial.println("Reading differential channels 0 and 1...");
}

void loop() {
    // Read differential channel 0 (AIN0 - AIN1) in current mode
    // In IRQ_INTERNAL mode, the driver waits for ALRT/RDY pin assertion
    // This avoids continuous I2C polling during conversion (more efficient)
    int16_t rawADC0 = InkaBUS_AnalogRead(ANALOG_MODE_CURRENT, 0);

    // Read differential channel 1 (AIN2 - AIN3) in current mode
    int16_t rawADC1 = InkaBUS_AnalogRead(ANALOG_MODE_CURRENT, 1);

    // Error checking
    if (rawADC0 == INKA_AI_ERROR || rawADC1 == INKA_AI_ERROR) {
        Serial.println("Read error! Check connections.");
    } else {
        // Display raw ADC values
        Serial.printf("CH0 (AIN0-AIN1): %d | CH1 (AIN2-AIN3): %d\n", rawADC0, rawADC1);
    }

    // Wait before next reading cycle
    delay(1000);
}
