# Introduciton

This firmware is for the ATMEGA8A Signal Generators which you can find on Aliexpress which use the ADF3541 chips for their 35-4400Mhz RF Output.

As the initial boards are built incorrect, software SPI is needed to configure the ADF chip.


# Software Used
- Microchip Studio using the C generated ATMEGA8A project.

# TODO:
- Update code comments
- Clean up the code
- Fix the README
- Fix the LICENSES
- XLate to CPP

# Sources
For brevity, I'm adding my reference sources

For ADF4351 code:
- https://github.com/s54mtb/ADF4351/

For Software SPI code:
- https://github.com/Alex2269/stm32_soft_spi/tree/master/stm32_software_spi
- Various others....