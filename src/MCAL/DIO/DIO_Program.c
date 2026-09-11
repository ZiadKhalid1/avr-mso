#include "DIO_Private.h"
#include "DIO_Interface.h"

/* Static inline helpers */
static inline volatile u8* DIO_GetPORT(DIO_Port_t port)
{
    switch (port)
    {
        case DIO_PORTB: return (volatile u8*)PORTB_ADDRESS;
        case DIO_PORTC: return (volatile u8*)PORTC_ADDRESS;
        case DIO_PORTD: return (volatile u8*)PORTD_ADDRESS;
        default:        return Null;
    }
}

static inline volatile u8* DIO_GetDDR(DIO_Port_t port)
{
    switch (port)
    {
        case DIO_PORTB: return (volatile u8*)DDRB_ADDRESS;
        case DIO_PORTC: return (volatile u8*)DDRC_ADDRESS;
        case DIO_PORTD: return (volatile u8*)DDRD_ADDRESS;
        default:        return Null;
    }
}

static inline volatile u8* DIO_GetPIN(DIO_Port_t port)
{
    switch (port)
    {
        case DIO_PORTB: return (volatile u8*)PINB_ADDRESS;
        case DIO_PORTC: return (volatile u8*)PINC_ADDRESS;
        case DIO_PORTD: return (volatile u8*)PIND_ADDRESS;
        default:        return Null;
    }
}

void DIO_SetPinDirection(DIO_Port_t port, DIO_Pin_t pin, DIO_Direction_t direction)
{
    if (pin > DIO_PIN7 || (port == DIO_PORTC && pin > DIO_PIN6)) return;

    volatile u8* ddr = DIO_GetDDR(port);
    if (ddr == Null) return;

    if (direction == DIO_DIRECTION_OUTPUT)
    {
        SET_BIT(*ddr, pin);
    }
    else
    {
        CLR_BIT(*ddr, pin);
    }
}

void DIO_SetPinValue(DIO_Port_t port, DIO_Pin_t pin, DIO_Level_t value)
{
    if (pin > DIO_PIN7 || (port == DIO_PORTC && pin > DIO_PIN6)) return;

    volatile u8* ddr = DIO_GetDDR(port);
    volatile u8* port_reg = DIO_GetPORT(port);
    if (ddr == Null || port_reg == Null) return;

    /* Verify pin is configured as output */
    if (GET_BIT(*ddr, pin) == 0) return;

    if (value == DIO_PIN_HIGH)
    {
        SET_BIT(*port_reg, pin);
    }
    else
    {
        CLR_BIT(*port_reg, pin);
    }
}

DIO_Level_t DIO_GetPinValue(DIO_Port_t port, DIO_Pin_t pin)
{
    if (pin > DIO_PIN7 || (port == DIO_PORTC && pin > DIO_PIN6)) return DIO_PIN_LOW;

    volatile u8* pin_reg = DIO_GetPIN(port);
    if (pin_reg == Null) return DIO_PIN_LOW;

    return GET_BIT(*pin_reg, pin) ? DIO_PIN_HIGH : DIO_PIN_LOW;
}

void DIO_TogglePinValue(DIO_Port_t port, DIO_Pin_t pin)
{
    if (pin > DIO_PIN7 || (port == DIO_PORTC && pin > DIO_PIN6)) return;

    volatile u8* ddr = DIO_GetDDR(port);
    volatile u8* pin_reg = DIO_GetPIN(port);
    if (ddr == Null || pin_reg == Null) return;

    /* Check if the pin is configured as output */
    if (GET_BIT(*ddr, pin) == 0) return;

    /* On ATmega328P, writing 1 to PIN toggles the corresponding PORT bit */
    *pin_reg = (1 << pin);
}

void DIO_SetPinPullup(DIO_Port_t port, DIO_Pin_t pin, DIO_Pullup_t pullup)
{
    if (pin > DIO_PIN7 || (port == DIO_PORTC && pin > DIO_PIN6)) return;

    volatile u8* ddr = DIO_GetDDR(port);
    volatile u8* port_reg = DIO_GetPORT(port);
    if (ddr == Null || port_reg == Null) return;

    if (pullup == DIO_PULLUP_ENABLE)
    {
        /* Defensive: switch to input mode first */
        CLR_BIT(*ddr, pin);
        SET_BIT(*port_reg, pin);
    }
    else
    {
        CLR_BIT(*port_reg, pin);
    }
}

void DIO_SetPortDirection(DIO_Port_t port, u8 direction_mask)
{
    volatile u8* ddr = DIO_GetDDR(port);
    if (ddr == Null) return;

    *ddr = direction_mask;
}

void DIO_SetPortPullup(DIO_Port_t port, u8 pullup_mask)
{
    volatile u8* ddr = DIO_GetDDR(port);
    volatile u8* port_reg = DIO_GetPORT(port);
    if (ddr == Null || port_reg == Null) return;

    /* Set target pins to input first before enabling pull-ups */
    *ddr &= ~pullup_mask;
    *port_reg = pullup_mask;
}

u8 DIO_ReadPort(DIO_Port_t port)
{
    u8 result = 0;

    switch (port)
    {
        case DIO_PORTB:
            __asm__ __volatile__ ("in %0, %1" : "=r" (result) : "I" (PINB_IO_ADDR));
            break;

        case DIO_PORTC:
            __asm__ __volatile__ ("in %0, %1" : "=r" (result) : "I" (PINC_IO_ADDR));
            break;

        case DIO_PORTD:
            __asm__ __volatile__ ("in %0, %1" : "=r" (result) : "I" (PIND_IO_ADDR));
            break;

        default:
            result = 0;
            break;
    }

    return result;
}
