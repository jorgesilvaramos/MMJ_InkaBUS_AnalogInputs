/*
 * SingleShot_Polling_InkaLogic_Pro.ino
 *
 * Example sketch demonstrating basic polling mode usage of the MMJ_InkaAnalogInputs library
 * with InkaLogic Pro single-ended analog input module.
 *
 * This example shows how to:
 * - Initialize the ADS1115 ADC for InkaLogic Pro module
 * - Read all 4 single-ended analog channels in current mode using polling
 * - Display raw ADC values via serial monitor
 * - Implement proper error checking for I2C communication
 *
 * Hardware Requirements:
 * - ESP32 or compatible Arduino board
 * - InkaLogic Pro analog input module (4 single-ended channels)
 * - Jumper J_SHUNT closed for current mode (0-24 mA)
 * - Jumper J_SHUNT open for voltage mode (0-10 V)
 * - I2C connections: SDA to GPIO 21, SCL to GPIO 22 (default ESP32)
 *
 * Wiring:
 * - Connect ADS1115 ALRT/RDY pin to GPIO (not used in polling mode)
 * - Ensure proper I2C pull-up resistors (typically 4.7kΩ on SDA and SCL)
 * - Connect each analog signal to one of the single-ended inputs (AIN0-AIN3)
 *
 * Expected Output:
 * Raw ADC counts for all 4 single-ended channels (AIN0 to AIN3 vs GND) printed every second.
 * For 0-24 mA signals with J_SHUNT closed, expect values in range 0-30720.
 * For 0-10 V signals with J_SHUNT open, expect values in range 0-32000.
 */

#include <Wire.h>
#include "MMJ_InkaBUS_AnalogInputs.h"

void setup() {
    // Initialize serial communication for debugging
    Serial.begin(115200);
    delay(1000);  // Allow serial monitor to connect

    // Initialize I2C bus with default ESP32 pins
    // Wire.begin(SDA, SCL, frequency)
    Wire.begin(21, 22, 100000);  // SDA=21, SCL=22, 100kHz I2C clock

    // Initialize ADS1115 with default settings for InkaLogic Pro
    // This automatically sets DEVICE_INKALOGIC_PRO for single-ended channels
    if (!InkaLogic_AnalogInputInit()) {
        Serial.println("ADS1115 initialization failed! Check I2C connections.");
        while (1);  // Halt execution on failure
    }

    Serial.println("ADS1115 initialized successfully for InkaLogic Pro");
    Serial.println("Reading 4 single-ended channels in current mode...");
    Serial.println("Format: CH0 (AIN0-GND) | CH1 (AIN1-GND) | CH2 (AIN2-GND) | CH3 (AIN3-GND)");
}

void loop() {
    // Read single-ended channel 0 (AIN0 vs GND) in current mode
    // Returns raw ADC counts (0-32767) or INKA_AI_ERROR (-1000) on failure
    int16_t rawADC0 = InkaLogic_AnalogRead(ANALOG_MODE_CURRENT, 0);

    // Read single-ended channel 1 (AIN1 vs GND) in current mode
    int16_t rawADC1 = InkaLogic_AnalogRead(ANALOG_MODE_CURRENT, 1);

    // Read single-ended channel 2 (AIN2 vs GND) in current mode
    int16_t rawADC2 = InkaLogic_AnalogRead(ANALOG_MODE_CURRENT, 2);

    // Read single-ended channel 3 (AIN3 vs GND) in current mode
    int16_t rawADC3 = InkaLogic_AnalogRead(ANALOG_MODE_CURRENT, 3);

    // Check for I2C communication errors
    if (rawADC0 == INKA_AI_ERROR || rawADC1 == INKA_AI_ERROR ||
        rawADC2 == INKA_AI_ERROR || rawADC3 == INKA_AI_ERROR) {
        Serial.println("I2C communication error! Check wiring and power.");
    } else {
        // Print raw ADC values for all 4 single-ended channels
        // For 0-24 mA signals with J_SHUNT closed, expect 0-30720 range
        Serial.printf("CH0: %d | CH1: %d | CH2: %d | CH3: %d\n", rawADC0, rawADC1, rawADC2, rawADC3);

        // Optional: Scale to engineering units for any channel
        // Example for channel 0: convert to 0-24 mA
        // float mA0 = InkaLogic_AnalogReadScaled(rawADC0, 0.0f, 24.0f, 0, 30720);
        // Serial.printf("CH0: %.2f mA\n", mA0);
        
        // Example for channel 1: convert to 0-10 V (with J_SHUNT open)
        // float volts1 = InkaLogic_AnalogReadScaled(rawADC1, 0.0f, 10.0f, 0, 32000);
        // Serial.printf("CH1: %.2f V\n", volts1);
    }

    // Wait 1 second before next reading cycle
    delay(1000);
}
