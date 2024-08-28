#define _GNU_SOURCE //pipe2()
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <err.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/wait.h>
#include <getopt.h>
#include <poll.h>

#include "main.h"

#include "intercom.h"
#include "uart_worker.h"
#include "interface.h"

/*
This file is part of 869log (c) 2024 by kittennbfive - https://github.com/kittennbfive

AGPLv3+ and NO WARRANTY!

Please read the fine manual.
*/

static void configure_port(int uart)
{
	struct termios tty;
	
	if(tcgetattr(uart, &tty)<0)
		err(1, "configure_port: tcgetattr() failed");
	
	cfmakeraw(&tty);
	tty.c_cc[VMIN]=0;
	tty.c_cc[VTIME]=0;
	
	if(cfsetospeed(&tty, B115200)<0)
		err(1, "configure_port: cfsetospeed() failed");
	
	if(tcsetattr(uart, 0, &tty)<0)
		err(1, "configure_port: tcsetattr() failed");
	
	if(tcflush(uart, TCIOFLUSH)<0)
		err(1, "configure_port: tcflush() failed");
}

void print_usage_and_exit(void)
{
	fprintf(stderr, "usage: 869log [--port $serial_port] [--once|--twice] [--relative-time] [--output-file $file]\n\t--port $serial_port tells the software to use $serial_port instead of /dev/ttyUSB0 by default\n\t--once instructs the AVR to request 1 reading per second\n\t--twice is the same but 2 readings per second\n\tBy default the tool shows as many readings as the meter can return.\n\t--relative-time does not show current date/time but a clock starting at 00:00:00 once data is received\n\t--output-file $file writes received and decoded data to $file in addition to showing it on the screen\n\n");
	exit(0);
}

int main(int argc, char * argv[])
{
	const struct option optiontable[]=
	{
		{ "port",			required_argument,	NULL,	0 },
		{ "once",			no_argument,		NULL,	1 },
		{ "twice",	 		no_argument,		NULL,	2 },
		{ "relative-time",	no_argument,		NULL,	3 },
		{ "output-file",	required_argument,	NULL,	4 },
		
		{ "version",		no_argument,		NULL, 	100 },
		{ "help",			no_argument,		NULL, 	101 },
		{ "usage",			no_argument,		NULL, 	101 },
		
		{ NULL, 0, NULL, 0 }
	};

	int optionindex;
	int opt;
	
	char filename[SZ_OUT_FILE_MAX+1]={'\0'};
	
	config_t config;
	strncpy(config.portname, "/dev/ttyUSB0", SZ_PORT_NAME_MAX); config.portname[SZ_PORT_NAME_MAX]='\0';
	config.outfile=NULL;
	config.sample_mode=MODE_CONTINUOUS;
	config.relative_time=false;
	config.sampling_started=false;
	
	bool only_print_version=false;
	
	fprintf(stderr, "This is 869log version %s\n", VERSION_STR);
	fprintf(stderr, "(c) 2024 by kittennbfive - https://github.com/kittennbfive/\n");
	fprintf(stderr, "This tool is provided under AGPLv3+ and WITHOUT ANY WARRANTY!\n\n");
	
	while((opt=getopt_long(argc, argv, "", optiontable, &optionindex))!=-1)
	{
		switch(opt)
		{
			case '?': print_usage_and_exit(); break;
			
			case 0: strncpy(config.portname, optarg, SZ_PORT_NAME_MAX); config.portname[SZ_PORT_NAME_MAX]='\0'; break;
			case 1: config.sample_mode=MODE_ONCE_A_SEC; break;
			case 2: config.sample_mode=MODE_TWICE_A_SEC; break;
			case 3: config.relative_time=true; break;
			case 4: strncpy(filename, optarg, SZ_OUT_FILE_MAX); filename[SZ_OUT_FILE_MAX]='\0'; break;
			
			case 100: only_print_version=true; break;
			case 101: print_usage_and_exit(); break;
			
			default: errx(1, "don't know how to handle value %d returned by getopt_long - this is a bug", opt); break;
		}
	}
	
	if(only_print_version)
		return 0;
	
	
	if(strlen(filename))
	{
		config.outfile=fopen(filename, "w");
		if(!config.outfile)
			err(1, "opening output file \"%s\" failed", filename);
		fprintf(stderr, "writing data to file \"%s\"\n\n", filename);
	}
	
	
	int uart=open(config.portname, O_RDWR);
	if(uart<0)
		err(1, "can't open serial port %s", config.portname);
	
	configure_port(uart);
	
	
	//set sample mode in firmware 
	char command;
	
	switch(config.sample_mode)
	{
		case MODE_CONTINUOUS: command='c'; break;
		case MODE_ONCE_A_SEC: command='o'; break;
		case MODE_TWICE_A_SEC: command='t'; break;
	}
	
	if(write(uart, &command, sizeof(uint8_t))<0)
		err(1, "write() for mode setting failed");
	
	fprintf(stderr, "mode set - waiting for data...\n");
	
	
	int pipe_interface_to_uart_worker[2];
	int pipe_uart_worker_to_interface[2];
	
	if(pipe2(pipe_interface_to_uart_worker, O_DIRECT|O_NONBLOCK))
		err(1, "pipe2(pipe_interface_to_uart_worker) failed");
	
	if(pipe2(pipe_uart_worker_to_interface, O_DIRECT|O_NONBLOCK))
		err(1, "pipe2(pipe_uart_worker_to_interface) failed");
	
	pid_t pid=fork();
	
	if(pid==-1)
		err(1, "fork() failed");
	
	if(pid==0) //child
	{
		close(pipe_interface_to_uart_worker[PIPE_WRITE_END]);
		close(pipe_uart_worker_to_interface[PIPE_READ_END]);
		
		uart_worker(pipe_interface_to_uart_worker[PIPE_READ_END], pipe_uart_worker_to_interface[PIPE_WRITE_END], uart, &config);
		
		close(pipe_interface_to_uart_worker[PIPE_READ_END]);
		close(pipe_uart_worker_to_interface[PIPE_WRITE_END]);
	}
	else //parent
	{
		close(pipe_interface_to_uart_worker[PIPE_READ_END]);
		close(pipe_uart_worker_to_interface[PIPE_WRITE_END]);
		
		interface(pipe_uart_worker_to_interface[PIPE_READ_END], pipe_interface_to_uart_worker[PIPE_WRITE_END], &config);
		
		close(pipe_interface_to_uart_worker[PIPE_WRITE_END]);
		close(pipe_uart_worker_to_interface[PIPE_READ_END]);
		
		wait(NULL); //wait for childs
	}
	
	close(uart);
	
	return 0;
}
