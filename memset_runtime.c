/* Freestanding ABI entry point for compiler-emitted memset calls. */

#include "stddef.h"

void* memset(void* destination, int value, size_t size) {
    volatile unsigned char* output = (volatile unsigned char*)destination;
    unsigned char byte = (unsigned char)value;

    while (size != 0u) {
        *output++ = byte;
        --size;
    }
    return destination;
}
