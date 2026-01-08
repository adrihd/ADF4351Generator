/**
 * @file     adf4351.c
 * @brief    ADF4351 driver (Fixed: Safe Math & Golden Values)
 * @version  V2.40
 * @date     07 January 2026
 */

#include "adf4351.h"
#include "SoftwareSPI.h"
#include <math.h>

#define R0_TEST 0x00418008	// 0000 0000 0100 0001 1000 0000 0000 1000	 ALL MSB FIRST TX
#define R1_TEST 0x08008029	// 0000 1000 0000 0000 1000 0000 0010 1001
#define R2_TEST 0x00004E42	// 0000 0000 0000 0000 0100 1110 0100 0010		
#define R3_TEST 0x000004B3	// 0000 0000 0000 0000 0000 0100 1011 0011
#define R4_TEST 0x00BC803C	// 0000 0000 1011 1100 1000 0000 0011 1100
#define R5_TEST 0x00580005	// 0000 0000 0101 1000 0000 0000 0000 0101


// Global Shadow Registers
ADF4351_Reg0_t ADF4351_Reg0;
ADF4351_Reg1_t ADF4351_Reg1;
ADF4351_Reg2_t ADF4351_Reg2;
ADF4351_Reg3_t ADF4351_Reg3;
ADF4351_Reg4_t ADF4351_Reg4;
ADF4351_Reg5_t ADF4351_Reg5;

// Private Helper: Select Output Divider
static ADF4351_RFDIV_t ADF4351_Select_Output_Divider(double RFoutFrequency)
{
    if (RFoutFrequency >= 2200000000.0) return ADF4351_RFDIV_1;
    if (RFoutFrequency >= 1100000000.0)  return ADF4351_RFDIV_2;
    if (RFoutFrequency >= 550000000.0)   return ADF4351_RFDIV_4; 
    if (RFoutFrequency >= 275000000.0)   return ADF4351_RFDIV_8;
    if (RFoutFrequency >=  137500000.0)   return ADF4351_RFDIV_16;
    if (RFoutFrequency >=  68750000.0)   return ADF4351_RFDIV_32;
    return ADF4351_RFDIV_64;
}

// Private Helper: GCD
static int gcd_iter(uint32_t u, uint32_t v) {
    uint32_t t;
    while (v) {
        t = u; u = v; v = t % v;
    }
    return u;
}

// Private Helper: Write 32-bit word
static void ADF4351_WriteRegister32(uint32_t value) {
    soft_spi_chip_enable();
    soft_spi_transfer((uint8_t)((value >> 24) & 0xFF));
    soft_spi_transfer((uint8_t)((value >> 16) & 0xFF));
    soft_spi_transfer((uint8_t)((value >> 8)  & 0xFF));
    soft_spi_transfer((uint8_t)((value)       & 0xFF));
    soft_spi_chip_disable();
}

/** \brief Initialize Defaults with User "Golden" Values */
void ADF4351_Init(void)
{
    ADF4351_Reg0.w = R0_TEST;
    ADF4351_Reg1.w = R1_TEST;
    ADF4351_Reg2.w = R2_TEST;
    ADF4351_Reg3.w = R3_TEST;
    ADF4351_Reg4.w = R4_TEST;
    ADF4351_Reg5.w = R5_TEST;
}

/** \brief Main Calculation Logic */
ADF4351_ERR_t ADF4351_UpdateFrequencyRegisters(double RFout, double REFin, double OutputChannelSpacing, int gcd, int AutoBandSelectClock, double *RFoutCalc )
{
    ADF4351_RFDIV_t RfDivEnum;
    uint16_t        OutputDivider;
    double          PFDFreq;                            
    uint16_t        Rcounter;                           
    int             RefDoubler;                     
    int             RefD2;                          
    double          N;
    uint16_t        INT, MOD, FRAC;
    uint32_t        D;

    // --- FORCE CONSTANTS FROM GOLDEN VALUES ---
    ADF4351_Reg1.b.Prescaler = 1; // 8/9
    ADF4351_Reg1.b.PhaseVal  = 1; 
    ADF4351_Reg4.b.Feedback  = 1; 

    // 1. Get Ref Setup
    RefD2 = ADF4351_Reg2.b.RDiv2 + 1;                   
    RefDoubler = ADF4351_Reg2.b.RMul2 + 1;              
    Rcounter = ADF4351_Reg2.b.RCountVal;
    
    // PFD Calculation
    PFDFreq = (REFin * RefDoubler / RefD2) / Rcounter;

    // 2. Select Output Divider
    RfDivEnum = ADF4351_Select_Output_Divider(RFout);
    ADF4351_Reg4.b.RfDivSel = RfDivEnum; 
    OutputDivider = (1U << RfDivEnum);

    // 3. Calculate N
    N = ((RFout * OutputDivider) / PFDFreq);

    // 4. Calculate INT, MOD, FRAC
    INT = (uint16_t)N;
    MOD = (uint16_t)(round((PFDFreq / OutputChannelSpacing)));
    FRAC = (uint16_t)(round(((double)N - INT) * MOD));

    // 5. GCD Optimization
    if (gcd) {
        D = gcd_iter((uint32_t)MOD, (uint32_t)FRAC);
        MOD = MOD / D;
        FRAC = FRAC / D;
    }
    if (MOD == 1) MOD = 2; 

    // SAFETY CHECK: Ensure MOD doesn't overflow 12 bits (Max 4095)
    if (MOD > 4095) return ADF4351_Err_InvalidMOD;

    // Save Calculated Values
    ADF4351_Reg0.b.FracVal = (FRAC & 0x0FFF);
    ADF4351_Reg0.b.IntVal = (INT & 0xFFFF);
    ADF4351_Reg1.b.ModVal = (MOD & 0x0FFF);

    *RFoutCalc = (((double)((double)INT + ((double)FRAC / (double)MOD)) * (double)PFDFreq / OutputDivider));

    return ADF4351_Err_None;
}

void ADF4351_UpdateAllRegisters(void) {
    ADF4351_WriteRegister32(ADF4351_Reg5.w);
    ADF4351_WriteRegister32(ADF4351_Reg4.w);
    ADF4351_WriteRegister32(ADF4351_Reg3.w);
    ADF4351_WriteRegister32(ADF4351_Reg2.w);
    ADF4351_WriteRegister32(ADF4351_Reg1.w);
    ADF4351_WriteRegister32(ADF4351_Reg0.w);
}