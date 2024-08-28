#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <err.h>

#include "fifo.h"

/*
This file is part of 869log (c) 2024 by kittennbfive - https://github.com/kittennbfive

AGPLv3+ and NO WARRANTY!

Please read the fine manual.
*/

static uint8_t * fifo=NULL;
static size_t fifo_nb_entries=0;

static void fifo_cleanup(void)
{
	free(fifo);
}

void fifo_init(void)
{
	fifo=malloc(SZ_FIFO*sizeof(uint8_t));
	if(fifo==NULL)
		err(1, "malloc(fifo) failed");
	
	atexit(&fifo_cleanup);
}

void fifo_push(uint8_t const * const data, const size_t len)
{
	if(fifo_nb_entries+len<SZ_FIFO)
	{
		memcpy(&fifo[fifo_nb_entries], data, len*sizeof(uint8_t));
		fifo_nb_entries+=len;
	}
	else
		errx(1, "FIFO overflow");	
}

bool fifo_pop(uint8_t * const data, const size_t len)
{
	if(len<=fifo_nb_entries)
	{
		memcpy(data, &fifo[0], len*sizeof(uint8_t));
		memmove(&fifo[0], &fifo[len], (fifo_nb_entries-len)*sizeof(uint8_t));
		fifo_nb_entries-=len;
		return true;
	}
	else
		return false; //not enough data
}

bool fifo_peek(uint8_t * const data, const size_t len)
{
	if(len<=fifo_nb_entries)
	{
		memcpy(data, &fifo[0], len*sizeof(uint8_t));
		return true;
	}
	else
		return false; //not enough data
}

void fifo_remove(const size_t len)
{
	if(len<=fifo_nb_entries)
	{
		memmove(&fifo[0], &fifo[len], (fifo_nb_entries-len)*sizeof(uint8_t));
		fifo_nb_entries-=len;
	}
}

size_t fifo_get_nb_entries(void)
{
	return fifo_nb_entries;
}
