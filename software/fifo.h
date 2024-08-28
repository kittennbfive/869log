#ifndef __FIFO_H__
#define __FIFO_H__
#include <stdint.h>
#include <stdbool.h>

/*
This file is part of 869log (c) 2024 by kittennbfive - https://github.com/kittennbfive

AGPLv3+ and NO WARRANTY!

Please read the fine manual.
*/

#define SZ_FIFO 256

void fifo_init(void);
void fifo_push(uint8_t const * const data, const size_t len);
bool fifo_pop(uint8_t * const data, const size_t len);
bool fifo_peek(uint8_t * const data, const size_t len);
void fifo_remove(const size_t len);
size_t fifo_get_nb_entries(void);

#endif
