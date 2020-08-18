#ifndef _KERNEL_TIMER_H
#define _KERNEL_TIMER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <kernel/types.h>

void init_timer(uint32_t frequency);

#endif