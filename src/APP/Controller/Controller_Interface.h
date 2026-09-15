/**
 * @file    MSO_Control_Interface.h
 * @brief   Application-layer coordinator for the Mixed-Signal Oscilloscope (MSO)
 *          and Logic Analyzer state machine, Time/Div horizontal scaling,
 *          and host GUI command decoding.
 * @details This interface encapsulates instrument mode switching (Analog DSO vs.
 *          Digital Logic Analyzer), acquisition lifecycle states, and dynamic
 *          sample rate selection (Time/Div) to scale the horizontal timebase.
 *          It decouples high-level operational commands from low-level MCAL
 *          peripheral registers (ADC, AC, Timer1, DIO, UART).
 *
 * @author  Embedded Systems Team
 * @date    2026-09-14
 * @version 1.1.0
 */

#ifndef MSO_CONTROL_INTERFACE_H
#define MSO_CONTROL_INTERFACE_H

#include "../../LIB/STD_TYPES.h"

/* ========================================================================= */
/*                              Type Definitions                             */
/* ========================================================================= */

/**
 * @brief Active operational acquisition channels.
 */
typedef enum {
    MSO_MODE_IDLE = 0,          /**< Acquisition engine disabled; low-power stand-by. */
    MSO_MODE_OSCILLOSCOPE,      /**< Analog acquisition via PC0/ADC0 (8-bit resolution). */
    MSO_MODE_LOGIC_ANALYZER     /**< 6-channel digital bus acquisition via PINB[0:5]. */
} MSO_Mode_t;

/**
 * @brief Internal acquisition lifecycle states.
 */
typedef enum {
    MSO_STATE_STOPPED = 0,      /**< System halted; ignores incoming signal triggers. */
    MSO_STATE_ARMED,            /**< Hardware comparator/pin interrupt armed, awaiting trigger event. */
    MSO_STATE_CAPTURING,        /**< Memory acquisition in progress; filling the 256-sample buffer. */
    MSO_STATE_STREAMING         /**< Buffer filled; payload packet is transmitting over UART. */
} MSO_State_t;

/**
 * @brief Horizontal timebase scale settings (Time per Division).
 * @details Assuming standard 10 divisions per screen and a fixed 256-sample frame:
 *          Sample Interval (dt) = (10 * Time_Per_Div) / 256.
 */
typedef enum {
    MSO_TIMEDIV_10US = 0,       /**< 10 us/div: dt ~0.39 us (Fast logic tight-loop / Free-Running ADC). */
    MSO_TIMEDIV_50US,           /**< 50 us/div: dt ~1.95 us. */
    MSO_TIMEDIV_100US,          /**< 100 us/div: dt ~3.9 us. */
    MSO_TIMEDIV_500US,          /**< 500 us/div: dt ~19.5 us. */
    MSO_TIMEDIV_1MS,            /**< 1 ms/div: dt ~39 us. */
    MSO_TIMEDIV_5MS,            /**< 5 ms/div: dt ~195 us. */
    MSO_TIMEDIV_10MS,           /**< 10 ms/div: dt ~390 us. */
    MSO_TIMEDIV_50MS,           /**< 50 ms/div: dt ~1.95 ms. */
    MSO_TIMEDIV_COUNT           /**< Boundary sentry for total number of valid scales. */
} MSO_TimeDiv_t;

/* ========================================================================= */
/*                         Function Prototypes                               */
/* ========================================================================= */

/**
 * @brief  Initializes control flags, state machine variables, and default operational modes.
 *
 * @details Configures the engine to `MSO_STATE_STOPPED`, sets the default mode to
 *          `MSO_MODE_OSCILLOSCOPE`, and initializes the timebase to `MSO_TIMEDIV_1MS`.
 *          Ensures all MCAL triggers (Analog Comparator, ADC, and Timer) remain
 *          quiescent until an explicit RUN opcode is decoded.
 *
 * @note   Must be invoked during firmware startup after MCU clock and DIO
 *         direction registers are configured.
 *
 * @param  None
 * @return None
 */
void MSO_Control_Init(void);

/**
 * @brief  Polls the communication bus for incoming host commands and updates system configuration.
 *
 * @details Non-blocking command parser. Drains the UART RX FIFO and forwards each byte
 *          to MSO_Control_HandleKey(). Opcodes are identical to those listed in
 *          MSO_Control_HandleKey(); any unrecognized byte is safely ignored.
 *          - `'R'` (`0x52`): RUN command -> Transitions state to `MSO_STATE_ARMED`.
 *          - `'S'` (`0x53`): STOP command -> Halts acquisition, transitions to `MSO_STATE_STOPPED`.
 *          - `'O'` (`0x4F`): Selects `MSO_MODE_OSCILLOSCOPE`.
 *          - `'L'` (`0x4C`): Selects `MSO_MODE_LOGIC_ANALYZER`.
 *          - `'0'` to `'7'` (`0x30` - `0x37`): Directly updates the Time/Div scale
 *            matching `MSO_TimeDiv_t` indexes (0 = 10us/div, 1 = 50us/div, ...).
 *          - Any unrecognized opcode is ignored.
 *
 * @param  None
 * @return None
 */
void MSO_Control_ProcessCommand(void);

/**
 * @brief  Dispatches a single decoded host opcode.
 *
 * @details Pure command dispatcher (no UART coupling). ProcessCommand()
 *          wraps this in a UART read loop. Useful for host-side tools
 *          and hostless unit tests that inject bytes directly.
 *          Valid opcodes:
 *          - 'R'/'r': RUN (transition to ARMED)
 *          - 'S'/'s': STOP (halt acquisition)
 *          - 'O'/'o': Select Oscilloscope mode
 *          - 'L'/'l': Select Logic Analyzer mode
 *          - '0'..'7': Set Time/Div scale (enum index)
 *
 * @param[in] cmd Single ASCII opcode from the host.
 * @return None
 */
void MSO_Control_HandleKey(u8 cmd);

/**
 * @brief  Executes the active phase of the capture-and-stream state machine.
 *
 * @details Dispatches tasks according to the active `MSO_State_t`:
 *          - `MSO_STATE_STOPPED`: Keeps capture hardware disarmed; yields execution.
 *          - `MSO_STATE_ARMED`: Configures the sampling timebase (Timer/ADC prescalers)
 *            according to the selected `MSO_TimeDiv_t`, arms the trigger condition
 *            (AC for analog, Pin change for logic), and transitions to `MSO_STATE_CAPTURING`
 *            once triggered.
 *          - `MSO_STATE_CAPTURING`: Samples the input channel at the interval dictated
 *            by the selected Time/Div. OSC: fills a 256-sample buffer; LA: fills a
 *            512-sample buffer.
 *          - `MSO_STATE_STREAMING`: Transmits frame header (`0xAA, 0x55`), the raw
 *            sample payload (256 B for OSC, 512 B for LA), and frame terminator
 *            (`0x0D, 0x0A`). Re-arms immediately for continuous capture.
 *
 * @param  None
 * @return None
 */
void MSO_Control_Update(void);

/**
 * @brief  Sets the horizontal timebase scale manually from software.
 *
 * @details Validates the requested scale parameter against `MSO_TIMEDIV_COUNT`.
 *          If valid, updates internal timebase configuration. If acquisition is
 *          currently active (`MSO_STATE_ARMED` or `MSO_STATE_CAPTURING`), the new scale
 *          takes effect at the beginning of the next frame.
 *
 * @param[in] timeDiv Scale value selected from `MSO_TimeDiv_t`.
 * @return None
 */
void MSO_Control_SetTimeDiv(MSO_TimeDiv_t timeDiv);

/**
 * @brief  Retrieves the active horizontal timebase scale.
 *
 * @param  None
 * @return MSO_TimeDiv_t Currently configured Time/Div enum value.
 */
MSO_TimeDiv_t MSO_Control_GetTimeDiv(void);

/**
 * @brief  Halts acquisition immediately and forces an emergency disarm of all hardware.
 *
 * @details Disables active ADC conversions, masks analog comparator interrupts,
 *          stops sampling timers, and resets the lifecycle state to `MSO_STATE_STOPPED`.
 *
 * @param  None
 * @return None
 */
void MSO_Control_ForceStop(void);

/**
 * @brief  Retrieves the current operational instrument mode.
 *
 * @param  None
 * @return MSO_Mode_t Active instrument mode (`MSO_MODE_IDLE`, `MSO_MODE_OSCILLOSCOPE`,
 *                    or `MSO_MODE_LOGIC_ANALYZER`).
 */
MSO_Mode_t MSO_Control_GetMode(void);

/**
 * @brief  Retrieves the current execution phase of the acquisition engine.
 *
 * @param  None
 * @return MSO_State_t Current state (`MSO_STATE_STOPPED`, `MSO_STATE_ARMED`,
 *                     `MSO_STATE_CAPTURING`, or `MSO_STATE_STREAMING`).
 */
MSO_State_t MSO_Control_GetState(void);

#endif /* MSO_CONTROL_INTERFACE_H */
