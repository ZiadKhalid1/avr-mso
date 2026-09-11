/* DIO_Interface.h */
#ifndef DIO_INTERFACE_H_
#define DIO_INTERFACE_H_

#include "../../LIB/STD_TYPES.h"
#include "../../LIB/BIT_MATH.h"

typedef enum {
    DIO_PORTB = 0,
    DIO_PORTC,
    DIO_PORTD
} DIO_Port_t;

typedef enum {
    DIO_PIN0 = 0U,DIO_PIN1, DIO_PIN2, DIO_PIN3,
    DIO_PIN4,DIO_PIN5, DIO_PIN6, DIO_PIN7
} DIO_Pin_t;

typedef enum {
    DIO_DIRECTION_INPUT = 0,
    DIO_DIRECTION_OUTPUT
} DIO_Direction_t;

typedef enum {
    DIO_PULLUP_DISABLE = 0,
    DIO_PULLUP_ENABLE
} DIO_Pullup_t;

typedef enum {
    DIO_PIN_LOW = 0,
    DIO_PIN_HIGH
} DIO_Level_t;

/* Pin-level APIs */
void        DIO_SetPinDirection(DIO_Port_t port, DIO_Pin_t pin, DIO_Direction_t direction);
void        DIO_SetPinPullup(DIO_Port_t port, DIO_Pin_t pin, DIO_Pullup_t pullup);
void        DIO_SetPinValue(DIO_Port_t port, DIO_Pin_t pin, DIO_Level_t value);
DIO_Level_t DIO_GetPinValue(DIO_Port_t port, DIO_Pin_t pin);
void        DIO_TogglePinValue(DIO_Port_t port, DIO_Pin_t pin);

/* Port-level Fast APIs for Logic Analyzer Engine */
void        DIO_SetPortDirection(DIO_Port_t port, u8 direction_mask);
void        DIO_SetPortPullup(DIO_Port_t port, u8 pullup_mask);
u8     DIO_ReadPort(DIO_Port_t port);

#endif /* DIO_INTERFACE_H_ */
