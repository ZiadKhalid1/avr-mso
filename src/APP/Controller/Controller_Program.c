#include "Controller_Interface.h"
#include "Controller_Private.h"
#include "../../MCAL/DIO/DIO_Interface.h"
#include "../../MCAL/ADC/ADC_Private.h"
#include "../../MCAL/AC/AC_Interface.h"
#include "../../MCAL/ADC/ADC_Interface.h"
#include "../../MCAL/UART/UART_Interface.h"
#include "../../MCAL/TIMER1/Timer1_Interface.h"

/* Frame Protocol Constants */
#define MSO_FRAME_HEADER_1        0xAA
#define MSO_FRAME_HEADER_2        0x55
#define MSO_FRAME_FOOTER_CR       0x0D
#define MSO_FRAME_FOOTER_LF       0x0A
#define MSO_BUFFER_SIZE_OSC       256U
#define MSO_BUFFER_SIZE_LA        512U

/* State Variables */
static MSO_Mode_t    g_current_mode    = MSO_MODE_OSCILLOSCOPE;
static MSO_State_t   g_current_state   = MSO_STATE_STOPPED;
static MSO_TimeDiv_t g_current_timediv = MSO_TIMEDIV_1MS;

/* Shared memory buffer */
static CaptureBuffer_t g_shared_buffer;
static volatile u16    g_sample_index = 0;

/* Logic Analyzer helper: 100% Hardware Timer Driven ISR */
static void MSO_LogicAnalyzer_SampleISR(void)
{
    if (g_current_state == MSO_STATE_CAPTURING && g_current_mode == MSO_MODE_LOGIC_ANALYZER)
    {
        if (g_sample_index < MSO_BUFFER_SIZE_LA)
        {
            /* Sample 6 digital channels [PB0:PB5] */
            g_shared_buffer.dso_samples[g_sample_index++] = (DIO_ReadPort(DIO_PORTB) & 0x3F);
        }
        else
        {
            Timer1_Stop();
            Timer1_DisableCompareAInt();
            g_current_state = MSO_STATE_STREAMING;
        }
    }
}

/* DSO timer helper: Timer1 COMPA ISR triggers single ADC conversion at pacing interval */
static void MSO_DSO_TimerISR(void)
{
    if (g_current_state == MSO_STATE_CAPTURING && g_current_mode == MSO_MODE_OSCILLOSCOPE)
    {
        if (g_sample_index < MSO_BUFFER_SIZE_OSC)
        {
            ADC_StartSingleConversion(ADC_CHANNEL_0);
        }
    }
}

/* DSO helper: ADC Conversion Complete ISR */
static void MSO_DSO_SampleISR(void)
{
    if (g_current_state == MSO_STATE_CAPTURING && g_current_mode == MSO_MODE_OSCILLOSCOPE)
    {
        if (g_sample_index < MSO_BUFFER_SIZE_OSC)
        {
            /* Read the latched 8-bit result (ADLAR set) directly; no poll */
            g_shared_buffer.dso_samples[g_sample_index++] = ADCH_Reg;
        }

        if (g_sample_index >= MSO_BUFFER_SIZE_OSC)
        {
            ADC_Stop();
            ADC_DisableInterrupt();
            Timer1_Stop();
            Timer1_DisableCompareAInt();
            Timer1_DisableCompareBInt();
            g_current_state = MSO_STATE_STREAMING;
        }
    }
}

void MSO_Control_Init(void)
{
    g_current_mode    = MSO_MODE_OSCILLOSCOPE;
    g_current_state   = MSO_STATE_STOPPED;
    g_current_timediv = MSO_TIMEDIV_1MS;
    g_sample_index    = 0;

    Timer1_SetCompareACallback(MSO_LogicAnalyzer_SampleISR);
}

void MSO_Control_ProcessCommand(void)
{
    while (UART_IsDataAvailable())
    {
        MSO_Control_HandleKey(UART_ReceiveByte());
    }
}

void MSO_Control_HandleKey(u8 cmd)
{
    switch (cmd)
    {
        case 'R':
        case 'r':
            if (g_current_state == MSO_STATE_STOPPED)
            {
                g_current_state = MSO_STATE_ARMED;
            }
            break;

        case 'S':
        case 's':
            MSO_Control_ForceStop();
            break;

        case 'O':
        case 'o':
            if (g_current_state == MSO_STATE_STOPPED)
            {
                g_current_mode = MSO_MODE_OSCILLOSCOPE;
            }
            break;

        case 'L':
        case 'l':
            if (g_current_state == MSO_STATE_STOPPED)
            {
                g_current_mode = MSO_MODE_LOGIC_ANALYZER;
            }
            break;

        default:
            if (cmd >= '0' && cmd <= '7')
            {
                MSO_TimeDiv_t new_scale = (MSO_TimeDiv_t)(cmd - '0');
                MSO_Control_SetTimeDiv(new_scale);
            }
            break;
    }
}

void MSO_Control_SetTimeDiv(MSO_TimeDiv_t timeDiv)
{
    if (timeDiv >= MSO_TIMEDIV_COUNT)
    {
        return;
    }

    /* Clamp DSO mode to the highest sustainable ADC hardware sampling rate */
    if (g_current_mode == MSO_MODE_OSCILLOSCOPE && timeDiv < MSO_TIMEDIV_500US)
    {
        timeDiv = MSO_TIMEDIV_500US;
    }

    g_current_timediv = timeDiv;
}

MSO_TimeDiv_t MSO_Control_GetTimeDiv(void)
{
    return g_current_timediv;
}

void MSO_Control_ForceStop(void)
{
    Timer1_Stop();
    Timer1_DisableCompareAInt();
    Timer1_DisableCompareBInt();

    ADC_Stop();
    ADC_DisableInterrupt();

    AC_DisableInterrupt();
    AC_Disable();

    g_sample_index  = 0;
    g_current_state = MSO_STATE_STOPPED;
}

void MSO_Control_Update(void)
{
    switch (g_current_state)
    {
        case MSO_STATE_STOPPED:
            /* Idle state: awaiting user RUN command */
            break;

        case MSO_STATE_ARMED:
            g_sample_index = 0;

            if (g_current_mode == MSO_MODE_OSCILLOSCOPE)
            {
                /* Auto-trigger: acquisition starts on RUN without requiring an
                 * external analog signal. Fast scales free-run the ADC; slow
                 * scales pace the ADC with Timer1 COMPA. */
                ADC_Stop();
                ADC_DisableInterrupt();
                Timer1_Stop();
                Timer1_DisableCompareAInt();
                Timer1_DisableCompareBInt();

                ADC_EnableInterrupt(MSO_DSO_SampleISR);

                if (g_current_timediv <= MSO_TIMEDIV_500US)
                {
                    ADC_StartAutoTrigger(ADC_CHANNEL_0, ADC_TRIG_FREE_RUNNING);
                }
                else
                {
                    Timer1_Init(TIMER1_MODE_CTC_TOP_OCR1A, TIMER1_PRESCALER_8);

                    switch (g_current_timediv)
                    {
                        case MSO_TIMEDIV_1MS:   Timer1_SetCompareA(78);   break;
                        case MSO_TIMEDIV_5MS:   Timer1_SetCompareA(390);  break;
                        case MSO_TIMEDIV_10MS:  Timer1_SetCompareA(780);  break;
                        case MSO_TIMEDIV_50MS:  Timer1_SetCompareA(3906); break;
                        default:                Timer1_SetCompareA(78);   break;
                    }
                    Timer1_SetCompareACallback(MSO_DSO_TimerISR);
                    Timer1_EnableCompareAInt();
                    ADC_StartSingleConversion(ADC_CHANNEL_0);
                }

                g_current_state = MSO_STATE_CAPTURING;
            }
            else /* MSO_MODE_LOGIC_ANALYZER */
            {
                /* Configure PB0..PB5 as inputs with internal pull-ups enabled
                 * so un-probed channels remain stably HIGH (logic 1) instead of floating. */
                DIO_SetPortDirection(DIO_PORTB, 0x00);
                DIO_SetPortPullup(DIO_PORTB, 0x3F);

                /* Timer1-paced sampling starts immediately; no blocking
                 * software trigger spins on the input pins. */
                Timer1_Init(TIMER1_MODE_CTC_TOP_OCR1A, TIMER1_PRESCALER_1);
                Timer1_SetCompareACallback(MSO_LogicAnalyzer_SampleISR);

                switch (g_current_timediv)
                {
                    case MSO_TIMEDIV_10US:  Timer1_SetCompareA(1);    break;
                    case MSO_TIMEDIV_50US:  Timer1_SetCompareA(4);    break;
                    case MSO_TIMEDIV_100US: Timer1_SetCompareA(8);    break;
                    case MSO_TIMEDIV_500US: Timer1_SetCompareA(39);   break;
                    case MSO_TIMEDIV_1MS:   Timer1_SetCompareA(78);   break;
                    case MSO_TIMEDIV_5MS:   Timer1_SetCompareA(390);  break;
                    case MSO_TIMEDIV_10MS:  Timer1_SetCompareA(780);  break;
                    case MSO_TIMEDIV_50MS:  Timer1_SetCompareA(3906); break;
                    default:                Timer1_SetCompareA(78);   break;
                }

                Timer1_EnableCompareAInt();
                g_current_state = MSO_STATE_CAPTURING;
            }
            break;

        case MSO_STATE_CAPTURING:
            /* Sampling handled by hardware ISRs */
            break;

        case MSO_STATE_STREAMING:
            UART_SendByte(MSO_FRAME_HEADER_1);
            UART_SendByte(MSO_FRAME_HEADER_2);

            if (g_current_mode == MSO_MODE_OSCILLOSCOPE)
            {
                UART_SendBuffer(g_shared_buffer.dso_samples, MSO_BUFFER_SIZE_OSC);
            }
            else if (g_current_mode == MSO_MODE_LOGIC_ANALYZER)
            {
                UART_SendBuffer(g_shared_buffer.dso_samples, MSO_BUFFER_SIZE_LA);
            }

            UART_SendByte(MSO_FRAME_FOOTER_CR);
            UART_SendByte(MSO_FRAME_FOOTER_LF);

            g_current_state = MSO_STATE_ARMED;
            break;

        default:
            g_current_state = MSO_STATE_STOPPED;
            break;
    }
}

MSO_Mode_t MSO_Control_GetMode(void)
{
    return g_current_mode;
}

MSO_State_t MSO_Control_GetState(void)
{
    return g_current_state;
}
