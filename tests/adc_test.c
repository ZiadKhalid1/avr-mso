/* tests/adc_test.c
 * Hostless unit test for the MCAL ADC driver.
 * Runs on an ATmega328P under simavr and reports over UART0.
 * The Makefile 'test-adc' target drives: simavr -> grep "ADC_RESULT PASS".
 *
 * Driver API under test (spec-aligned 8-bit DSO front end):
 *   ADC_Init(ADC_RES_8BIT)      -> AVCC ref, ADLAR=1, prescaler=16, ADC LEFT OFF
 *   ADC_StartAutoTrigger(ch,src) -> ADEN, ADATE, (free-run) ADSC pulse
 *   ADC_StartSingleConversion(ch)             -> one-shot ADSC pulse
 *   ADC_ReadSample8Bit / 10Bit                -> waits ADIF, clears it, reads reg
 *
 * NOTE on simavr hardware semantics (faithfully modeled for ATmega328P):
 *  - ADIF is write-1-to-clear; it can only be SET by a hardware conversion
 *    completing. The read functions poll ADIF, so value tests must start a
 *    real conversion, wait for completion, then run the read. A completed
 *    conversion overwrites ADCH/ADCL with the model result, so the expected
 *    byte/half-word is written into the data registers AFTER completion and
 *    immediately before the driver read (ADSC is already 0, nothing to
 *    overwrite it).
 *  - Writing 0 to ADSC does NOT clear an in-progress conversion, so the
 *    ADSC-pulse tests run last, after every exact-register assertion.
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

/* ---- helpers ---- */
static void wait_adif(void)
{
    while ((ADCSRA & (1 << ADIF)) == 0) ;
}

/* ADIF can only be cleared by writing a one to it */
static void clear_adif(void)
{
    ADCSRA = (1 << ADIF);
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

    /* ---------------- ADC_Init 8-bit ----------------
     * Prescaler = 16 (ADPS[2:0] = 100) => f_ADC = 1 MHz.
     * ADEN must stay 0: the AC-trigger handover requires the ADC core to be
     * disabled until the hardware trigger fires (spec section 2). */
    ADMUX = 0; ADCSRA = 0; ADCSRB = 0;
    ADC_Init(ADC_RES_8BIT);
    check((ADMUX & (1 << ADLAR)) != 0 && (ADMUX & (1 << REFS0)) != 0
          && (ADMUX & (1 << REFS1)) == 0,
          "init-8bit-refs-adlar", ADCSRA, ADMUX, ADCSRB);
    check(ADCSRA == (1 << ADPS2), "init-8bit-prescaler-16", ADCSRA, ADMUX, ADCSRB);
    check((ADCSRA & (1 << ADEN)) == 0, "init-8bit-adc-stays-off",
          ADCSRA, ADMUX, ADCSRB);

    /* ---------------- ADC_Init 10-bit ---------------- */
    ADMUX = 0; ADCSRA = 0;
    ADC_Init(ADC_RES_10BIT);
    check((ADMUX & (1 << ADLAR)) == 0, "init-10bit-clears-adlar",
          ADCSRA, ADMUX, ADCSRB);

    /* ---------------- guards: read with ADC disabled ---------------- */
    ADCSRA = 0; ADCH = 0xFF;
    {
        u8 r8 = ADC_ReadSample8Bit();
        checkw(r8 == 0, "read-8bit-guard-adc-disabled", r8, 0);
    }
    ADCSRA = 0; ADCL = 0xFF; ADCH = 0xFF;
    {
        u16 r16 = ADC_ReadSample10Bit();
        checkw(r16 == 0, "read-10bit-guard-adc-disabled", r16, 0);
    }

    /* ---------------- ADC_StartAutoTrigger guard ---------------- */
    ADMUX = 0; ADCSRA = 0; ADCSRB = 0;
    ADC_StartAutoTrigger((ADC_Channel_t)8, ADC_TRIG_FREE_RUNNING);
    check(ADMUX == 0 && ADCSRA == 0 && ADCSRB == 0,
          "auto-trigger-reject-invalid-channel", ADCSRA, ADMUX, ADCSRB);

    /* ---------------- ADC_StartSingleConversion guard ---------------- */
    ADMUX = 0; ADCSRA = 0; ADCSRB = 0;
    ADC_StartSingleConversion((ADC_Channel_t)9);
    check(ADMUX == 0 && ADCSRA == 0, "single-conversion-reject-invalid-channel",
          ADCSRA, ADMUX, ADCSRB);

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

    /* ---------------- ADC_Stop ---------------- */
    clear_adif();
    ADCSRA = (1 << ADATE) | (1 << ADIE);
    ADC_Stop();
    check(ADCSRA == 0, "stop-clears-aden-adate-adie", ADCSRA, ADMUX, ADCSRB);

    /* ---------------- ADC_ReadSample8Bit (left-adjusted 8-bit) --------
     * Start a real conversion so simavr's hardware completion raises ADIF,
     * then plant the expected byte in ADCH (conversion already finished,
     * ADSC=0) and let the driver read+clear it. */
    clear_adif();
    ADMUX = 0; ADCSRA = 0; ADCSRB = 0;
    ADC_Init(ADC_RES_8BIT);
    ADC_StartSingleConversion(ADC_CHANNEL_0);
    wait_adif();
    ADCH = 0xA5;
    {
        u8 r8 = ADC_ReadSample8Bit();
        checkw(r8 == 0xA5, "read-sample-8bit", r8, 0xA5);
    }
    check((ADCSRA & (1 << ADIF)) == 0, "read-8bit-clears-adif",
          ADCSRA, ADMUX, ADCSRB);

    /* ---------------- ADC_ReadSample10Bit (right-adjusted) ---------------- */
    clear_adif();
    ADMUX = 0; ADCSRA = 0; ADCSRB = 0;
    ADC_Init(ADC_RES_10BIT);
    ADC_StartSingleConversion(ADC_CHANNEL_0);
    wait_adif();
    ADCL = 0x34; ADCH = 0x02;
    {
        u16 r16 = ADC_ReadSample10Bit();
        checkw(r16 == 0x0234, "read-sample-10bit", r16, 0x0234);
    }
    check((ADCSRA & (1 << ADIF)) == 0, "read-10bit-clears-adif",
          ADCSRA, ADMUX, ADCSRB);

    /* ---------------- ADSC-pulse tests (last) ----------------
     * simavr completes a single conversion in ~400 CPU cycles (first
     * conversion = 14 ADC clocks + 11 ADC clocks, prescaler 16), far faster
     * than a UART test line prints. A conversion in flight therefore mutates
     * ADCSRA between two successive check() calls (ADIF raises, ADSC drops),
     * so ADCSRA is snapshot immediately after the start call and every
     * assertion of this group runs against that frozen snapshot. */
    clear_adif();
    ADMUX = 0xE0; ADCSRA = (1 << ADATE) | (1 << ADIF); ADCSRB = 0;
    ADC_StartSingleConversion(ADC_CHANNEL_3);
    {
        u8 s = ADCSRA;
        check((ADMUX & 0x07) == 3 && (ADMUX & 0xE0) == 0xE0,
              "single-conversion-channel3", s, ADMUX, ADCSRB);
        check((s & (1 << ADATE)) == 0, "single-conversion-disables-adate",
              s, ADMUX, ADCSRB);
        check((s & (1 << ADEN)) != 0, "single-conversion-enables-adc",
              s, ADMUX, ADCSRB);
        check((s & (1 << ADIF)) == 0, "single-conversion-clears-adif",
              s, ADMUX, ADCSRB);
        check((s & (1 << ADSC)) != 0, "single-conversion-pulses-adsc",
              s, ADMUX, ADCSRB);
    }

    clear_adif();
    ADMUX = 0x60; ADCSRA = 0; ADCSRB = 0;
    ADC_StartAutoTrigger(ADC_CHANNEL_5, ADC_TRIG_FREE_RUNNING);
    {
        u8 s = ADCSRA;
        check((ADMUX & 0x07) == 5 && (ADMUX & 0xE0) == 0x60 && (ADMUX & 0x10) == 0,
              "auto-trigger-channel5-reserved-clear", s, ADMUX, ADCSRB);
        check((ADCSRB & 0x07) == 0, "auto-trigger-free-running-adts",
              s, ADMUX, ADCSRB);
        check((s & (1 << ADATE)) != 0, "auto-trigger-sets-adate",
              s, ADMUX, ADCSRB);
        check((s & (1 << ADEN)) != 0, "auto-trigger-enables-adc",
              s, ADMUX, ADCSRB);
        check((s & (1 << ADIF)) == 0, "auto-trigger-clears-adif",
              s, ADMUX, ADCSRB);
        check((s & (1 << ADSC)) != 0, "auto-trigger-free-running-pulses-adsc",
              s, ADMUX, ADCSRB);
    }

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