#ifndef REG_MEMORY_H
#define REG_MEMORY_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define REG_NUMBER 32
#define SPEC_REG_NUMBER 2
#define SPEC_END 1

typedef enum {
    TYPE_INT32,
    TYPE_INT64,
    TYPE_UNS32,
    TYPE_UNS64,
    TYPE_FLOAT32,
    TYPE_FLOAT64,
    TYPE_STRING,
    TYPE_INT8,
    TYPE_INT16,
    TYPE_UNS8,
    TYPE_UNS16,
    TYPE_BOOL
} ValType;

typedef struct {
    ValType type;
    union {
        int32_t int32;
        int64_t int64;
        uint32_t uns32;
        uint64_t uns64;
        float float32;
        double float64;
        char* string;
        int8_t int8;
        int16_t int16;
        uint8_t uns8;
        uint16_t uns16;
        bool boolean;
    } as;
} Val;

// void init_cpu_memory(void);

#endif