#ifndef UART_INTERFACE_H_
#define UART_INTERFACE_H_

#include "../../LIB/STD_TYPES.h"

/* Supported Baud Rates at F_CPU = 16 MHz */
typedef enum {
    UART_BAUD_9600    = 9600UL,
    UART_BAUD_19200   = 19200UL,
    UART_BAUD_38400   = 38400UL,
    UART_BAUD_57600   = 57600UL,
    UART_BAUD_115200  = 115200UL,
    UART_BAUD_230400  = 230400UL,
    UART_BAUD_500000  = 500000UL,
    UART_BAUD_1000000 = 1000000UL
} UART_BaudRate_t;

/* Character Data Frame Size */
typedef enum {
    UART_DATA_5_BITS = 0,
    UART_DATA_6_BITS,
    UART_DATA_7_BITS,
    UART_DATA_8_BITS,
    UART_DATA_9_BITS
} UART_DataSize_t;

/* Parity Mode */
typedef enum {
    UART_PARITY_DISABLED = 0,
    UART_PARITY_EVEN,
    UART_PARITY_ODD
} UART_Parity_t;

/* Stop Bit Selection */
typedef enum {
    UART_STOP_1_BIT = 0,
    UART_STOP_2_BITS
} UART_StopBit_t;

/* Speed Mode */
typedef enum {
    UART_NORMAL_SPEED = 0,
    UART_DOUBLE_SPEED       /* U2X mode (halves prescaler, minimizes baud error) */
} UART_Speed_t;

/* UART Channel Configuration Structure */
typedef struct {
    UART_BaudRate_t baud_rate;
    UART_DataSize_t data_size;
    UART_Parity_t   parity;
    UART_StopBit_t  stop_bits;
    UART_Speed_t    speed;
} UART_Config_t;

/* Standard Initialization and Control APIs */
void UART_Init(const UART_Config_t *config);
void UART_SendByte(u8 data);
u8   UART_ReceiveByte(void);

/* Non-blocking / Polling Check */
u8   UART_IsDataAvailable(void);

/* String & Stream APIs */
void UART_SendString(const char *str);

/* High-Speed Bulk Streaming API for Oscilloscope/Logic Analyzer Buffer Dumps */
void UART_SendBuffer(const u8 *buffer, u16 length);

/* Callback System for Interrupt-driven Mode (Optional) */
void UART_SetRxCallback(void (*callback)(u8 received_byte));

#endif 


