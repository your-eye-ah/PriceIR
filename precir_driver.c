#include "precir_driver.h"
#include <furi.h>
#include <furi_hal.h>
#include <stdlib.h>

// Standard IR TX Pin: GPIOB, Pin 9 (0x0200)
// Using Direct Register Access for 1.26MHz
#define PRECIR_PIN_MASK 0x0200
#define PRECIR_GPIO_PORT GPIOB

// Timings based on Furrtek.org documentation
#define PP4_PULSE_LEN 40
#define PP16_PULSE_LEN 21

// Lookup Tables for Total Symbol Duration (us)
// Index = binary value of symbols (00, 01, 10, 11)
static const uint16_t PP4_DURATIONS[] = {
    61,  // 00 (2t)
    122, // 01 (4t)
    244, // 10 (8t) - Note: Gray code, 10 is longest
    183  // 11 (6t)
};

// Index = nibble value 0-15
static const uint16_t PP16_DURATIONS[] = {
    27, 51, 35, 43,    // 0-3
    147, 123, 139, 131, // 4-7
    83, 59, 75, 67,    // 8-11
    91, 115, 99, 107   // 12-15
};

// extern const GpioPin gpio_infrared_tx; // Already defined in furi_hal_resources.h

void precir_driver_init() {
    FURI_LOG_I("PrecIR", "Init Driver (Furrtek Spec)");
}

void precir_driver_deinit() {
    FURI_LOG_I("PrecIR", "Deinit Driver (High Speed GPIO)");
}

// 1.26MHz Burst Helper
// Freq = 1.263 MHz -> Period = 792ns -> Half Period = 396ns.
// System Clock = 64 MHz -> 15.625ns per cycle.
// Half Period = ~25.3 cycles.
// BSRR Write ~ 2 cycles.
// Loop overhead ~ 4 cycles.
// Need ~19 cycles delay.
// __NOP() is 1 cycle.
#define DELAY_CYCLES 15  // Tuned conservatively (overhead might be higher)

static void precir_carrier_burst(uint32_t duration_us) {
    if (duration_us == 0) return;
    
    // Calculate iterations: Total US / Period US (0.792)
    // Approx: duration * 1.26
    uint32_t count = (duration_us * 126) / 100;
    
    FURI_CRITICAL_ENTER();
    for(uint32_t i = 0; i < count; i++) {
        // HIGH
        PRECIR_GPIO_PORT->BSRR = PRECIR_PIN_MASK;
        // Delay 22 cycles (Tuned for 1.26MHz on 64MHz clock)
        // Overhead ~7 cycles. Total period ~51 cycles.
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        
        // LOW
        PRECIR_GPIO_PORT->BSRR = (PRECIR_PIN_MASK << 16);
        // Delay 22 cycles
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
    }
    FURI_CRITICAL_EXIT();
}

void precir_driver_transmit(const uint8_t* frame, size_t len, bool pp16) {
    // Alloc buffer
    size_t edge_count = len * 8; 
    uint32_t* timings = malloc(sizeof(uint32_t) * edge_count);
    if (!timings) return;
    
    size_t idx = 0;
    for(size_t i = 0; i < len; i++) {
        uint8_t byte = frame[i];
        if(pp16) {
             // LSB Nibble First (Assumption based on PP4 logic)
             // Low Nibble
             uint8_t val = byte & 0x0F;
             timings[idx++] = PP16_PULSE_LEN;
             timings[idx++] = PP16_DURATIONS[val] - PP16_PULSE_LEN;
             
             // High Nibble
             val = (byte >> 4) & 0x0F;
             timings[idx++] = PP16_PULSE_LEN;
             timings[idx++] = PP16_DURATIONS[val] - PP16_PULSE_LEN;

        } else {
            // LSB Pair First: bits 0-1, then 2-3, etc.
            for(int shift = 0; shift <= 6; shift += 2) {
                uint8_t val = (byte >> shift) & 0x03;
                timings[idx++] = PP4_PULSE_LEN;
                timings[idx++] = PP4_DURATIONS[val] - PP4_PULSE_LEN;
            }
        }
    }
    
    FURI_LOG_I("PrecIR", "HS TX Start. Edges: %zu. Len: %zu", edge_count, len);

    FuriString* str = furi_string_alloc();
    for(size_t i=0; i<len; i++) {
        furi_string_cat_printf(str, "%02X ", frame[i]);
    }
    FURI_LOG_I("PrecIR", "Frame: %s", furi_string_get_cstr(str));
    furi_string_free(str);
    
    // Init Pin direct
    furi_hal_gpio_init(&gpio_infrared_tx, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    
    bool is_pulse = true;
    for(size_t i=0; i < idx; i++) {
        uint32_t duration = timings[i];
        
        if(is_pulse) {
            precir_carrier_burst(duration);
        } else {
            furi_delay_us(duration);
        }
        
        is_pulse = !is_pulse;
    }
    
    // Ensure Low
    PRECIR_GPIO_PORT->BSRR = (PRECIR_PIN_MASK << 16);
    furi_hal_gpio_init(&gpio_infrared_tx, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    
    FURI_LOG_I("PrecIR", "HS TX Done");
    free(timings);
}
