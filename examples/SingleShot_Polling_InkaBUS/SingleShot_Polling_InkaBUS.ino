/*
 * SingleShot_Polling_InkaBUS.ino
 *
 * Example sketch demonstrating basic polling mode usage of the MMJ_InkaAnalogInputs library
 * with InkaBUS (MikroBUS) differential analog input module.
 *
 * This example shows how to:
 * - Initialize the ADS1115 ADC for InkaBUS module
 * - Read differential analog channels in current mode using polling
 * - Display raw ADC values via serial monitor
 *
 * Hardware Requirements:
 * - ESP32 or compatible Arduino board
 * - InkaBUS analog input module (MikroBUS layout)
 * - Jumper J_SHUNT closed for current mode (0-24 mA)
 * - I2C connections: SDA to GPIO 21, SCL to GPIO 22 (default ESP32)
 *
 * Wiring:
 * - Connect ADS1115 ALRT/RDY pin to GPIO (not used in polling mode)
 * - Ensure proper I2C pull-up resistors
 *
 * Expected Output:
 * Raw ADC counts for channels 0 (AIN0-AIN1) and 1 (AIN2-AIN3) printed every second.
 * For 0-24 mA signals, expect values around 0-30720.
 */

#include <Wire.h>
#include "MMJ_InkaBUS_AnalogInputs.h"

void setup() {
    // Initialize serial communication for debugging
    Serial.begin(115200);
    delay(1000);  // Allow serial monitor to connect

    // Initialize I2C bus with custom pins (ESP32 default: SDA=21, SCL=22)
    // Wire.begin(SDA, SCL, frequency)
    Wire.begin(21, 22, 100000);  // 100kHz I2C clock

    // Initialize ADS1115 with default settings for InkaBUS
    // This automatically sets DEVICE_MIKROBUS for differential channels
    if (!InkaBUS_AnalogInputInit()) {
        Serial.println("ADS1115 initialization failed! Check I2C connections.");
        while (1);  // Halt execution on failure
    }

    Serial.println("ADS1115 initialized successfully for InkaBUS (MikroBUS)");
    Serial.println("Reading differential channels 0 and 1 in current mode...");
}

void loop() {
    // Read differential channel 0 (AIN0 - AIN1) in current mode
    // Returns raw ADC counts (0-32767) or INKA_AI_ERROR (-1000) on failure
    int16_t rawADC0 = InkaBUS_AnalogRead(ANALOG_MODE_CURRENT, 0);

    // Read differential channel 1 (AIN2 - AIN3) in current mode
    int16_t rawADC1 = InkaBUS_AnalogRead(ANALOG_MODE_CURRENT, 1);

    // Check for I2C communication errors
    if (rawADC0 == INKA_AI_ERROR || rawADC1 == INKA_AI_ERROR) {
        Serial.println("I2C communication error! Check wiring and power.");
    } else {
        // Print raw ADC values
        // For 0-24 mA signals with J_SHUNT closed, expect 0-30720 range
        Serial.printf("CH0 (AIN0-AIN1): %d | CH1 (AIN2-AIN3): %d\n", rawADC0, rawADC1);

        // Optional: Scale to engineering units (0-24 mA)
        // float mA0 = InkaBUS_AnalogReadScaled(rawADC0, 0.0f, 24.0f, 0, 30720);
        // float mA1 = InkaBUS_AnalogReadScaled(rawADC1, 0.0f, 24.0f, 0, 30720);
        // Serial.printf("CH0: %.2f mA | CH1: %.2f mA\n", mA0, mA1);
    }

    // Wait 1 second before next reading
    delay(1000);
}
