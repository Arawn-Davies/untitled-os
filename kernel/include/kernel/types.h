#ifndef _KERNEL_TYPES_H_
#define _KERNEL_TYPES_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef char* string;

typedef struct {
	int32_t X, Y;
} Point;

typedef struct {
	uint32_t X, Y;
} UPoint;

#endif
