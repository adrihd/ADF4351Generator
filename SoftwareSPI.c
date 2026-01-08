/*
 * SoftwareSPI.c
 *
 * Created: 05/01/2026 10:59:16
 * Author: Adrian Sluijters
 * Refactored by Gemini: Fixed logic using your exact defines.
 */ 

#ifndef F_CPU
#define F_CPU 11059200UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include "SoftwareSPI.h"

// --- Pin Definitions (From your uploaded file) ---
#define ADF_PIN_LE      PB0
#define ADF_PIN_DATA    PB1
#define ADF_PIN_CLK     PB2

// Mapped to internal names
#define SOFT_SPI_CS_PIN   ADF_PIN_LE
#define SOFT_SPI_MOSI_PIN ADF_PIN_DATA
#define SOFT_SPI_SCK_PIN  ADF_PIN_CLK

void soft_spi_init(void) {
    // 1. Set MOSI, SCK, CS as Outputs
    // Use |= to preserve other pin settings on PORTB (like LCD)
    DDRB |= (1 << SOFT_SPI_MOSI_PIN) | (1 << SOFT_SPI_SCK_PIN) | (1 << SOFT_SPI_CS_PIN);

    // 2. Set Initial States
    // CS (LE) starts HIGH (Inactive)
    PORTB |= (1 << SOFT_SPI_CS_PIN);
    
    // SCK starts LOW (Mode 0)
    PORTB &= ~(1 << SOFT_SPI_SCK_PIN);
}

void soft_spi_chip_enable(void) {
    // Pull CS (LE) Low to start transfer
    PORTB &= ~(1 << SOFT_SPI_CS_PIN);
}

void soft_spi_chip_disable(void) {
    // Pull CS (LE) High to latch data
    PORTB |= (1 << SOFT_SPI_CS_PIN);
}

void soft_spi_transfer(uint8_t data) {
    // Bit-bang 8 bits, MSB first
    for (int i = 0; i < 8; i++) {
        
        // 1. Set MOSI Data
        if (data & 0x80) {
            PORTB |= (1 << SOFT_SPI_MOSI_PIN);
        } else {
            PORTB &= ~(1 << SOFT_SPI_MOSI_PIN);
        }

        // 2. Pulse SCK
        _delay_us(2); 
        PORTB |= (1 << SOFT_SPI_SCK_PIN); // High
        _delay_us(2);
        PORTB &= ~(1 << SOFT_SPI_SCK_PIN); // Low

        // 3. Shift Data
        data <<= 1;
    }
}