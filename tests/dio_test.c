/* tests/dio_test.c
 * Hostless unit test for the MCAL DIO driver.
 * Runs on an ATmega328P under simavr and reports over UART0.
 * The Makefile 'test' target drives: simavr -> grep "DIO_RESULT PASS".
 */
#include <avr/io.h>
#include "MCAL/DIO/DIO_Interface.h"

static u16 g_pass = 0;
static u16 g_fail = 0;

static void uart_init(void)
{
    UBRR0 = 0;
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ00) | (1 << UCSZ01);
}

static void uart_putc(char c)
{
    while (!(UCSR0A & (1 << UDRE0))) ;
    UDR0 = c;
}

static void uart_str(const char *s)
{
    while (*s) uart_putc(*s++);
}

static void uart_hex8(u8 v)
{
    const char *digits = "0123456789ABCDEF";
    uart_putc(digits[(v >> 4) & 0x0F]);
    uart_putc(digits[v & 0x0F]);
}

static void check(int ok, const char *name, u8 ddr, u8 port, u8 pin)
{
    uart_str(ok ? "PASS " : "FAIL ");
    uart_str(name);
    uart_str("  DDR=");
    uart_hex8(ddr);
    uart_str(" PORT=");
    uart_hex8(port);
    uart_str(" PIN=");
    uart_hex8(pin);
    uart_str("\n");
    if (ok) g_pass++;
    else    g_fail++;
}

int main(void)
{
    uart_init();
    uart_str("== DIO driver test start ==\n");

    /* ---------------- pin direction ---------------- */
    DDRB = 0; PORTB = 0;
    DIO_SetPinDirection(DIO_PORTB, DIO_PIN0, DIO_DIRECTION_OUTPUT);
    check((DDRB & (1 << 0)) != 0, "set-pin-direction-output", DDRB, PORTB, PINB);

    DIO_SetPinDirection(DIO_PORTB, DIO_PIN0, DIO_DIRECTION_INPUT);
    check((DDRB & (1 << 0)) == 0, "set-pin-direction-input", DDRB, PORTB, PINB);

    /* PORTC has only pins 0..6; pin 7 must be rejected */
    DDRC = 0;
    DIO_SetPinDirection(DIO_PORTC, DIO_PIN7, DIO_DIRECTION_OUTPUT);
    check(DDRC == 0, "reject-out-of-range-portc-pin7", DDRC, PORTC, PINC);

    /* ---------------- pin value ---------------- */
    DDRB = (1 << 1); PORTB = 0;
    DIO_SetPinValue(DIO_PORTB, DIO_PIN1, DIO_PIN_HIGH);
    check((PORTB & (1 << 1)) != 0, "set-pin-value-high", DDRB, PORTB, PINB);

    DIO_SetPinValue(DIO_PORTB, DIO_PIN1, DIO_PIN_LOW);
    check((PORTB & (1 << 1)) == 0, "set-pin-value-low", DDRB, PORTB, PINB);

    /* writing to an input-only pin must be ignored */
    DDRB = 0; PORTB = (1 << 1);
    DIO_SetPinValue(DIO_PORTB, DIO_PIN1, DIO_PIN_LOW);
    check((PORTB & (1 << 1)) != 0, "ignore-write-on-input-pin", DDRB, PORTB, PINB);

    /* ---------------- toggle ---------------- */
    DDRB = (1 << 2); PORTB = 0;
    DIO_TogglePinValue(DIO_PORTB, DIO_PIN2);
    check((PORTB & (1 << 2)) != 0, "toggle-pin-high", DDRB, PORTB, PINB);

    DIO_TogglePinValue(DIO_PORTB, DIO_PIN2);
    check((PORTB & (1 << 2)) == 0, "toggle-pin-low", DDRB, PORTB, PINB);

    /* toggle on an input pin must not be applied */
    DDRB = 0; PORTB = 0;
    DIO_TogglePinValue(DIO_PORTB, DIO_PIN3);
    check((PORTB & (1 << 3)) == 0, "ignore-toggle-on-input-pin", DDRB, PORTB, PINB);

    /* ---------------- pullup ---------------- */
    DDRD = 0xFF; PORTD = 0;
    DIO_SetPinPullup(DIO_PORTD, DIO_PIN2, DIO_PULLUP_ENABLE);
    check((DDRD & (1 << 2)) == 0 && (PORTD & (1 << 2)) != 0,
          "pullup-enable-clears-ddr-sets-port", DDRD, PORTD, PIND);

    DIO_SetPinPullup(DIO_PORTD, DIO_PIN2, DIO_PULLUP_DISABLE);
    check((PORTD & (1 << 2)) == 0, "pullup-disable-clears-port", DDRD, PORTD, PIND);

    /* ---------------- port-level ---------------- */
    DDRC = 0;
    DIO_SetPortDirection(DIO_PORTC, 0x3F);
    check(DDRC == 0x3F, "set-port-direction-mask", DDRC, PORTC, PINC);

    DDRC = 0x3F; PORTC = 0;
    DIO_SetPortPullup(DIO_PORTC, 0x05);
    check((DDRC & 0x05) == 0 && (PORTC & 0x05) == 0x05,
          "set-port-pullup-mask", DDRC, PORTC, PINC);

    /* ---------------- readback ---------------- */
    /* input pin with internal pull-up reads back high through DIO_GetPinValue */
    DDRC = 0; PORTC = (1 << 0);
    check(DIO_GetPinValue(DIO_PORTC, DIO_PIN0) == DIO_PIN_HIGH,
          "get-pin-value-pullup-high", DDRC, PORTC, PINC);

    /* DIO_ReadPort must return all ones when all pins are pulled up */
    DDRC = 0; PORTC = 0xFF;
    check(DIO_ReadPort(DIO_PORTC) == 0xFF, "read-port-all-high", DDRC, PORTC, PINC);

    /* DIO_ReadPort must reflect pulled-up pins (low bits may float, only
     * assert the pins we drive with an internal pull-up) */
    DDRD = 0; PORTD = 0xAA;
    check((DIO_ReadPort(DIO_PORTD) & 0xAA) == 0xAA, "read-port-mask", DDRD, PORTD, PIND);

    /* ---------------- result ---------------- */
    if (g_fail == 0)
    {
        uart_str("DIO_RESULT PASS ");
    }
    else
    {
        uart_str("DIO_RESULT FAIL ");
    }
    uart_hex8((u8)g_pass);
    uart_putc('/');
    uart_hex8((u8)(g_pass + g_fail));
    uart_str("\n");

    /* simavr runs until killed by the harness */
    for (;;) ;
}