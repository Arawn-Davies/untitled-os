#ifndef _KERNEL_TIMER_H
#define _KERNEL_TIMER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <kernel/types.h>

void init_timer(uint32_t frequency);
uint32_t timer_get_ticks(void);
void ksleep(uint32_t ticks);

#endif