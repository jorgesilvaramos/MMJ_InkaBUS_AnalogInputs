#include "MMJ_InkaBUS_AnalogInputs.h"

// ─── Module state ─────────────────────────────────────────────────────────────
//
// Precondition: Wire.begin() must be called by the application before
//               Inka_AnalogInputInit().

static uint8_t      current_i2c_address = INKA_AI_I2C_ADDRESS_0;
static read_mode_t  current_read_mode   = SINGLE;
static uint8_t      current_data_rate   = RATE_128SPS;
static device_t     current_device      = DEVICE_INKALOGIC_PRO;
static int8_t       current_irq_pin     = -1;
static irq_mode_t   current_irq_mode    = IRQ_NONE;

typedef struct {
    uint16_t AIN[4];
} mux_config_t;

static mux_config_t current_mux = { {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF} };

// ─── Internal helpers ─────────────────────────────────────────────────────────

static bool _write_register(uint8_t reg, uint16_t value)
{
    Wire.beginTransmission(current_i2c_address);
    Wire.write(reg);
    Wire.write((value >> 8) & 0xFF);
    Wire.write( value       & 0xFF);
    return (Wire.endTransmission() == 0);
}

static bool _read_register(uint8_t reg, uint16_t *out)
{
    Wire.beginTransmission(current_i2c_address);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) return false;
    if (Wire.requestFrom(current_i2c_address, (uint8_t)2) != 2) return false;
    if (Wire.available() < 2) return false;
    *out = ((uint16_t)Wire.read() << 8) | Wire.read();
    return true;
}

/**
 * Programs HI_THRESH = 0x8000 and LO_THRESH = 0x0000 to enable the
 * ALRT/RDY pin in conversion-ready mode (see ADS1115 datasheet §9.3.8).
 */
static bool _configure_rdy_pin()
{
    if (!_write_register(REG_HI_THRESH, 0x8000)) return false;
    if (!_write_register(REG_LO_THRESH, 0x0000)) return false;
    return true;
}

/**
 * Blocks until the ADS1115 signals conversion complete, or until
 * INKA_AI_CONV_TIMEOUT_MS elapses.
 *
 * IRQ_INTERNAL: waits for ALRT/RDY pin to assert (LOW) using digitalRead().
 * IRQ_NONE:     polls REG_CONFIG bit 15 (OS) over I2C.
 *
 * @return true if conversion completed, false on timeout or I2C error.
 */
static bool _wait_conversion()
{
    uint32_t t0 = millis();

    if (current_irq_pin >= 0 && current_irq_mode == IRQ_INTERNAL) {
        while (digitalRead(current_irq_pin) == HIGH) {
            if (millis() - t0 > INKA_AI_CONV_TIMEOUT_MS) return false;
            yield();
        }
        return true;
    }

    // ── NEW: In continuous mode, the OS bit is not enabled.
    //    Waiting for a full period ensures that REG_CONV has
    //    a fresh sample from the newly configured channel/PGA.
    if (current_read_mode == CONTINUOUS) {
        // Conversion period in ms = 1000 / SPS.
        // We derive SPS from the data-rate bits (bits [7:5] of the config word).
        static const uint16_t sps_table[] = {8, 16, 32, 64, 128, 250, 475, 860};
        uint8_t dr_index = (current_data_rate >> 5) & 0x07;
        delayMicroseconds((1000000UL / sps_table[dr_index]) + 2000);
        return true;
    }

    // Polling: read REG_CONFIG until OS bit is set (conversion done)
    uint16_t status = 0;
    do {
        if (millis() - t0 > INKA_AI_CONV_TIMEOUT_MS) return false;
        if (!_read_register(REG_CONFIG, &status)) return false;
    } while (!(status & REG_OS));

    return true;
}

/**
 * Validates the channel index against the active device layout, builds the
 * ADS1115 16-bit config word, and writes it to REG_CONFIG to start a
 * single-shot conversion.
 *
 * Always forces MODE_SINGLE_SHOT regardless of current_read_mode because:
 *   - In SINGLE mode this is the normal flow.
 *   - In CONTINUOUS mode callers are responsible for not mixing scan modes;
 *     this function is only called from paths that need a fresh conversion.
 *
 * @param  mode     Signal type (selects PGA range).
 * @param  channel  Channel index (0–3 for INKALOGIC_PRO, 0–1 for MIKROBUS).
 * @return true if the config was written successfully, false otherwise.
 */
static bool _build_and_write_config(analog_mode_t mode, uint8_t channel)
{
    uint8_t max_ch = (current_device == DEVICE_MIKROBUS) ? 1 : 3;
    if (channel > max_ch) return false;

    uint16_t mux = current_mux.AIN[channel];
    if (mux == 0xFFFF) return false;

    uint16_t config = 0;
    config |= REG_OS;
    config |= mux;
    config |= (mode == ANALOG_MODE_VOLT) ? PGA_INDUSTRIAL_VOLTAGE_AI
                                         : PGA_INDUSTRIAL_CURRENT_AI;
    config |= MODE_SINGLE_SHOT;
    config |= current_data_rate;
    config |= (current_irq_mode != IRQ_NONE) ? COMP_QUE_1CONV : COMP_QUE_DISABLE;

    return _write_register(REG_CONFIG, config);
}

// ─── Public API ───────────────────────────────────────────────────────────────

bool Inka_AnalogInputInit(uint8_t     addr,
                          read_mode_t mode,
                          uint8_t     data_rate,
                          int8_t      irq_pin,
                          irq_mode_t  irq_mode)
{
    current_i2c_address = addr;
    current_read_mode   = mode;
    current_data_rate   = data_rate;
    current_irq_pin     = irq_pin;
    current_irq_mode    = irq_mode;

    if (irq_pin >= 0) {
        pinMode(irq_pin, INPUT);
    }

    // Build and write a baseline config so the device is in a known state.
    // PGA and MUX are intentionally left at their reset defaults here;
    // they are set correctly by _build_and_write_config() before every read.
    uint16_t config = 0;
    config |= (mode == SINGLE) ? MODE_SINGLE_SHOT : MODE_CONTINUOUS;
    config |= data_rate;
    config |= (irq_mode != IRQ_NONE) ? COMP_QUE_1CONV : COMP_QUE_DISABLE;

    if (!_write_register(REG_CONFIG, config)) return false;

    if (irq_mode != IRQ_NONE && irq_pin >= 0) {
        if (!_configure_rdy_pin()) return false;
    }

    // Apply the default device layout so the MUX table is always valid.
    Inka_AnalogInSetDevice(DEVICE_INKALOGIC_PRO);

    return true;
}

void Inka_AnalogInSetDevice(device_t device)
{
    current_device = device;

    current_mux.AIN[0] = 0xFFFF;
    current_mux.AIN[1] = 0xFFFF;
    current_mux.AIN[2] = 0xFFFF;
    current_mux.AIN[3] = 0xFFFF;

    if (device == DEVICE_MIKROBUS) {
        current_mux.AIN[0] = MUX_DIFF_AN0;
        current_mux.AIN[1] = MUX_DIFF_AN1;
    } else {
        current_mux.AIN[0] = MUX_AN0;
        current_mux.AIN[1] = MUX_AN1;
        current_mux.AIN[2] = MUX_AN2;
        current_mux.AIN[3] = MUX_AN3;
    }
}

bool Inka_AnalogConfigureContinuous(analog_mode_t mode, uint8_t channel)
{
    if (current_read_mode != CONTINUOUS) return false;
    
    if (current_irq_mode == IRQ_EXTERNAL && current_irq_pin < 0) return false;

    uint8_t max_ch = (current_device == DEVICE_MIKROBUS) ? 1 : 3;
    if (channel > max_ch) return false;

    uint16_t mux = current_mux.AIN[channel];
    if (mux == 0xFFFF) return false;
    uint16_t pga = (mode == ANALOG_MODE_VOLT) ?
                    PGA_INDUSTRIAL_VOLTAGE_AI :
                    PGA_INDUSTRIAL_CURRENT_AI;

    uint16_t config = 0;
    config |= mux;
    config |= pga;
    config |= MODE_CONTINUOUS;
    config |= current_data_rate;
    config |= (current_irq_mode != IRQ_NONE) ? COMP_QUE_1CONV : COMP_QUE_DISABLE;

    if (!_write_register(REG_CONFIG, config)) return false;

    if (!_wait_conversion()) return false;

    uint16_t dummy;
    if (!_read_register(REG_CONV, &dummy)) return false;

    return true;
}

int16_t Inka_AnalogRead(analog_mode_t mode, uint8_t channel)
{
    uint8_t max_ch = (current_device == DEVICE_MIKROBUS) ? 1 : 3;
    if (channel > max_ch) return INKA_AI_ERROR;

    if (current_read_mode == CONTINUOUS) {
        // In continuous mode, reconfigure the MUX and PGA for the requested
        // channel and wait one full conversion period before reading.
        // Without this, reading a different channel returns stale data from
        // whichever channel the ADS1115 was scanning before the call.
        if (!_build_and_write_config(mode, channel)) return INKA_AI_ERROR;

        // Switch the device back to continuous mode after _build_and_write_config
        // forced single-shot, and wait for the first fresh sample.
        uint16_t config = 0;
        config |= current_mux.AIN[channel];
        config |= (mode == ANALOG_MODE_VOLT) ? PGA_INDUSTRIAL_VOLTAGE_AI
                                             : PGA_INDUSTRIAL_CURRENT_AI;
        config |= MODE_CONTINUOUS;
        config |= current_data_rate;
        config |= (current_irq_mode != IRQ_NONE) ? COMP_QUE_1CONV : COMP_QUE_DISABLE;
        if (!_write_register(REG_CONFIG, config)) return INKA_AI_ERROR;

        if (!_wait_conversion()) return INKA_AI_ERROR;

        uint16_t raw_u = 0;
        if (!_read_register(REG_CONV, &raw_u)) return INKA_AI_ERROR;
        return (int16_t)raw_u;
    }

    // Single-shot: configure, wait, read.
    if (!_build_and_write_config(mode, channel)) return INKA_AI_ERROR;
    if (!_wait_conversion())                     return INKA_AI_ERROR;

    uint16_t raw_u = 0;
    if (!_read_register(REG_CONV, &raw_u)) return INKA_AI_ERROR;
    return (int16_t)raw_u;
}

bool Inka_AnalogStartConversion(analog_mode_t mode, uint8_t channel)
{
    // This function is only valid in IRQ_EXTERNAL + SINGLE mode.
    // Calling it in CONTINUOUS mode would silently switch the device to
    // single-shot, corrupting the scan. Guard against misuse.
    if (current_irq_mode != IRQ_EXTERNAL) return false;
    if (current_read_mode != SINGLE)      return false;

    return _build_and_write_config(mode, channel);
}

int16_t Inka_AnalogReadResult()
{
    uint16_t raw_u = 0;
    if (!_read_register(REG_CONV, &raw_u)) return INKA_AI_ERROR;
    return (int16_t)raw_u;
}

float Inka_AnalogReadScaled(float rawADC, float y1, float y2, float x1, float x2)
{
    float dx = x2 - x1;

    if (fabsf(dx) < 1e-6f) {
        return NAN;
    }

    float m = (y2 - y1) / dx;

    return m * (rawADC - x1) + y1;
}

// ─── Branded API for DEVICE_MIKROBUS (InkaBUS prefix) ─────────────────────────
//
// Strategy: Each function (except Init) automatically sets DEVICE_MIKROBUS before
// delegating to the corresponding Inka_* function. This eliminates the need for
// users to call Inka_AnalogInSetDevice() in application code when using InkaBUS.
//
// Rationale:
//  • Reduces confusion: users choose one prefix (InkaBUS_* or InkaLogic_*) upfront
//  • Prevents accidental device mismatches in multi-channel or multi-device scenarios
//  • Maintains backward compatibility with the original Inka_* API
//

bool InkaBUS_AnalogInputInit(uint8_t     addr,
                             read_mode_t mode,
                             uint8_t     data_rate,
                             int8_t      irq_pin,
                             irq_mode_t  irq_mode)
{
    return Inka_AnalogInputInit(addr, mode, data_rate, irq_pin, irq_mode);
}

bool InkaBUS_AnalogConfigureContinuous(analog_mode_t mode, uint8_t channel)
{
    Inka_AnalogInSetDevice(DEVICE_MIKROBUS);
    return Inka_AnalogConfigureContinuous(mode, channel);
}

int16_t InkaBUS_AnalogRead(analog_mode_t mode, uint8_t channel)
{
    Inka_AnalogInSetDevice(DEVICE_MIKROBUS);
    return Inka_AnalogRead(mode, channel);
}

bool InkaBUS_AnalogStartConversion(analog_mode_t mode, uint8_t channel)
{
    Inka_AnalogInSetDevice(DEVICE_MIKROBUS);
    return Inka_AnalogStartConversion(mode, channel);
}

int16_t InkaBUS_AnalogReadResult()
{
    return Inka_AnalogReadResult();
}

float InkaBUS_AnalogReadScaled(float rawADC, float y1, float y2, float x1, float x2)
{
    return Inka_AnalogReadScaled(rawADC, y1, y2, x1, x2);
}

// ─── Branded API for DEVICE_INKALOGIC_PRO (InkaLogic prefix) ──────────────────
//
// Strategy: Each function (except Init) automatically sets DEVICE_INKALOGIC_PRO
// before delegating to the corresponding Inka_* function. See InkaBUS rationale above.
//

bool InkaLogic_AnalogInputInit(uint8_t     addr,
                               read_mode_t mode,
                               uint8_t     data_rate,
                               int8_t      irq_pin,
                               irq_mode_t  irq_mode)
{
    return Inka_AnalogInputInit(addr, mode, data_rate, irq_pin, irq_mode);
}

bool InkaLogic_AnalogConfigureContinuous(analog_mode_t mode, uint8_t channel)
{
    Inka_AnalogInSetDevice(DEVICE_INKALOGIC_PRO);
    return Inka_AnalogConfigureContinuous(mode, channel);
}

int16_t InkaLogic_AnalogRead(analog_mode_t mode, uint8_t channel)
{
    Inka_AnalogInSetDevice(DEVICE_INKALOGIC_PRO);
    return Inka_AnalogRead(mode, channel);
}

bool InkaLogic_AnalogStartConversion(analog_mode_t mode, uint8_t channel)
{
    Inka_AnalogInSetDevice(DEVICE_INKALOGIC_PRO);
    return Inka_AnalogStartConversion(mode, channel);
}

int16_t InkaLogic_AnalogReadResult()
{
    return Inka_AnalogReadResult();
}

float InkaLogic_AnalogReadScaled(float rawADC, float y1, float y2, float x1, float x2)
{
    return Inka_AnalogReadScaled(rawADC, y1, y2, x1, x2);
}