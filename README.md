# MMJ_InkaAnalogInputs Library for Arduino

A comprehensive driver library for the ADS1115 16-bit ADC used in Inka analog input modules. This library supports two physical board layouts (INKALOGIC_PRO and MIKROBUS) and provides both generic and branded APIs for easy integration into Arduino projects. The branded APIs (`InkaBUS_*` for MIKROBUS differential channels and `InkaLogic_*` for INKALOGIC_PRO single-ended channels) automatically handle device configuration, eliminating manual device selection.

## Features

- **Dual Board Support**: Automatic configuration for INKALOGIC_PRO (4 single-ended channels) and MIKROBUS (2 differential channels).
- **Signal Modes**: Support for industrial voltage (0–10 V) and current (0–24 mA) signals with appropriate PGA ranges.
- **Conversion Modes**: Single-shot and continuous conversion modes.
- **Interrupt Handling**: Polling, internal GPIO wait, and external ISR-driven modes.
- **Branded APIs**: Dedicated `InkaBUS_*` and `InkaLogic_*` function sets for each board type, automatically managing device layout.
- **Calibration**: Built-in linear scaling function for engineering unit conversion.
- **Arduino Compatible**: Works with ESP32 and other Arduino-compatible boards with I2C support.

## Hardware Support

### DEVICE_INKALOGIC_PRO (InkaLogic)
- **Channels**: 4 single-ended inputs (AIN0, AIN1, AIN2, AIN3 vs GND).
- **Signal Conditioning**: 1/5 voltage divider for 0–10 V or 0–24 mA with 200 Ω shunt.
- **PGA Ranges**: ±2.048 V (voltage) or ±1.024 V (current).
- **Use Case**: Multi-channel single-ended applications.

### DEVICE_MIKROBUS (InkaBUS)
- **Channels**: 2 differential inputs (AIN0−AIN1, AIN2−AIN3).
- **Signal Conditioning**: Same as above.
- **PGA Ranges**: Same as above.
- **Use Case**: Differential measurements, noise-immune applications.

## Installation

1. Download or clone this repository.
2. Copy the library files to your Arduino libraries folder:
   - `MMJ_InkaBUS_AnalogInputs.h`
   - `MMJ_InkaBUS_AnalogInputs.cpp`
3. Restart the Arduino IDE.
4. Include the library in your sketch: `#include "MMJ_InkaBUS_AnalogInputs.h"`

Ensure your board has I2C support and call `Wire.begin()` in `setup()`.

## Usage

### Basic Initialization (Generic API)

```cpp
#include <Wire.h>
#include "MMJ_InkaBUS_AnalogInputs.h"

void setup() {
    Serial.begin(115200);
    Wire.begin();  // Initialize I2C

    // Initialize ADS1115 with default settings
    if (!Inka_AnalogInputInit()) {
        Serial.println("ADS1115 initialization failed!");
        while (1);
    }

    // Set device type (required before first read)
    Inka_AnalogInSetDevice(DEVICE_INKALOGIC_PRO);  // or DEVICE_MIKROBUS
}
```

### Reading Analog Values (Generic API)

```cpp
void loop() {
    // Read channel 0 in current mode
    int16_t raw = Inka_AnalogRead(ANALOG_MODE_CURRENT, 0);

    if (raw != INKA_AI_ERROR) {
        // Scale to engineering units (0-24 mA)
        float mA = Inka_AnalogReadScaled(raw, 0.0f, 24.0f, 0, 30720);
        Serial.printf("Current: %.2f mA\n", mA);
    }

    delay(1000);
}
```

### Branded API Usage (Recommended)

For cleaner code and automatic device management, use the branded APIs:

#### InkaBUS (MikroBUS layout)

```cpp
#include <Wire.h>
#include "MMJ_InkaBUS_AnalogInputs.h"

void setup() {
    Wire.begin();
    InkaBUS_AnalogInputInit();  // Automatically handles MIKROBUS
}

void loop() {
    int16_t raw = InkaBUS_AnalogRead(ANALOG_MODE_CURRENT, 0);  // Channel 0: AIN0-AIN1
    float mA = InkaBUS_AnalogReadScaled(raw, 0.0f, 24.0f, 0, 30720);
    Serial.printf("Differential Current: %.2f mA\n", mA);
    delay(1000);
}
```

#### InkaLogic (InkaLogic Pro layout)

```cpp
#include <Wire.h>
#include "MMJ_InkaBUS_AnalogInputs.h"

void setup() {
    Wire.begin();
    InkaLogic_AnalogInputInit();  // Automatically handles INKALOGIC_PRO
}

void loop() {
    int16_t raw = InkaLogic_AnalogRead(ANALOG_MODE_VOLT, 2);  // Channel 2: AIN2 vs GND
    float volts = InkaLogic_AnalogReadScaled(raw, 0.0f, 10.0f, 0, 32000);
    Serial.printf("Voltage: %.2f V\n", volts);
    delay(1000);
}
```

### Interrupt-Driven Mode

```cpp
volatile bool conversionReady = false;

void IRAM_ATTR onConversionComplete() {
    conversionReady = true;
}

void setup() {
    Wire.begin();
    InkaBUS_AnalogInputInit(INKA_AI_I2C_ADDRESS_0, SINGLE, RATE_128SPS, 39, IRQ_EXTERNAL);

    attachInterrupt(digitalPinToInterrupt(39), onConversionComplete, FALLING);
}

void loop() {
    if (conversionReady) {
        conversionReady = false;
        InkaBUS_AnalogStartConversion(ANALOG_MODE_CURRENT, 0);

        // Wait for ISR
        while (!conversionReady);
        conversionReady = false;

        int16_t raw = InkaBUS_AnalogReadResult();
        float mA = InkaBUS_AnalogReadScaled(raw, 0.0f, 24.0f, 0, 30720);
        Serial.printf("ISR Current: %.2f mA\n", mA);
    }
}
```

## API Reference

### Generic API (Inka_ prefix)

#### Initialization
- `Inka_AnalogInputInit(addr, mode, data_rate, irq_pin, irq_mode)`: Initialize ADS1115.
- `Inka_AnalogInSetDevice(device)`: Set board layout (DEVICE_MIKROBUS or DEVICE_INKALOGIC_PRO).

#### Reading
- `Inka_AnalogRead(mode, channel)`: Blocking read with configuration.
- `Inka_AnalogStartConversion(mode, channel)`: Start non-blocking conversion (IRQ_EXTERNAL).
- `Inka_AnalogReadResult()`: Read result after conversion complete.
- `Inka_AnalogConfigureContinuous(mode, channel)`: Setup continuous mode.

#### Utilities
- `Inka_AnalogReadScaled(rawADC, y1, y2, x1, x2)`: Linear scaling to engineering units.

### Branded APIs

#### InkaBUS (MikroBUS, 2 differential channels)
- `InkaBUS_AnalogInputInit(...)`: Initialize for MIKROBUS.
- `InkaBUS_AnalogRead(mode, channel)`: Read differential channel (0 or 1).
- `InkaBUS_AnalogStartConversion(mode, channel)`: Start conversion.
- `InkaBUS_AnalogReadResult()`: Read result.
- `InkaBUS_AnalogConfigureContinuous(mode, channel)`: Continuous mode.
- `InkaBUS_AnalogReadScaled(...)`: Scaling utility.

#### InkaLogic (InkaLogic Pro, 4 single-ended channels)
- `InkaLogic_AnalogInputInit(...)`: Initialize for INKALOGIC_PRO.
- `InkaLogic_AnalogRead(mode, channel)`: Read single-ended channel (0-3).
- `InkaLogic_AnalogStartConversion(mode, channel)`: Start conversion.
- `InkaLogic_AnalogReadResult()`: Read result.
- `InkaLogic_AnalogConfigureContinuous(mode, channel)`: Continuous mode.
- `InkaLogic_AnalogReadScaled(...)`: Scaling utility.

### Enums and Constants

- `device_t`: `DEVICE_MIKROBUS`, `DEVICE_INKALOGIC_PRO`
- `analog_mode_t`: `ANALOG_MODE_CURRENT`, `ANALOG_MODE_VOLT`
- `read_mode_t`: `SINGLE`, `CONTINUOUS`
- `irq_mode_t`: `IRQ_NONE`, `IRQ_INTERNAL`, `IRQ_EXTERNAL`
- Data rates: `RATE_8SPS` to `RATE_860SPS`
- I2C addresses: `INKA_AI_I2C_ADDRESS_0`, `INKA_AI_I2C_ADDRESS_1`
- Error code: `INKA_AI_ERROR` (-1000)

## Examples

Six example sketches are provided in the `examples/` directory:

### InkaBUS Examples (MikroBUS differential channels):
1. **SingleShot_Polling_InkaBUS**: Basic polling mode for MIKROBUS differential inputs (channels 0-1).
2. **SingleShot_InterruptInternal_InkaBUS**: Internal GPIO interrupt for MIKROBUS.
3. **SingleShot_InterruptExternal_InkaBUS**: External ISR-driven for MIKROBUS, alternating channels.

### InkaLogic Examples (InkaLogic Pro single-ended channels):
4. **SingleShot_Polling_InkaLogic_Pro**: Basic polling mode for all 4 single-ended channels (AIN0-AIN3 vs GND).
5. **SingleShot_InterruptExternal_InkaLogic_Pro**: External ISR-driven for InkaLogic Pro, cycling through all 4 channels.

### Continuous Mode:
6. **Continuous_InterruptExternal_SingleChannel**: Continuous mode with external interrupt (works with both device types).

Each example includes detailed comments explaining the usage, hardware requirements, and expected output.

## Calibration Constants

### Voltage Mode (0–10 V, J_SHUNT open)
- Raw range: 0 to 32000 counts
- Scaling: `Inka_AnalogReadScaled(raw, 0.0f, 10.0f, 0, 32000)`

### Current Mode (0–24 mA, J_SHUNT closed)
- Raw range: 0 to 30720 counts
- Scaling: `Inka_AnalogReadScaled(raw, 0.0f, 24.0f, 0, 30720)`

## Dependencies

- Arduino framework
- Wire library (I2C)
- math.h (for scaling)

## Hardware Notes

- **Jumper J_SHUNT**: Must be CLOSED for current mode, OPEN for voltage mode.
- **I2C Pull-ups**: Ensure proper pull-up resistors on SDA/SCL lines.
- **Interrupt Pin**: Connect ADS1115 ALRT/RDY to GPIO for interrupt modes.
- **Power Supply**: ADS1115 requires stable 3.3V or 5V supply.

## License

This project is released under the MIT License. See LICENSE file for details.

## Contributing

Contributions are welcome! Please submit issues and pull requests on GitHub.

## Support

For questions or issues, please open an issue on the GitHub repository.