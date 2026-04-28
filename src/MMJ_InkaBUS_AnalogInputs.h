#pragma once

/*
 * MMJ_InkaBUS_AnalogInputs.h
 *
 *  Created on: 2026-04-21
 *  Author:     Jorge A. Silva
 *
 * ─── Description ─────────────────────────────────────────────────────────────
 *
 *  Driver for the ADS1115 16-bit ADC used in Inka analog input modules.
 *  Supports two physical board layouts (INKALOGIC_PRO / MIKROBUS) and two
 *  industrial signal ranges:
 *  
 *    • Voltage mode  (0–10 V)   — jumper J_SHUNT open
 *    • Current mode  (0–24 mA)  — jumper J_SHUNT closed (R_shunt = 200 Ω)
 *
 * ─── Signal Conditioning ─────────────────────────────────────────────────────
 *
 *  Both signal types pass through a 1st-order active low-pass differential
 *  filter with a gain of 1/5 before reaching the ADS1115.
 *
 *  ┌──────────────┬──────────────────────┬───────────────┬──────────────────┐
 *  │ Mode         │ Input range          │ ADS1115 input │ PGA (FSR)        │
 *  ├──────────────┼──────────────────────┼───────────────┼──────────────────┤
 *  │ Voltage      │ 0 – 10 V             │ 0 – 2.000 V   │ ±2.048 V  (×2)  │
 *  │ Current      │ 0 – 24 mA            │ 0 – 0.960 V   │ ±1.024 V  (×3)  │
 *  └──────────────┴──────────────────────┴───────────────┴──────────────────┘
 *
 *  Current path: I_in × 200 Ω → 0–4.8 V → ÷5 filter → 0–0.96 V @ ADS1115
 *  Voltage path: V_in → ÷5 filter → 0–2.0 V @ ADS1115
 *
 * ─── Typical Inka_AnalogReadScaled() calibration constants ───────────────────
 *
 *  Voltage (0–10 V):
 *    x1 = 0,      x2 = 32000    (raw ADC counts, 2.000 V / 2.048 V × 32767)
 *    y1 = 0.0,    y2 = 10.0     (engineering units: V)
 *
 *  Current (0–24 mA):
 *    x1 = 0,      x2 = 30720    (raw ADC counts, 0.960 V / 1.024 V × 32767)
 *    y1 = 0.0,    y2 = 24.0     (engineering units: mA)
 *
 * ─── Dependencies ────────────────────────────────────────────────────────────
 *
 *  Arduino.h, Wire.h, math.h
 *
 * ─── Thread safety ───────────────────────────────────────────────────────────
 *
 *  This driver is NOT re-entrant. Do NOT call its functions from multiple
 *  RTOS tasks simultaneously without an external mutex. In IRQ_EXTERNAL mode,
 *  Inka_AnalogReadResult() may be called from a task context after the ISR
 *  sets the application flag, but never from the ISR itself.
 * ─── Warning ─────────────────────────────────────────────────────────────────
 * The ADS1115 is a multiplexed ADC. It does NOT support simultaneous
 * multi-channel sampling. Channel switching always incurs one conversion
 * latency, regardless of mode.
 */

#include "Arduino.h"
#include "Wire.h"
#include "math.h"

// ─── I2C Address ─────────────────────────────────────────────────────────────
#define INKA_AI_I2C_ADDRESS_0   0x48    // ADDR pin → GND
#define INKA_AI_I2C_ADDRESS_1   0x49    // ADDR pin → VCC

// ─── Register Addresses ──────────────────────────────────────────────────────
#define REG_CONV                0x00    // Conversion result (read-only)
#define REG_CONFIG              0x01    // Configuration
#define REG_LO_THRESH           0x02    // Low threshold (comparator / ALRT mode)
#define REG_HI_THRESH           0x03    // High threshold (comparator / ALRT mode)

// ─── Config register bits ────────────────────────────────────────────────────
#define REG_OS                  (1U << 15)  // Operational status: start single-shot conversion

#define MODE_CONTINUOUS         (0U << 8)   // Device operates continuously
#define MODE_SINGLE_SHOT        (1U << 8)   // Device powers down after each conversion (default)

// ─── MUX: single-ended inputs (used by DEVICE_INKALOGIC_PRO) ─────────────────
#define MUX_AN0                 (0b100U << 12)   // AIN0 vs GND
#define MUX_AN1                 (0b101U << 12)   // AIN1 vs GND
#define MUX_AN2                 (0b110U << 12)   // AIN2 vs GND
#define MUX_AN3                 (0b111U << 12)   // AIN3 vs GND

// ─── MUX: differential inputs (used by DEVICE_MIKROBUS) ──────────────────────
#define MUX_DIFF_AN0            (0b000U << 12)   // AIN0 − AIN1
#define MUX_DIFF_AN1            (0b011U << 12)   // AIN2 − AIN3

// ─── PGA ─────────────────────────────────────────────────────────────────────
//  Voltage mode: max signal at ADS1115 = 10 V / 5 = 2.000 V → FSR ±2.048 V
#define PGA_INDUSTRIAL_VOLTAGE_AI   (0b010U << 9)   // FSR = ±2.048 V (1 LSB = 62.5 µV)

//  Current mode: max signal at ADS1115 = (24 mA × 200 Ω) / 5 = 0.960 V → FSR ±1.024 V
#define PGA_INDUSTRIAL_CURRENT_AI   (0b011U << 9)   // FSR = ±1.024 V (1 LSB = 31.25 µV)

// ─── Data Rate ───────────────────────────────────────────────────────────────
#define RATE_8SPS               (0b000U << 5)   //   8 samples/s — slowest, lowest noise
#define RATE_16SPS              (0b001U << 5)   //  16 samples/s
#define RATE_32SPS              (0b010U << 5)   //  32 samples/s
#define RATE_64SPS              (0b011U << 5)   //  64 samples/s
#define RATE_128SPS             (0b100U << 5)   // 128 samples/s (default)
#define RATE_250SPS             (0b101U << 5)   // 250 samples/s
#define RATE_475SPS             (0b110U << 5)   // 475 samples/s
#define RATE_860SPS             (0b111U << 5)   // 860 samples/s — fastest, highest noise

// ─── Comparator / ALRT queue ─────────────────────────────────────────────────
#define COMP_QUE_DISABLE        (0b11U)     // Disable comparator, ALRT pin high-Z
#define COMP_QUE_1CONV          (0b00U)     // Assert ALRT after 1 conversion (RDY mode)

// ─── Timeout ─────────────────────────────────────────────────────────────────
//  Maximum time to wait for a conversion in IRQ_NONE or IRQ_INTERNAL mode.
//  At the slowest rate (8 SPS), one conversion takes 125 ms, so 500 ms
//  provides a safe 4× margin.
#define INKA_AI_CONV_TIMEOUT_MS 500U

// ─── Error code ──────────────────────────────────────────────────────────────
//  Returned as int16_t by read functions on I2C failure or timeout.
//  The ADS1115 output is a signed 16-bit value in the range −32768 to +32767.
//  −1000 (0xFC18) is outside the expected 0–32767 range for these unipolar
//  Inka inputs, so it can be used unambiguously as a sentinel error value.
#define INKA_AI_ERROR           ((int16_t)(-1000))

// ─── Types ───────────────────────────────────────────────────────────────────

/** Conversion trigger mode for the ADS1115. */
typedef enum {
    SINGLE     = 0,     ///< Single-shot: one conversion per read call, then power-down.
    CONTINUOUS  = 1     ///< Continuous: the device converts repeatedly at the selected rate.
                        ///< @note  Best suited for single-channel applications. When
                        ///<        Inka_AnalogRead() is called with a different channel,
                        ///<        the driver must reconfigure the MUX and discard one
                        ///<        sample, so there is no throughput advantage over SINGLE
                        ///<        in multi-channel scans. For multi-channel use, prefer
                        ///<        SINGLE or SINGLE + IRQ_EXTERNAL.
} read_mode_t;

/** Physical board layout. Determines MUX wiring and available channels. */
typedef enum {
    DEVICE_MIKROBUS      = 0,   ///< 2 differential channels (AIN0−AIN1, AIN2−AIN3)
    DEVICE_INKALOGIC_PRO = 1    ///< 4 single-ended channels (AIN0…AIN3 vs GND)
} device_t;

/**
 * Analog signal type. Selects the PGA range and the expected input scaling.
 *
 *  ANALOG_MODE_CURRENT → PGA ±1.024 V, input: 0–24 mA (via 200 Ω shunt ÷5)
 *  ANALOG_MODE_VOLT    → PGA ±2.048 V, input: 0–10 V  (direct ÷5)
 *
 * @note Jumper J_SHUNT must be CLOSED for current mode and OPEN for voltage mode.
 */
typedef enum {
    ANALOG_MODE_CURRENT = 0,    ///< 0–24 mA industrial current loop (J_SHUNT closed)
    ANALOG_MODE_VOLT    = 1     ///< 0–10 V industrial voltage signal  (J_SHUNT open)
} analog_mode_t;

/**
 * Interrupt (ALRT/RDY pin) handling strategy.
 *
 *  IRQ_NONE      Polling only. Inka_AnalogRead() writes the config, then
 *                polls REG_CONFIG bit 15 (OS) until the conversion is done
 *                or INKA_AI_CONV_TIMEOUT_MS elapses.
 *
 *  IRQ_INTERNAL  Driver waits for the ALRT/RDY GPIO inside Inka_AnalogRead()
 *                using digitalRead(). Avoids repeated I2C transactions during
 *                the wait, but blocks the calling task until the pin asserts.
 *
 *  IRQ_EXTERNAL  Non-blocking, ISR-driven. Intended for RTOS environments or
 *                tight control loops. The recommended flow is:
 *
 *                  1. Inka_AnalogStartConversion(mode, channel)
 *                       → writes config, returns immediately
 *                  2. Your ISR fires when ALRT/RDY asserts
 *                       → sets an application-level flag
 *                  3. Your loop() / task checks the flag
 *                       → calls Inka_AnalogReadResult()
 *                  4. Process the result, then go to step 1 for the next cycle
 *
 *                Do NOT call Inka_AnalogRead() in IRQ_EXTERNAL mode; use
 *                Start + ReadResult instead.
 */
typedef enum {
    IRQ_NONE     = 0,
    IRQ_INTERNAL = 1,
    IRQ_EXTERNAL = 2
} irq_mode_t;

// ─── API ─────────────────────────────────────────────────────────────────────

/**
 * @brief  Initialize the ADS1115 and verify I2C communication.
 *
 * @param  addr       I2C address of the ADS1115.
 *                    Use INKA_AI_I2C_ADDRESS_0 (ADDR→GND) or
 *                    INKA_AI_I2C_ADDRESS_1 (ADDR→VCC).
 *                    Default: INKA_AI_I2C_ADDRESS_0.
 *
* @param  mode       Conversion trigger mode (SINGLE or CONTINUOUS).
 *                    Use SINGLE for on-demand reads or multi-channel scans.
 *                    Use CONTINUOUS only when reading a single fixed channel;
 *                    switching channels in continuous mode forces a MUX
 *                    reconfiguration and one discarded sample per call,
 *                    negating any throughput benefit.
 *                    Default: SINGLE.
 *
 * @param  data_rate  Samples per second. One of the RATE_xSPS macros.
 *                    Lower rates reduce noise; higher rates improve latency.
 *                    Default: RATE_128SPS (128 samples/s, ~7.8 ms/conversion).
 *
 * @param  irq_pin    GPIO number connected to the ADS1115 ALRT/RDY pin, or
 *                    -1 if not used. Required for IRQ_INTERNAL and IRQ_EXTERNAL.
 *                    Default: -1 (no pin).
 *
 * @param  irq_mode   Interrupt strategy. See irq_mode_t documentation.
 *                    Default: IRQ_NONE (polling).
 *
 * @return true   ADS1115 acknowledged on I2C and configuration was written.
 * @return false  I2C communication failed; check wiring and pull-up resistors.
 */
bool Inka_AnalogInputInit(uint8_t     addr      = INKA_AI_I2C_ADDRESS_0,
                          read_mode_t mode      = SINGLE,
                          uint8_t     data_rate = RATE_128SPS,
                          int8_t      irq_pin   = -1,
                          irq_mode_t  irq_mode  = IRQ_NONE);

/**
 * @brief  Select the physical board layout.
 *
 *  This call affects how the channel argument of Inka_AnalogRead() is
 *  mapped to the ADS1115 MUX bits:
 *
 *    DEVICE_INKALOGIC_PRO  channel 0–3 → MUX_AN0 … MUX_AN3  (single-ended)
 *    DEVICE_MIKROBUS       channel 0–1 → MUX_DIFF_AN0, MUX_DIFF_AN1 (differential)
 *
 * @param  device  Board variant. Default: DEVICE_INKALOGIC_PRO.
 *
 * @note   Call this before the first Inka_AnalogRead(). The default is
 *         DEVICE_INKALOGIC_PRO, so this call can be omitted on that board.
 */
void Inka_AnalogInSetDevice(device_t device = DEVICE_INKALOGIC_PRO);


/**
 * @brief  Configure the ADS1115 for continuous conversion on a fixed channel.
 *
 *  This function is intended for high-performance acquisition using
 *  CONTINUOUS + IRQ_EXTERNAL or CONTINUOUS + IRQ_INTERNAL modes.
 *
 *  It writes the MUX, PGA, mode, and data rate once, then lets the ADS1115
 *  run in free-running mode. No further configuration writes are required
 *  for subsequent samples.
 *
 *  Internally, the function:
 *    1. Programs the MUX (channel) and PGA (based on analog_mode_t).
 *    2. Sets MODE_CONTINUOUS and the configured data rate.
 *    3. Waits one full conversion period to allow the ADC to settle after
 *       the MUX/PGA change.
 *    4. Discards the first conversion result, which may contain stale data
 *       from the previous configuration.
 *
 *  After this call, the ADS1115 continuously updates REG_CONV at the
 *  configured sample rate. The application must read samples using:
 *
 *      Inka_AnalogReadResult()
 *
 *  and must NOT call:
 *
 *      Inka_AnalogRead()
 *      Inka_AnalogStartConversion()
 *
 *  while operating in CONTINUOUS mode.
 *
 * @param  mode     Signal type: ANALOG_MODE_CURRENT or ANALOG_MODE_VOLT.
 *                  Selects the PGA range and expected input scaling.
 *
 * @param  channel  Input channel index:
 *                    DEVICE_INKALOGIC_PRO → 0 to 3 (single-ended)
 *                    DEVICE_MIKROBUS      → 0 or 1 (differential)
 *
 * @return true   Configuration was written and the ADC is producing
 *                valid continuous samples.
 * @return false  I2C write/read failed or invalid parameters.
 *
 * @note  This function replaces the need for a manual "priming read"
 *        (dummy call to Inka_AnalogRead()) in application code.
 *
 * @note  Only one channel can be active in continuous mode. To switch
 *        channels, call this function again; one conversion period will
 *        be required to settle the new configuration.
 *
 * @note  At high data rates (e.g., 860 SPS), ensure the I2C bus speed
 *        and host processing can sustain the throughput to avoid
 *        missing samples.
 *
 * @note   No REG_OS bit here — conversions are free-running in continuous mode.
 *
 * @par Typical usage (CONTINUOUS + IRQ_EXTERNAL)
 * @code
 *    Inka_AnalogInputInit(..., CONTINUOUS, RATE_860SPS, irq_pin, IRQ_EXTERNAL);
 *    Inka_AnalogInSetDevice(DEVICE_MIKROBUS);
 *
 *    Inka_AnalogConfigureContinuous(ANALOG_MODE_CURRENT, 0);
 *
 *    attachInterrupt(digitalPinToInterrupt(irq_pin), onALRT, FALLING);
 *
 *    // loop():
 *    if (conversionReady) {
 *        conversionReady = false;
 *        int16_t raw = Inka_AnalogReadResult();
 *    }
 * @endcode
 */
bool Inka_AnalogConfigureContinuous(analog_mode_t mode, uint8_t channel);

/**
 * @brief  Blocking read of a single channel (IRQ_NONE and IRQ_INTERNAL modes).
 *
 *  Writes the configuration, waits for the conversion to finish (by polling or
 *  pin wait, depending on irq_mode), then reads and returns REG_CONV.
 *
 * @param  mode     Signal type: ANALOG_MODE_CURRENT or ANALOG_MODE_VOLT.
 *                  Selects the PGA range (±1.024 V or ±2.048 V).
 *
 * @param  channel  Input channel index:
 *                    DEVICE_INKALOGIC_PRO → 0 to 3
 *                    DEVICE_MIKROBUS      → 0 or 1
 *
 * @return Raw signed ADC counts (int16_t) in the range 0–32767 for Inka
 *         unipolar signals. Returns INKA_AI_ERROR (-1000) on I2C error
 *         or timeout.
 *
 * @note   In IRQ_EXTERNAL mode, use Inka_AnalogStartConversion() +
 *         Inka_AnalogReadResult() instead of this function.
 */
int16_t Inka_AnalogRead(analog_mode_t mode, uint8_t channel);

/**
 * @brief  [IRQ_EXTERNAL only] Start a conversion without blocking.
 *
 *  Writes the MUX, PGA, and mode bits to REG_CONFIG, then returns immediately.
 *  The ADS1115 will assert the ALRT/RDY pin when the conversion is complete,
 *  triggering your external ISR.
 *
 *  After calling this function, do not access the I2C bus for this device
 *  until the ALRT/RDY pin asserts.
 *
 * @param  mode     Signal type: ANALOG_MODE_CURRENT or ANALOG_MODE_VOLT.
 * @param  channel  Channel index (see Inka_AnalogRead()).
 *
 * @return true   Configuration was written successfully.
 * @return false  I2C write failed.
 */
bool Inka_AnalogStartConversion(analog_mode_t mode, uint8_t channel);

/**
 * @brief  [IRQ_EXTERNAL only] Read the result of the last conversion.
 *
 *  Reads REG_CONV directly without writing a new configuration or waiting.
 *  Call this function only after your ISR has asserted that ALRT/RDY fired.
 *
 *  Typical usage inside loop():
 *  @code
 *    if (conversionReady) {          // flag set by your ISR
 *        conversionReady = false;
 *        int16_t raw = Inka_AnalogReadResult();
 *        if (raw != INKA_AI_ERROR) {
 *            float mA = Inka_AnalogReadScaled(raw, 0.0f, 24.0f, 0, 30720);
 *        }
 *        Inka_AnalogStartConversion(ANALOG_MODE_CURRENT, 0); // arm next
 *    }
 *  @endcode
 *
 * @return Raw signed ADC counts (int16_t) or INKA_AI_ERROR on I2C failure.
 */
int16_t Inka_AnalogReadResult();

/**
 * @brief  Scale a raw ADC count to an engineering-unit value using a linear map.
 *
 *  Computes the slope m = (y2 − y1) / (x2 − x1) and returns:
 *      y = m × (rawADC − x1) + y1
 *
 * @param  rawADC  Raw ADC value returned by Inka_AnalogRead() or
 *                 Inka_AnalogReadResult().
 *
 * @param  y1      Engineering-unit value at x1 (minimum of output range).
 * @param  y2      Engineering-unit value at x2 (maximum of output range).
 * @param  x1      Raw ADC count at the minimum physical signal (typically 0).
 * @param  x2      Raw ADC count at the maximum physical signal.
 *
 * @return Scaled floating-point value, or NAN if x2 == x1 (division by zero).
 *
 * @par Typical calibration constants
 *
 *  Voltage (0–10 V):
 *  @code
 *    float volts = Inka_AnalogReadScaled(raw, 0.0f, 10.0f, 0, 32000);
 *    // x2 = 32767 × (2.000 / 2.048) ≈ 32000 counts @ 10 V input
 *  @endcode
 *
 *  Current (0–24 mA):
 *  @code
 *    float mA = Inka_AnalogReadScaled(raw, 0.0f, 24.0f, 0, 30720);
 *    // x2 = 32767 × (0.960 / 1.024) ≈ 30720 counts @ 24 mA input
 *  @endcode
 */
float Inka_AnalogReadScaled(float rawADC, float y1, float y2, float x1, float x2);

// ─── Branded API for DEVICE_MIKROBUS (InkaBUS prefix) ─────────────────────────
//
// The following functions provide a branded API for InkaBUS modules with
// differential channel configuration. They automatically invoke Inka_AnalogInSetDevice(DEVICE_MIKROBUS)
// on each call to ensure the correct device layout is active, eliminating the need for
// explicit device selection in application code.
//
// Channels: 0 = (AIN0 − AIN1), 1 = (AIN2 − AIN3)

/**
 * @brief  Initialize the ADS1115 and verify I2C communication for InkaBUS.
 *
 *  Calls Inka_AnalogInputInit() with all provided parameters. This is the only
 *  InkaBUS function that does NOT automatically invoke Inka_AnalogInSetDevice(),
 *  as initialization should occur only once per device instance.
 *
 * @param  addr       I2C address of the ADS1115.
 *                    Use INKA_AI_I2C_ADDRESS_0 (ADDR→GND) or
 *                    INKA_AI_I2C_ADDRESS_1 (ADDR→VCC).
 *                    Default: INKA_AI_I2C_ADDRESS_0.
 * @param  mode       Conversion trigger mode (SINGLE or CONTINUOUS).
 *                    Default: SINGLE.
 * @param  data_rate  Samples per second (RATE_xSPS macro).
 *                    Default: RATE_128SPS.
 * @param  irq_pin    GPIO number connected to ALRT/RDY pin, or -1.
 *                    Default: -1 (no pin).
 * @param  irq_mode   Interrupt strategy (IRQ_NONE, IRQ_INTERNAL, or IRQ_EXTERNAL).
 *                    Default: IRQ_NONE.
 *
 * @return true   ADS1115 acknowledged on I2C and configuration was written.
 * @return false  I2C communication failed.
 *
 * @note  Call this once at startup after Wire.begin().
 * @note  Subsequent InkaBUS_Analog*() calls will automatically set DEVICE_MIKROBUS.
 */
bool InkaBUS_AnalogInputInit(uint8_t     addr      = INKA_AI_I2C_ADDRESS_0,
                             read_mode_t mode      = SINGLE,
                             uint8_t     data_rate = RATE_128SPS,
                             int8_t      irq_pin   = -1,
                             irq_mode_t  irq_mode  = IRQ_NONE);

/**
 * @brief  Configure the ADS1115 for continuous conversion on a fixed channel (InkaBUS).
 *
 *  Automatically sets DEVICE_MIKROBUS (2 differential channels) before configuration.
 *
 * @param  mode     Signal type: ANALOG_MODE_CURRENT or ANALOG_MODE_VOLT.
 * @param  channel  Differential channel: 0 (AIN0−AIN1) or 1 (AIN2−AIN3).
 *
 * @return true   Configuration was written and ADC is producing valid continuous samples.
 * @return false  I2C write/read failed or invalid channel for MIKROBUS.
 */
bool InkaBUS_AnalogConfigureContinuous(analog_mode_t mode, uint8_t channel);

/**
 * @brief  Blocking read of a single channel (InkaBUS).
 *
 *  Automatically sets DEVICE_MIKROBUS before reading.
 *  Suitable for IRQ_NONE and IRQ_INTERNAL modes only.
 *
 * @param  mode     Signal type: ANALOG_MODE_CURRENT or ANALOG_MODE_VOLT.
 * @param  channel  Differential channel: 0 (AIN0−AIN1) or 1 (AIN2−AIN3).
 *
 * @return Raw signed ADC counts (int16_t) in range 0–32767, or INKA_AI_ERROR on failure.
 */
int16_t InkaBUS_AnalogRead(analog_mode_t mode, uint8_t channel);

/**
 * @brief  Start a non-blocking conversion (InkaBUS, IRQ_EXTERNAL mode only).
 *
 *  Automatically sets DEVICE_MIKROBUS before starting the conversion.
 *  Returns immediately; the ISR will signal completion via ALRT/RDY.
 *
 * @param  mode     Signal type: ANALOG_MODE_CURRENT or ANALOG_MODE_VOLT.
 * @param  channel  Differential channel: 0 (AIN0−AIN1) or 1 (AIN2−AIN3).
 *
 * @return true   Configuration was written successfully.
 * @return false  I2C write failed or invalid parameters.
 */
bool InkaBUS_AnalogStartConversion(analog_mode_t mode, uint8_t channel);

/**
 * @brief  Read the result of the last conversion (InkaBUS, IRQ_EXTERNAL mode only).
 *
 *  Call this after your ISR indicates that ALRT/RDY has asserted.
 *  Does NOT automatically invoke Inka_AnalogInSetDevice() because the device
 *  state is already configured by the prior InkaBUS_AnalogStartConversion() call.
 *
 * @return Raw signed ADC counts (int16_t) or INKA_AI_ERROR on I2C failure.
 */
int16_t InkaBUS_AnalogReadResult();

/**
 * @brief  Scale a raw ADC count to engineering units (InkaBUS).
 *
 *  Linear scaling: y = m × (rawADC − x1) + y1, where m = (y2 − y1) / (x2 − x1).
 *
 * @param  rawADC  Raw ADC value from InkaBUS_AnalogRead() or InkaBUS_AnalogReadResult().
 * @param  y1      Engineering-unit value at minimum (typically 0.0).
 * @param  y2      Engineering-unit value at maximum.
 * @param  x1      Raw ADC count at minimum (typically 0).
 * @param  x2      Raw ADC count at maximum.
 *
 * @return Scaled floating-point value, or NAN if x2 == x1.
 *
 * @par Example: 0–24 mA current loop
 * @code
 *    float mA = InkaBUS_AnalogReadScaled(raw, 0.0f, 24.0f, 0, 30720);
 * @endcode
 */
float InkaBUS_AnalogReadScaled(float rawADC, float y1, float y2, float x1, float x2);

// ─── Branded API for DEVICE_INKALOGIC_PRO (InkaLogic prefix) ──────────────────
//
// The following functions provide a branded API for InkaLogic modules with
// single-ended channel configuration. They automatically invoke Inka_AnalogInSetDevice(DEVICE_INKALOGIC_PRO)
// on each call to ensure the correct device layout is active, eliminating the need for
// explicit device selection in application code.
//
// Channels: 0 = AIN0 vs GND, 1 = AIN1 vs GND, 2 = AIN2 vs GND, 3 = AIN3 vs GND

/**
 * @brief  Initialize the ADS1115 and verify I2C communication for InkaLogic.
 *
 *  Calls Inka_AnalogInputInit() with all provided parameters. This is the only
 *  InkaLogic function that does NOT automatically invoke Inka_AnalogInSetDevice(),
 *  as initialization should occur only once per device instance.
 *
 * @param  addr       I2C address of the ADS1115.
 *                    Use INKA_AI_I2C_ADDRESS_0 (ADDR→GND) or
 *                    INKA_AI_I2C_ADDRESS_1 (ADDR→VCC).
 *                    Default: INKA_AI_I2C_ADDRESS_0.
 * @param  mode       Conversion trigger mode (SINGLE or CONTINUOUS).
 *                    Default: SINGLE.
 * @param  data_rate  Samples per second (RATE_xSPS macro).
 *                    Default: RATE_128SPS.
 * @param  irq_pin    GPIO number connected to ALRT/RDY pin, or -1.
 *                    Default: -1 (no pin).
 * @param  irq_mode   Interrupt strategy (IRQ_NONE, IRQ_INTERNAL, or IRQ_EXTERNAL).
 *                    Default: IRQ_NONE.
 *
 * @return true   ADS1115 acknowledged on I2C and configuration was written.
 * @return false  I2C communication failed.
 *
 * @note  Call this once at startup after Wire.begin().
 * @note  Subsequent InkaLogic_Analog*() calls will automatically set DEVICE_INKALOGIC_PRO.
 */
bool InkaLogic_AnalogInputInit(uint8_t     addr      = INKA_AI_I2C_ADDRESS_0,
                               read_mode_t mode      = SINGLE,
                               uint8_t     data_rate = RATE_128SPS,
                               int8_t      irq_pin   = -1,
                               irq_mode_t  irq_mode  = IRQ_NONE);

/**
 * @brief  Configure the ADS1115 for continuous conversion on a fixed channel (InkaLogic).
 *
 *  Automatically sets DEVICE_INKALOGIC_PRO (4 single-ended channels) before configuration.
 *
 * @param  mode     Signal type: ANALOG_MODE_CURRENT or ANALOG_MODE_VOLT.
 * @param  channel  Single-ended channel: 0, 1, 2, or 3 (AIN0…AIN3 vs GND).
 *
 * @return true   Configuration was written and ADC is producing valid continuous samples.
 * @return false  I2C write/read failed or invalid channel for INKALOGIC_PRO.
 */
bool InkaLogic_AnalogConfigureContinuous(analog_mode_t mode, uint8_t channel);

/**
 * @brief  Blocking read of a single channel (InkaLogic).
 *
 *  Automatically sets DEVICE_INKALOGIC_PRO before reading.
 *  Suitable for IRQ_NONE and IRQ_INTERNAL modes only.
 *
 * @param  mode     Signal type: ANALOG_MODE_CURRENT or ANALOG_MODE_VOLT.
 * @param  channel  Single-ended channel: 0, 1, 2, or 3 (AIN0…AIN3 vs GND).
 *
 * @return Raw signed ADC counts (int16_t) in range 0–32767, or INKA_AI_ERROR on failure.
 */
int16_t InkaLogic_AnalogRead(analog_mode_t mode, uint8_t channel);

/**
 * @brief  Start a non-blocking conversion (InkaLogic, IRQ_EXTERNAL mode only).
 *
 *  Automatically sets DEVICE_INKALOGIC_PRO before starting the conversion.
 *  Returns immediately; the ISR will signal completion via ALRT/RDY.
 *
 * @param  mode     Signal type: ANALOG_MODE_CURRENT or ANALOG_MODE_VOLT.
 * @param  channel  Single-ended channel: 0, 1, 2, or 3 (AIN0…AIN3 vs GND).
 *
 * @return true   Configuration was written successfully.
 * @return false  I2C write failed or invalid parameters.
 */
bool InkaLogic_AnalogStartConversion(analog_mode_t mode, uint8_t channel);

/**
 * @brief  Read the result of the last conversion (InkaLogic, IRQ_EXTERNAL mode only).
 *
 *  Call this after your ISR indicates that ALRT/RDY has asserted.
 *  Does NOT automatically invoke Inka_AnalogInSetDevice() because the device
 *  state is already configured by the prior InkaLogic_AnalogStartConversion() call.
 *
 * @return Raw signed ADC counts (int16_t) or INKA_AI_ERROR on I2C failure.
 */
int16_t InkaLogic_AnalogReadResult();

/**
 * @brief  Scale a raw ADC count to engineering units (InkaLogic).
 *
 *  Linear scaling: y = m × (rawADC − x1) + y1, where m = (y2 − y1) / (x2 − x1).
 *
 * @param  rawADC  Raw ADC value from InkaLogic_AnalogRead() or InkaLogic_AnalogReadResult().
 * @param  y1      Engineering-unit value at minimum (typically 0.0).
 * @param  y2      Engineering-unit value at maximum.
 * @param  x1      Raw ADC count at minimum (typically 0).
 * @param  x2      Raw ADC count at maximum.
 *
 * @return Scaled floating-point value, or NAN if x2 == x1.
 *
 * @par Example: 0–10 V voltage signal
 * @code
 *    float volts = InkaLogic_AnalogReadScaled(raw, 0.0f, 10.0f, 0, 32000);
 * @endcode
 */
float InkaLogic_AnalogReadScaled(float rawADC, float y1, float y2, float x1, float x2);