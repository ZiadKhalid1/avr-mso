#include "../../LIB/STD_TYPES.h"
#include "../../LIB/BIT_MATH.h"
#include "AC_Interface.h"
#include "AC_Private.h"

/* Static callback pointer stored in SRAM */
static void (*AC_Callback)(void) = Null;

void AC_Init(const AC_Config_t *config)
{
    if (config == Null)
    {
        return;
    }

    /* 1. Power down comparator temporarily while reconfiguring to avoid false trigger spikes */
    SET_BIT(ACSR_Reg, ACD_Bit);

    /* Disable comparator interrupt during setup */
    CLR_BIT(ACSR_Reg, ACIE_Bit);

    /* 2. Configure Positive Input (AIN0 Pin vs Internal 1.1V Bandgap) */
    if (config->positive_source == AC_POS_INPUT_BANDGAP)
    {
        SET_BIT(ACSR_Reg, ACBG_Bit);
    }
    else
    {
        CLR_BIT(ACSR_Reg, ACBG_Bit);
        /* Disable digital input buffer on PD6 (AIN0) to reduce analog pin noise */
        SET_BIT(DIDR1_Reg, AIN0D_Bit);
    }

    /* 3. Configure Negative Input (AIN1 Pin vs ADC Channel via Multiplexer) */
    if (config->negative_source == AC_NEG_INPUT_AIN1)
    {
        /* Connect negative terminal to physical AIN1 (PD7) */
        CLR_BIT(ADCSRB_Reg, ACME_Bit);
        /* Disable digital input buffer on PD7 (AIN1) */
        SET_BIT(DIDR1_Reg, AIN1D_Bit);
    }
    else
    {
        /* To route ADC MUX to Comparator:
         * 1. The ADC must be disabled (ADEN = 0)
         * 2. ACME in ADCSRB must be written to 1
         * 3. Lower 3 bits of ADMUX (MUX2:0) select the ADC pin (ADC0 - ADC7)
         */
        CLR_BIT(ADCSRA_Reg, ADEN_Bit);
        SET_BIT(ADCSRB_Reg, ACME_Bit);

        /* Clear existing MUX bits and insert target ADC channel (0-7) */
        ADMUX_Reg = (ADMUX_Reg & 0xF8) | ((u8)(config->negative_source - 1) & 0x07);
    }

    /* 4. Configure Interrupt Trigger Edge Mode */
    switch (config->interrupt_mode)
    {
        case AC_INT_TOGGLE:
            CLR_BIT(ACSR_Reg, ACIS1_Bit);
            CLR_BIT(ACSR_Reg, ACIS0_Bit);
            break;

        case AC_INT_FALLING_EDGE:
            SET_BIT(ACSR_Reg, ACIS1_Bit);
            CLR_BIT(ACSR_Reg, ACIS0_Bit);
            break;

        case AC_INT_RISING_EDGE:
            SET_BIT(ACSR_Reg, ACIS1_Bit);
            SET_BIT(ACSR_Reg, ACIS0_Bit);
            break;

        default:
            /* Fallback to toggle */
            CLR_BIT(ACSR_Reg, ACIS1_Bit);
            CLR_BIT(ACSR_Reg, ACIS0_Bit);
            break;
    }

    /* 5. Clear pending interrupt flag (writing 1 clears ACI in AVR hardware) */
    SET_BIT(ACSR_Reg, ACI_Bit);

    /* 6. Re-enable the Analog Comparator */
    CLR_BIT(ACSR_Reg, ACD_Bit);
}


void AC_Enable(void)
{
    CLR_BIT(ACSR_Reg, ACD_Bit);
}

void AC_Disable(void)
{
    SET_BIT(ACSR_Reg, ACD_Bit);
}

AC_OutputState_t AC_GetOutputState(void)
{
    return GET_BIT(ACSR_Reg, ACO_Bit) ? AC_OUTPUT_HIGH : AC_OUTPUT_LOW;
}

void AC_EnableTimer1Capture(void)
{
    SET_BIT(ACSR_Reg, ACIC_Bit);
}

void AC_DisableTimer1Capture(void)
{
    CLR_BIT(ACSR_Reg, ACIC_Bit);
}

void AC_EnableInterrupt(void)
{
    /* Always clear flag before enabling interrupt to prevent immediate spurious firing */
    SET_BIT(ACSR_Reg, ACI_Bit);
    SET_BIT(ACSR_Reg, ACIE_Bit);
}

void AC_DisableInterrupt(void)
{
    CLR_BIT(ACSR_Reg, ACIE_Bit);
}

void AC_SetCallback(void (*callback)(void))
{
    AC_Callback = callback;
}

/* -------------------------------------------------------------------------- */
/* Interrupt Service Routine (Vector 24: ANALOG_COMP_vect)                    */
/* -------------------------------------------------------------------------- */
void __vector_23(void) __attribute__((signal, used));
void __vector_23(void)
{
    if (AC_Callback != Null)
    {
        AC_Callback();
    }
}
