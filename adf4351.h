/**
 * @file     adf4351.h
 * @brief    ADF4351 driver Header File (Refactored for AVR/ATmega8A)
 * @version  V2.00
 * @date     05 January 2026
 */

#ifndef _ADF4351_H_
#define _ADF4351_H_

#include <stdint.h>
#include <stdbool.h>

// --- Constants from Datasheet ---
#define ADF5451_PFD_MAX         32.0e6
#define ADF4351_RFOUT_MAX       4400.0e6
#define ADF4351_RFOUTMIN        35.000e6
#define ADF4351_REFINMAX        250.0e6

/** \brief  Union type for Register 0 */
typedef union {
    struct {
        uint32_t ControlBits    :3;//Bit 0
        uint32_t FracVal        :12;
        uint32_t IntVal         :16;
        uint32_t _reserved_0    :1;
    } b;
    uint32_t w;
} ADF4351_Reg0_t;

/** \brief  Union type for Register 1 */
typedef union {
    struct {
        uint32_t ControlBits    :3;//Bit 0
        uint32_t ModVal         :12;
        uint32_t PhaseVal       :12;
        uint32_t Prescaler      :1;
        uint32_t PhaseAdjust    :1;
        uint32_t _reserved_0    :3;
    } b;
    uint32_t w;
} ADF4351_Reg1_t;

/** \brief  Union type for Register 2 */
typedef union {
    struct {
        uint32_t ControlBits    :3; //Bit 0
        uint32_t CounterReset   :1;
        uint32_t CPTristate     :1;
        uint32_t PowerDown      :1;
        uint32_t PhasePolarity  :1;
        uint32_t LDP            :1;
        uint32_t LDF            :1;
        uint32_t CPCurrent      :4;
        uint32_t DoubleBuffer   :1;
        uint32_t RCountVal      :10;
        uint32_t RDiv2          :1;
        uint32_t RMul2          :1;
        uint32_t MuxOut         :3;
        uint32_t LowNoiseSpur   :2;
        uint32_t _reserved_0    :1;
    } b;
    uint32_t w;
} ADF4351_Reg2_t;

/** \brief  Union type for Register 3 */
typedef union {
    struct {
        uint32_t ControlBits    :3;//Bit 0
        uint32_t ClkDivVal      :12;
        uint32_t ClkDivMod      :2;
        uint32_t _reserved_0    :1;
        uint32_t CsrEn          :1;
        uint32_t _reserved_1    :2;
        uint32_t ChargeCh       :1;
        uint32_t AntibacklashW  :1;
        uint32_t BandSelMode    :1;
        uint32_t _reserved_2    :8;
    } b;
    uint32_t w;
} ADF4351_Reg3_t;

/** \brief  Union type for Register 4 */
typedef union {
    struct {
        uint32_t ControlBits    :3;//Bit 0
        uint32_t OutPower       :2;
        uint32_t OutEnable      :1;
        uint32_t AuxPower       :2;
        uint32_t AuxEnable      :1;
        uint32_t AuxSel         :1;
        uint32_t Mtld           :1;
        uint32_t VcoPowerDown   :1;
        uint32_t BandClkDiv     :8;
        uint32_t RfDivSel       :3;
        uint32_t Feedback       :1;
        uint32_t _reserved_0    :8;
    } b;
    uint32_t w;
} ADF4351_Reg4_t;

/** \brief  Union type for Register 5 */
typedef union {
    struct {
        uint32_t ControlBits    :3;
        uint32_t _reserved_0    :16;
        uint32_t _reserved_1    :2;
        uint32_t _reserved_2    :1;
        uint32_t LdPinMode      :2;
        uint32_t _reserved_3    :8;
    } b;
    uint32_t w;
} ADF4351_Reg5_t;

/** \brief Output Divider Enum */
typedef enum {
    ADF4351_RFDIV_1  = 0,
    ADF4351_RFDIV_2  = 1,
    ADF4351_RFDIV_4  = 2,
    ADF4351_RFDIV_8  = 3,
    ADF4351_RFDIV_16 = 4,
    ADF4351_RFDIV_32 = 5,
    ADF4351_RFDIV_64 = 6
} ADF4351_RFDIV_t;

/** \brief Error Codes */
typedef enum {
    ADF4351_Err_None,
    ADF4351_Err_PFD,
    ADF4351_Err_BandSelFreqTooHigh,
    ADF4351_Err_RFoutTooHigh,
    ADF4351_Err_RFoutTooLow,
    ADF4351_Err_REFinTooHigh,
    ADF4351_Err_InvalidN,
    ADF4351_Err_InvalidMOD,
    ADF4351_Warn_NotTuned
} ADF4351_ERR_t;

// --- External Access to Shadows ---
extern ADF4351_Reg0_t ADF4351_Reg0;
extern ADF4351_Reg1_t ADF4351_Reg1;
extern ADF4351_Reg2_t ADF4351_Reg2;
extern ADF4351_Reg3_t ADF4351_Reg3;
extern ADF4351_Reg4_t ADF4351_Reg4;
extern ADF4351_Reg5_t ADF4351_Reg5;

// --- API Functions ---
void ADF4351_Init(void);
ADF4351_ERR_t ADF4351_UpdateFrequencyRegisters(double RFout, double REFin, double OutputChannelSpacing, int gcd, int AutoBandSelectClock, double *RFoutCalc);
void ADF4351_UpdateAllRegisters(void);

#endif /* _ADF4351_H_ */