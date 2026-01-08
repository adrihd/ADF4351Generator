/*
 * SoftwareSPI.h
 *
 * Created: 05/01/2026 10:59:16
 * Author: Adrian Sluijters
 * Refactored by Gemini to match your specific function names
 */ 

#ifndef SOFTWARESPI_H_
#define SOFTWARESPI_H_

#include <avr/io.h>
#include <stdint.h>

void soft_spi_init(void);
void soft_spi_transfer(uint8_t data);
void soft_spi_chip_enable(void);
void soft_spi_chip_disable(void);

#endif /* SOFTWARESPI_H_ */