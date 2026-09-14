#include "Controller_Interface.h"
#include "Controller_Private.h"
#include "../../MCAL/AC/AC_Interface.h"
#include "../../MCAL/ADC/ADC_Interface.h"
#include "../../MCAL/UART/UART_Interface.h"


static MSO_Mode_t g_current_mode = MSO_MODE_IDLE ;
static MSO_State_t g_current_state = MSO_STATE_STOPPED;
static CaptureBuffer_t g_shared_buffer;

void MSO_Control_Init(void)
{

}

void MSO_Control_ProcessCommand(void)
{
    /* Todo: Check if there a data in UART
     * if there make a switch case for the recieved byte
     * 'R' : run
     * 'S' : stop
     * 'L' : Logic analyzer
     * 'O' : OSCILLOSCOPE
     *  from 0:7 Set timeDive, but you check that in Oscilloscope can't set from(0:2)
     */

}

void MSO_Control_Update(void)
{
    /* Todo: Change the state based on current state */
    switch (g_current_state) {
        case MSO_STATE_STOPPED :
        break;
        case MSO_STATE_ARMED :
        if(g_current_mode == MSO_MODE_OSCILLOSCOPE)
        {
             AC_Init(&(AC_Config_t){
                 .negative_source = ,
                 .positive_source = ,
                 .interrupt_mode =
             });
        }
        break;
        case MSO_STATE_CAPTURING :
        break;
        case MSO_STATE_STREAMING :
        break;
        default:
        break;
    }

}

MSO_Mode_t  MSO_Control_GetMode(void)
{
    return g_current_mode;
}

MSO_State_t MSO_Control_GetState(void)
{
    return g_current_state;

}
