/* tests/adc_test.c
 * Hostless unit test for the MCAL ADC driver.
 * Runs on an ATmega328P under simavr and reports over UART0.
 * The Makefile 'test-adc' target drives: simavr -> grep "ADC_RESULT PASS".
 *
 * NOTE on ATmega328P hardware semantics (faithfully modeled by simavr):
 *  - Writing 0 to ADCSRA's ADSC bit does NOT clear it; a conversion already
 *    in progress keeps ADSC=1 until the hardware finishes it. Tests that
 *    assert exact ADCSRA contents therefore run BEFORE any test sets ADSC,
 *    and the ADSC-pulse tests (single conversion / free running) are last.
 */
#include <avr/io.h>
#include "MCAL/ADC/ADC_Interface.h"

extern void __vector_21(void);  /* ADC Conversion Complete ISR */

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

static void uart_hex16(u16 v)
{
    uart_hex8((u8)(v >> 8));
    uart_hex8((u8)(v & 0xFF));
}

static void uart_num(u16 v)
{
    char buf[6];
    u8  i = 0;
    if (v == 0) { uart_putc('0'); return; }
    while (v > 0) { buf[i++] = '0' + (v % 10); v /= 10; }
    while (i > 0) uart_putc(buf[--i]);
}

static void check(int ok, const char *name, u8 adcsra, u8 admux, u8 adcsrb)
{
    uart_str(ok ? "PASS " : "FAIL ");
    uart_str(name);
    uart_str("  ADCSRA=");
    uart_hex8(adcsra);
    uart_str(" ADMUX=");
    uart_hex8(admux);
    uart_str(" ADCSRB=");
    uart_hex8(adcsrb);
    uart_str("\n");
    if (ok) g_pass++;
    else    g_fail++;
}

static void checkw(int ok, const char *name, u16 got, u16 want)
{
    uart_str(ok ? "PASS " : "FAIL ");
    uart_str(name);
    uart_str("  got=");
    uart_hex16(got);
    uart_str(" want=");
    uart_hex16(want);
    uart_str("\n");
    if (ok) g_pass++;
    else    g_fail++;
}

/* ---- callback for the ISR dispatch test ---- */
static u8 g_cb_called = 0;
static void adc_callback(void)
{
    g_cb_called = 1;
}

int main(void)
{
    uart_init();
    uart_str("== ADC driver test start ==\n");

    /* ---------------- ADC_Init guard ---------------- */
    ADMUX = 0; ADCSRA = 0; ADCSRB = 0;
    ADC_Init((ADC_Resolution_t)99);
    check(ADMUX == 0 && ADCSRA == 0, "init-reject-invalid-resolution",
          ADCSRA, ADMUX, ADCSRB);

    /* ---------------- ADC_Init 8-bit ---------------- */
    ADMUX = 0; ADCSRA = 0;
    ADC_Init(ADC_RES_8BIT);
    check((ADMUX & (1 << ADLAR)) != 0 && (ADMUX & (1 << REFS0)) != 0
          && (ADMUX & (1 << REFS1)) == 0,
          "init-8bit-refs-adlar", ADCSRA, ADMUX, ADCSRB);
    check((ADCSRA & ((1 << ADEN) | (1 << ADPS2) | (1 << ADPS0))) != 0
          && (ADCSRA & (1 << ADPS1)) == 0,
          "init-8bit-enable-prescaler-32", ADCSRA, ADMUX, ADCSRB);

    /* ---------------- ADC_Init 10-bit ---------------- */
    ADMUX = 0; ADCSRA = 0;
    ADC_Init(ADC_RES_10BIT);
    check((ADMUX & (1 << ADLAR)) == 0, "init-10bit-clears-adlar",
          ADCSRA, ADMUX, ADCSRB);

    /* ---------------- ADC_StartAutoTrigger guards ---------------- */
    ADMUX = 0; ADCSRA = 0; ADCSRB = 0;
    ADC_StartAutoTrigger((ADC_Channel_t)8, ADC_TRIG_FREE_RUNNING);
    check(ADMUX == 0 && ADCSRA == 0 && ADCSRB == 0,
          "auto-trigger-reject-invalid-channel", ADCSRA, ADMUX, ADCSRB);

    ADMUX = 0; ADCSRA = 0; ADCSRB = 0;
    ADC_StartAutoTrigger(ADC_CHANNEL_0, (ADC_TriggerSource_t)8);
    check(ADMUX == 0 && ADCSRA == 0 && ADCSRB == 0,
          "auto-trigger-reject-invalid-source", ADCSRA, ADMUX, ADCSRB);

    /* ---------------- ADC_StartAutoTrigger ---------------- */
    ADMUX = 0xE0; ADCSRA = 0; ADCSRB = 0;
    ADC_StartAutoTrigger(ADC_CHANNEL_5, ADC_TRIG_TIMER1_CAPTURE);
    check((ADMUX & 0x0F) == 5 && (ADMUX & 0xE0) == 0xE0,
          "auto-trigger-channel5", ADCSRA, ADMUX, ADCSRB);
    check((ADCSRB & 0x07) == 7 && (ADCSRA & (1 << ADATE)) != 0,
          "auto-trigger-source7", ADCSRA, ADMUX, ADCSRB);
    check((ADMUX & 0x10) == 0, "reserved-admux-bit4-stays-clear",
          ADCSRA, ADMUX, ADCSRB);

    /* ---------------- ADC_StartSingleConversion guard ---------------- */
    ADMUX = 0; ADCSRA = 0; ADCSRB = 0;
    ADC_StartSingleConversion((ADC_Channel_t)9);
    check(ADMUX == 0 && ADCSRA == 0, "single-conversion-reject-invalid-channel",
          ADCSRA, ADMUX, ADCSRB);

    /* ---------------- ADC_ReadSample8Bit ---------------- */
    ADCSRA = (1 << ADEN);
    ADCH = 0xA5;
    checkw(ADC_ReadSample8Bit() == 0xA5, "read-sample-8bit",
           ADC_ReadSample8Bit(), 0xA5);

    /* ---------------- ADC_ReadSample10Bit ----------------
     * The ADC is enabled, and no conversion is running (ADSC=0), so the
     * poll exits immediately; the result returns straight from ADCL/ADCH. */
    ADCSRA = (1 << ADEN);
    ADCL = 0x34; ADCH = 0x02;
    checkw(ADC_ReadSample10Bit() == 0x0234, "read-sample-10bit",
           ADC_ReadSample10Bit(), 0x0234);

    /* ---------------- ADC_EnableInterrupt ---------------- */
    ADCSRA = 0;
    ADC_EnableInterrupt((void (*)(void))0);
    check((ADCSRA & (1 << ADIE)) == 0, "enable-interrupt-reject-null-callback",
          ADCSRA, ADMUX, ADCSRB);

    g_cb_called = 0;
    ADC_EnableInterrupt(adc_callback);
    check((ADCSRA & (1 << ADIE)) != 0, "enable-interrupt-sets-adie",
          ADCSRA, ADMUX, ADCSRB);

    __vector_21();
    check(g_cb_called == 1, "isr-invokes-callback", ADCSRA, ADMUX, ADCSRB);

    g_cb_called = 0;
    ADC_DisableInterrupt();
    check((ADCSRA & (1 << ADIE)) == 0, "disable-interrupt-clears-adie",
          ADCSRA, ADMUX, ADCSRB);

    __vector_21();
    check(g_cb_called == 0, "isr-idle-after-disable", ADCSRA, ADMUX, ADCSRB);

    /* ---------------- ADC_Stop ----------------
     * ADSC must be 0 here (hardware cannot clear it via software), so only
     * ADATE/ADIE are set; ADC_Stop clears them. */
    ADCSRA = (1 << ADATE) | (1 << ADIE);
    ADC_Stop();
    check(ADCSRA == 0, "stop-clears-adate-adie", ADCSRA, ADMUX, ADCSRB);

    /* ---------------- ADSC-pulse tests (last) ----------------
     * ADC_StartSingleConversion / free running set ADSC; once set it cannot
     * be cleared by software, so everything above must already be done. */
    ADMUX = 0xE0; ADCSRA = (1 << ADATE); ADCSRB = 0;
    ADC_StartSingleConversion(ADC_CHANNEL_3);
    check((ADMUX & 0x0F) == 3 && (ADMUX & 0xE0) == 0xE0,
          "single-conversion-channel3", ADCSRA, ADMUX, ADCSRB);
    check((ADCSRA & (1 << ADSC)) != 0 && (ADCSRA & (1 << ADATE)) == 0,
          "single-conversion-start", ADCSRA, ADMUX, ADCSRB);

    ADC_StartAutoTrigger(ADC_CHANNEL_0, ADC_TRIG_FREE_RUNNING);
    check((ADCSRA & (1 << ADSC)) != 0, "auto-trigger-free-running-pulses-adsc",
          ADCSRA, ADMUX, ADCSRB);

    /* ---------------- result ---------------- */
    if (g_fail == 0)
    {
        uart_str("ADC_RESULT PASS ");
    }
    else
    {
        uart_str("ADC_RESULT FAIL ");
    }
    uart_num(g_pass);
    uart_putc('/');
    uart_num(g_pass + g_fail);
    uart_str("\n");

    /* simavr runs until killed by the harness */
    for (;;) ;
}