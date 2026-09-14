#ifndef CONTROLLER_PRIVATE_H
#define CONTROLLER_PRIVATE_H
#include "../../LIB/STD_TYPES.h"

typedef union {
    u8 dso_samples[256];
    u8 logic_samples[512];
} CaptureBuffer_t;

#endif
