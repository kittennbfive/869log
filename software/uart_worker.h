#ifndef __UART_WORKER_H__
#define __UART_WORKER_H__

#include "main.h"

/*
This file is part of 869log (c) 2024 by kittennbfive - https://github.com/kittennbfive

AGPLv3+ and NO WARRANTY!

Please read the fine manual.
*/

#define SIZE_BUFFER_RX 256

void uart_worker(int pipe_read, int pipe_write, int uart, config_t * const config);

#endif
