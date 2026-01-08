/*
 * ==========================================================================
 * Project: ADF4351 RF Signal Generator (Fixed: 10kHz Math Resolution)
 * Target:  ATmega8A
 * ==========================================================================
 */

#define F_CPU 11059200UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdlib.h> 
#include <string.h>
#include "SoftwareSPI.h" 
#include "adf4351.h" 

// --- LCD Control (Port C) ---
#define LCD_CTRL_PORT   PORTC
#define LCD_CTRL_DDR    DDRC
#define LCD_RS          PC3
#define LCD_RW          PC4
#define LCD_EN          PC5

// --- LCD Data (Port D) ---
#define LCD_DATA_PORT   PORTD
#define LCD_DATA_DDR    DDRD

// --- Rotary Encoder (Port C) ---
#define ROT_PIN         PINC
#define ROT_PORT        PORTC
#define ROT_DDR         DDRC
#define ROT_A           PC0
#define ROT_B           PC1

#define ADC_KEYPAD_CH   7
#define LED_RUN_PIN     PC2

#define MIN_FREQ_KHZ    35000UL
#define MAX_FREQ_KHZ    4400000UL

// State Defaults
volatile uint32_t g_current_freq_khz = 410000UL;
volatile bool     g_rf_output_on = true; // Starts ON matching Golden Config
volatile bool     g_scan_mode = false;
volatile int8_t   g_scan_dir = 0; 

const uint32_t STEP_SIZES[4] = {100, 1000, 10000, 100000};
volatile uint8_t g_step_index = 1; 

volatile int8_t   g_rotary_delta = 0;
volatile uint8_t  g_key_pressed = 0xFF; 
volatile bool     g_action_fire = false; 

char    g_input_buf[12];
uint8_t g_input_pos = 0;
bool    g_editing = false;

// --- LCD Functions ---
void LCD_Pulse() {
    LCD_CTRL_PORT |= (1 << LCD_EN);
    _delay_us(2);
    LCD_CTRL_PORT &= ~(1 << LCD_EN);
    _delay_us(50);
}

void LCD_WriteNibble(uint8_t nibble) {
    uint8_t current = LCD_DATA_PORT & 0x0F;
    LCD_DATA_PORT = current | (nibble << 4);
    LCD_Pulse();
}

void LCD_Cmd(uint8_t cmd) {
    LCD_CTRL_PORT &= ~((1 << LCD_RS) | (1 << LCD_RW)); 
    LCD_WriteNibble(cmd >> 4);
    LCD_WriteNibble(cmd & 0x0F);
    _delay_ms(2); 
}

void LCD_Char(char data) {
    LCD_CTRL_PORT |= (1 << LCD_RS);
    LCD_CTRL_PORT &= ~(1 << LCD_RW); 
    LCD_WriteNibble(data >> 4);
    LCD_WriteNibble(data & 0x0F);
    _delay_us(50);
}

void LCD_String(const char *str) {
    while (*str) LCD_Char(*str++);
}

void LCD_PrintDec(uint32_t n) {
    if (n == 0) { LCD_Char('0'); return; }
    char buf[11];
    uint8_t i = 0;
    while (n > 0) { buf[i++] = (n % 10) + '0'; n /= 10; }
    while (i > 0) LCD_Char(buf[--i]);
}

void LCD_PrintDec3(uint32_t n) {
    if (n < 100) LCD_Char('0');
    if (n < 10)  LCD_Char('0');
    LCD_PrintDec(n);
}

void LCD_Init() {
    LCD_CTRL_DDR |= (1 << LCD_RS) | (1 << LCD_EN) | (1 << LCD_RW);
    LCD_DATA_DDR |= 0xF0; 
    LCD_CTRL_PORT &= ~((1 << LCD_RS) | (1 << LCD_EN) | (1 << LCD_RW));
    _delay_ms(50); 
    LCD_WriteNibble(0x03); _delay_ms(5);
    LCD_WriteNibble(0x03); _delay_us(150);
    LCD_WriteNibble(0x03);
    LCD_WriteNibble(0x02); 
    LCD_Cmd(0x28); LCD_Cmd(0x0C); LCD_Cmd(0x01); 
}

// --- Wrapper ---
void SetRF_Frequency(uint32_t freq_khz) {
    if (freq_khz < MIN_FREQ_KHZ) freq_khz = MIN_FREQ_KHZ;
    if (freq_khz > MAX_FREQ_KHZ) freq_khz = MAX_FREQ_KHZ;

    // Sync Enable Bit
    ADF4351_Reg4.b.OutEnable = g_rf_output_on ? 1 : 0;

    double calc_freq;
    
    // FIX: Using 10000.0 (10 kHz) spacing instead of 1000.0 (1 kHz).
    ADF4351_UpdateFrequencyRegisters(
        (double)freq_khz * 1000.0, 
        25000000.0,                
        100000.0,                    
        0,                         
        0,                         
        &calc_freq                 
    );
    ADF4351_UpdateAllRegisters();
}

// --- Inputs ---
uint8_t Decode_ADC(uint16_t adc) {
    if (adc > 1000) return 0xFF;
    if (adc < 130)  return '9';
    if (adc < 200)  return '8';
    if (adc < 265)  return '7';
    if (adc < 315)  return '6';
    if (adc < 360)  return '5';
    if (adc < 400)  return '4';
    if (adc < 450)  return '3';
    if (adc < 510)  return '2';
    if (adc < 555)  return '1';
    if (adc < 595)  return '0';
    if (adc < 628)  return 'd'; 
    if (adc < 656)  return 'u'; 
    if (adc < 687)  return 'k'; 
    if (adc < 720)  return 'c'; 
    if (adc < 850)  return 's'; 
    return 0xFF;
}

// --- Interrupts ---

// Timer0 Overflow: Handles Rotary Encoder & LED Heartbeat
ISR(TIMER0_OVF_vect) {
    static uint8_t rot_prev = 0;
    static uint8_t hb_cnt = 0;
    
    hb_cnt++;
    if (hb_cnt == 0) LCD_CTRL_PORT ^= (1 << LED_RUN_PIN);

    uint8_t rot_curr = ROT_PIN & 0x03; 
    if (rot_curr != rot_prev) {
        if ((rot_prev == 0 && rot_curr == 1) || (rot_prev == 1 && rot_curr == 3) || 
            (rot_prev == 3 && rot_curr == 2) || (rot_prev == 2 && rot_curr == 0)) 
            g_rotary_delta--;
        else if ((rot_prev == 0 && rot_curr == 2) || (rot_prev == 2 && rot_curr == 3) || 
                 (rot_prev == 3 && rot_curr == 1) || (rot_prev == 1 && rot_curr == 0)) 
            g_rotary_delta++;
        rot_prev = rot_curr;
    }
}

// ADC Complete: Handles Keypad Scanning (Non-blocking)
ISR(ADC_vect) {
    // 1. Read the result
    uint16_t val = ADCW;
    
    // 2. Process Logic
    uint8_t key = Decode_ADC(val);
    static uint8_t last_key = 0xFF;
    static uint16_t hold_time = 0;

    if (key != 0xFF && key == last_key) {
        hold_time++;
        if (hold_time == 300) { 
            g_key_pressed = key; 
            if (key == 'k') g_action_fire = true; 
        }
        if (hold_time > 3000) {
            if (key == 'u') { g_scan_mode = true; g_scan_dir = 1; }
            if (key == 'd') { g_scan_mode = true; g_scan_dir = -1; }
        }
    } else {
        hold_time = 0;
        last_key = key;
    }

    // 3. Start the next conversion immediately (Tail Chaining)
    ADCSRA |= (1 << ADSC);
}

// --- Main ---
void Update_Screen() {
    LCD_Cmd(0x80); 
    if (g_editing) {
        LCD_String("Set:"); LCD_String(g_input_buf); LCD_String(" MHz  "); 
    } else {
        uint32_t mhz = g_current_freq_khz / 1000;
        uint32_t dec = g_current_freq_khz % 1000;
        LCD_PrintDec(mhz); LCD_Char('.'); LCD_PrintDec3(dec); LCD_String(" MHz ");
    }
    LCD_Cmd(0xC0); 
    const char *sl[] = {"0.1M", " 1M ", " 10M", "100M"};
    LCD_String(sl[g_step_index]);
    if (g_rf_output_on) LCD_String("  >> ON ");
    else                LCD_String("     OFF");
}

uint32_t Parse_Input_Buffer() {
    uint32_t val = 0;
    for(uint8_t i = 0; i < g_input_pos; i++) {
        if(g_input_buf[i] >= '0' && g_input_buf[i] <= '9') 
            val = (val * 10) + (g_input_buf[i] - '0');
    }
    return val;
}

int main(void) {
    LCD_Init();
    soft_spi_init();
    ADF4351_Init(); // Loads Golden Hex
	
    ROT_DDR &= ~((1<<ROT_A)|(1<<ROT_B)); 
    ROT_PORT |= (1<<ROT_A)|(1<<ROT_B);   
    LCD_CTRL_DDR |= (1 << LED_RUN_PIN);  

    // Setup ADC
    ADMUX = (1 << REFS0) | ADC_KEYPAD_CH;
    // Added (1 << ADIE) to enable Interrupts
    ADCSRA = (1 << ADEN) | (1 << ADIE) | (1 << ADPS2) | (1 << ADPS1); 

    // Setup Timer0
    TCCR0 = (1 << CS01) | (1 << CS00); 
    TIMSK |= (1 << TOIE0);
    
    sei(); // Enable Global Interrupts

    // Start first ADC conversion manually to kick off the chain
    ADCSRA |= (1 << ADSC);

    LCD_String("RF Generator");
    LCD_Cmd(0xC0); LCD_String("35M - 4000M");
    _delay_ms(1000); LCD_Cmd(0x01);
    
    // Force initial load of correct values
    Update_Screen();

    while (1) {
        // Poll_Inputs() removed. ISR(ADC_vect) now handles this.

        if (g_rotary_delta >= 4 || g_rotary_delta <= -4) {
            uint32_t step = STEP_SIZES[g_step_index];
            int8_t clicks = 0;
            cli();
            clicks = g_rotary_delta / 4; 
            g_rotary_delta %= 4; 
            sei();

            if (clicks != 0) {
                int32_t change = (int32_t)clicks * (int32_t)step;
                
                if (clicks > 0) {
                    if (MAX_FREQ_KHZ - g_current_freq_khz < change) g_current_freq_khz = MAX_FREQ_KHZ;
                    else g_current_freq_khz += change;
                } else {
                    uint32_t abs_change = -change;
                    if (g_current_freq_khz < MIN_FREQ_KHZ + abs_change) g_current_freq_khz = MIN_FREQ_KHZ;
                    else g_current_freq_khz -= abs_change;
                }

                if (g_rf_output_on) SetRF_Frequency(g_current_freq_khz);
			    ADF4351_UpdateAllRegisters();

                Update_Screen();
            }
        }

        if (g_scan_mode) {
            uint32_t step = STEP_SIZES[g_step_index];
            if (g_scan_dir > 0) g_current_freq_khz += step;
            else                g_current_freq_khz -= step;
            if (g_current_freq_khz > MAX_FREQ_KHZ) g_current_freq_khz = MAX_FREQ_KHZ;
            if (g_current_freq_khz < MIN_FREQ_KHZ) g_current_freq_khz = MIN_FREQ_KHZ;

            if (!g_rf_output_on) g_rf_output_on = true;
            SetRF_Frequency(g_current_freq_khz);
            Update_Screen();
            _delay_ms(80);
        }

        if (g_key_pressed != 0xFF) {
            uint8_t key = g_key_pressed;
            g_key_pressed = 0xFF;

            if (g_scan_mode) {
                g_scan_mode = false;
                if (key == 'c') {
                    g_rf_output_on = false;
                    SetRF_Frequency(g_current_freq_khz);
                    Update_Screen();
                }
                continue;
            }

            if (key >= '0' && key <= '9') {
                if (!g_editing) {
                    g_editing = true; g_input_pos = 0;
                    memset(g_input_buf, 0, 12);
                }
                if (g_input_pos < 10) g_input_buf[g_input_pos++] = key;
                Update_Screen();
            }
            else if (key == 'k') { 
                if (g_editing) {
                    uint32_t val = Parse_Input_Buffer();
                    g_current_freq_khz = val * 1000;
                    g_editing = false;
                } else {
                    g_rf_output_on = !g_rf_output_on;
                }
                SetRF_Frequency(g_current_freq_khz);
                Update_Screen();
            }
            else if (key == 's') {
                g_step_index = (g_step_index + 1) % 4;
                Update_Screen();
            }
            else if (key == 'c') {
                g_rf_output_on = false;
                g_editing = false;
                SetRF_Frequency(g_current_freq_khz);
                Update_Screen();
            }
            else if (key == 'u' || key == 'd') {
                uint32_t step = STEP_SIZES[g_step_index];
                if (key == 'u') g_current_freq_khz += step;
                else            g_current_freq_khz -= step;
                if (g_rf_output_on) SetRF_Frequency(g_current_freq_khz);
                Update_Screen();
            }
        }
        _delay_us(100);
    }
}