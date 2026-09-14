#include "../../LIB/STD_TYPES.h"
#include "../../LIB/BIT_MATH.h"

#include "UART_Interface.h"
#include "UART_Private.h"
static void (*UART_RxCallback)(u8) = Null;

u8 checkconfig(const UART_Config_t *config)
{
    if (config == Null)
    {
        return 0; // Null pointer, invalid configuration
    }

    // Check for valid baud rate
    if (config->baud_rate < UART_BAUD_9600 || config->baud_rate > UART_BAUD_1000000)
    {
        return 0; // Invalid baud rate
    }

    // Check for valid data size
    if (config->data_size < UART_DATA_5_BITS || config->data_size > UART_DATA_9_BITS)
    {
        return 0; // Invalid data size
    }

    // Check for valid parity mode
    if (config->parity < UART_PARITY_DISABLED || config->parity > UART_PARITY_ODD)
    {
        return 0; // Invalid parity mode
    }

    // Check for valid stop bits
    if (config->stop_bits < UART_STOP_1_BIT || config->stop_bits > UART_STOP_2_BITS)
    {
        return 0; // Invalid stop bits
    }

    // Check for valid speed mode
    if (config->speed < UART_NORMAL_SPEED || config->speed > UART_DOUBLE_SPEED)
    {
        return 0; // Invalid speed mode
    }

    return 1; // Configuration is valid
}

void UART_Init(const UART_Config_t *config)
{
    // check for null pointer
   if (!checkconfig(config)) {return;}
    // set baud rate
    u32 divisor = (config->speed == UART_DOUBLE_SPEED) ? 8UL : 16UL;
    u16 UBRR_Value = (u16)((F_CPU / (divisor * (u32)config->baud_rate)) - 1UL);
    UBRRL_Reg = (u8)(UBRR_Value & 0xFF); // Set lower byte of UBRR
    UBRRH_Reg = (u8)((UBRR_Value >> 8) & 0xFF); // Set upper byte of UBRR
    // set data size
    switch (config->data_size)
    {
        case UART_DATA_5_BITS:
        case UART_DATA_6_BITS:
        case UART_DATA_7_BITS:
        case UART_DATA_8_BITS:
            CLR_BIT(UCSRnB_Reg, UCSZ2);
            break;
        case UART_DATA_9_BITS:
            break;
    }
    switch (config->data_size)
    {
        //5 bits
        case UART_DATA_5_BITS:
            CLR_BIT(UCSRnC_Reg, UCSZ1);
            CLR_BIT(UCSRnC_Reg, UCSZ0);
            break;
        //6 bits
        case UART_DATA_6_BITS:
            CLR_BIT(UCSRnC_Reg, UCSZ1);
            SET_BIT(UCSRnC_Reg, UCSZ0);
            break;
        case UART_DATA_7_BITS:
            SET_BIT(UCSRnC_Reg, UCSZ1);
            CLR_BIT(UCSRnC_Reg, UCSZ0);
            break;
        case UART_DATA_8_BITS:
            SET_BIT(UCSRnC_Reg, UCSZ1);
            SET_BIT(UCSRnC_Reg, UCSZ0);
            break;
        case UART_DATA_9_BITS:
            SET_BIT(UCSRnC_Reg, UCSZ1);
            SET_BIT(UCSRnC_Reg, UCSZ0);
            SET_BIT(UCSRnB_Reg, UCSZ2);
            break;
    }
   // set parity mode
    switch (config->parity)
    {
        case UART_PARITY_DISABLED:
            CLR_BIT(UCSRnC_Reg, UPM1);
            CLR_BIT(UCSRnC_Reg, UPM0);
            break;
        case UART_PARITY_EVEN:
            SET_BIT(UCSRnC_Reg, UPM1);
            CLR_BIT(UCSRnC_Reg, UPM0);
            break;
        case UART_PARITY_ODD:
            SET_BIT(UCSRnC_Reg, UPM1);
            SET_BIT(UCSRnC_Reg, UPM0);
            break;
    }
    // set stop bits
    switch (config->stop_bits)
    {
        case UART_STOP_1_BIT:
            CLR_BIT(UCSRnC_Reg, USBS);
            break;
        case UART_STOP_2_BITS:
            SET_BIT(UCSRnC_Reg, USBS);
            break;
    }
    // set speed mode
    switch (config->speed)
    {
        case UART_NORMAL_SPEED:
            CLR_BIT(UCSRnA_Reg, U2X);
            break;
        case UART_DOUBLE_SPEED:
            SET_BIT(UCSRnA_Reg, U2X);
            break;
    }
   // make asynchronous mode
   CLR_BIT(UCSRnC_Reg, UMSEL);

  /* [4] Enable Receiver & Transmitter (written last, after frame format) */
    SET_BIT(UCSRnB_Reg, RXEN);
    SET_BIT(UCSRnB_Reg, TXEN);
}

void UART_SendByte(u8 data)
{
    /* [1] Wait for empty transmit buffer */
    while (!GET_BIT(UCSRnA_Reg, UDRE)) ;
    /* [2] Put data into buffer, sends the data */
    UDR_Reg = data;
}
u8   UART_ReceiveByte(void)
{
    /* [1] Wait for data to be received */
    while (!GET_BIT(UCSRnA_Reg, RXC))
        ;
    /* [2] Get and return received data from buffer */
    return UDR_Reg;
}
u8   UART_IsDataAvailable(void)
{
    return GET_BIT(UCSRnA_Reg, RXC);
}
void UART_SendString(const char *str)
{
    // check for null pointer
    if (str == Null) { return; }
    while (*str != '\0')
    {
        UART_SendByte((u8)(*str));
        str++;
    }
}
void UART_SendBuffer(const u8 *buffer, u16 length)
{
    // check for null pointer
    if (buffer == Null) { return; }
    for (u16 i = 0; i < length; i++)
    {
        UART_SendByte(buffer[i]);
    }
}
void UART_SetRxCallback(void (*callback)(u8 received_byte))
{
    // check for null pointer
    if (callback == Null) { return; }
    // Enable RX Complete Interrupt
    SET_BIT(UCSRnB_Reg, RXCIE);
    // Set the callback function pointer
    UART_RxCallback = callback;
}
void __vector_18(void) __attribute__((signal, used));
void __vector_18(void)
{
    if (UART_RxCallback != Null)
    {
        u8 received_byte = UDR_Reg; // Read the received byte from the UART data register
        UART_RxCallback(received_byte); // Call the user-defined callback function
    }

}
