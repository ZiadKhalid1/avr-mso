#include "../../LIB/STD_TYPES.h"
#include "../../LIB/BIT_MATH.h"
#include "ADC_Interface.h"
#include "ADC_Private.h"

static void (*ADC_CallBack)(void) = Null;

void ADC_Init(ADC_Resolution_t resolution)
{
    /* Guard: reject invalid resolution */
    if (resolution != ADC_RES_8BIT && resolution != ADC_RES_10BIT)
    {
        return;
    }

    /* Voltage Reference selection: AVCC with external capacitor on AREF pin */
    CLR_BIT(ADMUX_Reg, REFS1_Bit);
    SET_BIT(ADMUX_Reg, REFS0_Bit);

    if (resolution == ADC_RES_8BIT)
    {
        /* Left Adjust Result: highest 8 bits in ADCH for fast single-cycle reads */
        SET_BIT(ADMUX_Reg, ADLAR_Bit);
    }
    else
    {
        /* Right Adjust Result: full 10-bit resolution across ADCL and ADCH */
        CLR_BIT(ADMUX_Reg, ADLAR_Bit);
    }

    /* Prescaler = 16 (16 MHz / 16 = 1 MHz ADC Clock) */
    SET_BIT(ADCSRA_Reg, ADPS2_Bit);
    CLR_BIT(ADCSRA_Reg, ADPS1_Bit);
    CLR_BIT(ADCSRA_Reg, ADPS0_Bit);
}

void ADC_StartAutoTrigger(ADC_Channel_t channel, ADC_TriggerSource_t trigger_src)
{
    /* Guard: reject invalid channel or trigger source */
    if (channel < ADC_MIN_CHANNEL || channel > ADC_MAX_CHANNEL)
    {
        return;
    }
    if (trigger_src < ADC_MIN_TRIGGER || trigger_src > ADC_MAX_TRIGGER)
    {
        return;
    }
/* Enable ADC Peripheral */
    SET_BIT(ADCSRA_Reg, ADEN_Bit);
    /* Select analog channel while preserving reference/alignment bits */
    ADMUX_Reg = (ADMUX_Reg & ADC_ADMUX_CFG_MASK) | (channel & ADC_CHANNEL_MASK);

    /* Configure trigger source in the lowest 3 bits of ADCSRB */
    ADCSRB_Reg = (ADCSRB_Reg & ADC_ADCSRB_CFG_MASK) | (trigger_src & ADC_TRIGGER_MASK);

    /* Enable Auto Trigger feature */
    SET_BIT(ADCSRA_Reg, ADATE_Bit);

    if (trigger_src == ADC_TRIG_FREE_RUNNING)
    {
        /* Free-running mode requires a manual start conversion pulse */
        SET_BIT(ADCSRA_Reg, ADSC_Bit);
    }
}

void ADC_StartSingleConversion(ADC_Channel_t channel)
{
    /* Guard: reject invalid channel */
    if (channel < ADC_MIN_CHANNEL || channel > ADC_MAX_CHANNEL)
    {
        return;
    }
    /* Enable ADC */
    SET_BIT(ADCSRA_Reg, ADEN_Bit);

    /* Disable Auto Trigger for manual single-conversion mode */
    CLR_BIT(ADCSRA_Reg, ADATE_Bit);

    /* Select target channel */
    ADMUX_Reg = (ADMUX_Reg & ADC_ADMUX_CFG_MASK) | (channel & ADC_CHANNEL_MASK);
    /* Clear flag */
    SET_BIT(ADCSRA_Reg, ADIF_Bit);
    /* Start conversion */
    SET_BIT(ADCSRA_Reg, ADSC_Bit);
}

u8 ADC_ReadSample8Bit(void)
{
    /* Guard: nothing to read while the ADC is disabled */
    if (GET_BIT(ADCSRA_Reg, ADEN_Bit) == 0)
    {
        return 0;
    }

    while (GET_BIT(ADCSRA_Reg, ADIF_Bit) == 0);

    SET_BIT(ADCSRA_Reg, ADIF_Bit);

    return ADCH_Reg;
}

u16 ADC_ReadSample10Bit(void)
{
    /* Guard: nothing to read while the ADC is disabled */
    if (GET_BIT(ADCSRA_Reg, ADEN_Bit) == 0)
    {
        return 0;
    }
    while (GET_BIT(ADCSRA_Reg, ADIF_Bit) == 0);

    SET_BIT(ADCSRA_Reg, ADIF_Bit);

    return ADC_Reg;
}

void ADC_EnableInterrupt(void (*callBackPtr)(void))
{
    /* Guard: do not install a null callback */
    if (callBackPtr == Null)
    {
        return;
    }

    ADC_CallBack = callBackPtr;
    SET_BIT(ADCSRA_Reg, ADIE_Bit);
}

void ADC_DisableInterrupt(void)
{
    /* Disable ADC Conversion Complete Interrupt */
    CLR_BIT(ADCSRA_Reg, ADIE_Bit);
    ADC_CallBack = Null;
}

void ADC_Stop(void)
{
    /* Disable ADC, disable auto-triggering, and mask interrupts */
    CLR_BIT(ADCSRA_Reg, ADEN_Bit);
    CLR_BIT(ADCSRA_Reg, ADATE_Bit);
    CLR_BIT(ADCSRA_Reg, ADIE_Bit);
}

void __vector_21(void) __attribute__((signal, used));

void __vector_21(void)
{
    if (ADC_CallBack != Null)
    {
        ADC_CallBack();
    }
}
