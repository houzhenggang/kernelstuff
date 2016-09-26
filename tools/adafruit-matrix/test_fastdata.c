#include <bcm2835.h>
#include <linux/errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <time.h>

#include "io.h"

#define GPIO_R1		11
#define GPIO_R2		8
#define GPIO_G1		27
#define GPIO_G2		9
#define GPIO_B1		7
#define GPIO_B2		10
#define GPIO_A		22
#define GPIO_B		23
#define GPIO_C		24
#define GPIO_D		25
#define GPIO_E		15
#define GPIO_OE		18
#define GPIO_STR	4
#define GPIO_CLK	17

#define ROWS 16
#define COLUMNS 32
#define PWM_BITS 8

#define GPIO_CLOCK_MASK (1 << 17)

#define GPIO_ADDR_OFFSET 22
#define GPIO_HI_ADDR_MASK 0b0011110000000000000000000000
#define GPIO_ADDR_MASK 0b0011110000001000000000000000

#define GPIO_DATA_OFFSET 0
#define GPIO_DATA_MASK 0b1000000000000000111110000000

typedef struct panel_io {
	uint32_t GPIO0	: 1;
	uint32_t GPIO1	: 1;
	uint32_t GPIO2	: 1;
	uint32_t GPIO3	: 1;
	uint32_t STR	: 1;
	uint32_t GPIO5	: 1;
	uint32_t GPIO6	: 1;
	uint32_t B1		: 1;
	uint32_t R2		: 1;
	uint32_t G2		: 1;
	uint32_t B2		: 1;
	uint32_t R1		: 1;
	uint32_t GPIO12 : 1;
	uint32_t GPIO13 : 1;
	uint32_t GPIO14 : 1;
	uint32_t E		: 1;
	uint32_t GPIO16 : 1;
	uint32_t CLK	: 1;
	uint32_t OE		: 1;
	uint32_t GPIO19 : 1;
	uint32_t GPIO20 : 1;
	uint32_t GPIO21 : 1;
	uint32_t A		: 1;
	uint32_t B		: 1;
	uint32_t C		: 1;
	uint32_t D		: 1;
	uint32_t GPIO26 : 1;
	uint32_t G1		: 1;
};

int run = 1;
int numchld = 0;
int address = 0;

long updates = 0;

#define LL_IO

void llgpio_setup()
{
	gpio_set_outputs((1 << GPIO_R1) | (1 << GPIO_R2) | (1 << GPIO_G1) | (1 << GPIO_G2) | (1 << GPIO_B1) | (1 << GPIO_B2) | (1 << GPIO_A) | (1 << GPIO_B) | (1 << GPIO_C) | (1 << GPIO_D) | (1 << GPIO_E) | (1 << GPIO_OE) | (1 << GPIO_STR) | (1 << GPIO_CLK));	
}

#define GPIO_HI(gpio) gpio_set_bits((1 << gpio))
#define GPIO_LO(gpio) gpio_clr_bits((1 << gpio))
#define GPIO_SET(gpio, state) (state ? GPIO_HI(gpio) : GPIO_LO(gpio))

void clock_out(struct panel_io* data, int length)
{
	GPIO_LO(GPIO_STR);
	int i;
	uint32_t clock = 0;
	for(i = 0; i < length; i++)
	{
		gpio_write_bits(((uint32_t*)data)[i] | (clock << GPIO_CLK));
		clock ^= 1;
//		GPIO_HI(GPIO_CLK);
	}
	GPIO_HI(GPIO_STR);
}

void set_address(int addr)
{
	struct panel_io data;
	*((uint32_t*)(&data)) |= (addr << GPIO_ADDR_OFFSET) & GPIO_HI_ADDR_MASK;
	data.E = addr >> 4;
	gpio_write_masked_bits(*((uint32_t*)&data), GPIO_ADDR_MASK);	
}

int fork_child()
{
	int ret;
	ret = fork();
	if(ret == 0)
	{
		sleep(1);
		exit(0);
	}
	if(ret < 0)
	{
		run = 0;
		perror("Failed to fork");
	}
	else
		numchld++;
	return ret;
}

void signalhandler(int sig)
{
	printf("Signal: %d\n", sig);
	if(sig == SIGINT)
		run = 0;
	else if(sig == SIGCHLD)
	{
		numchld--;
		address++;
		address %= ROWS;
		printf("%ld updates per second\n", updates);
		updates = 0;
		if(run)
		{
			fork_child();
		}
	}
}

#define UCP unsigned char*

#define U16P uint16_t*

void prerender_frame(struct panel_io* rowdata, U16P red, U16P green, U16P blue, int bits, int rows, int columns)
{
	int i;
	int j;
	int k;
	int pwm_steps = 1 << bits;
	struct panel_io row[columns];
	for(i = 0; i < rows / 2; i++)
	{
		int row1_base = i * columns;
		int row2_base = (rows / 2 + i) * columns;
		for(j = 0; j < pwm_steps; j++)
		{
			memset(row, 0, columns * sizeof(struct panel_io));
			for(k = 0; k < columns; k++)
			{
				if(i == 0)
					printf("%d: %d:%d:%d Offset: %d\n ", j, red[row1_base + k], green[row1_base + k], blue[row1_base + k], row1_base + k);
				if(red[row1_base + k] > j)
				{
					row[k].R1 = 1;
				}
				if(green[row1_base + k] > j)
				{
					row[k].G1 = 1;
				}
				if(blue[row1_base + k] > j)
				{
					row[k].B1 = 1;
				}
				if(red[row2_base + k] > j)
				{
					row[k].R2 = 1;
				}
				if(green[row2_base + k] > j)
				{
					row[k].G2 = 1;
				}
				if(blue[row2_base + k] > j)
				{
					row[k].B2 = 1;
				}			
				*((uint32_t*)(&row[k])) |= (i << GPIO_ADDR_OFFSET) & GPIO_HI_ADDR_MASK;
				row[k].E = i >> 4;
			}
			memcpy(rowdata + i * pwm_steps * columns + j * columns, row, columns * sizeof(struct panel_io));
		}
	}
}

void show_frame(struct panel_io* frame, int bits, int rows, int columns)
{
	int i;
	int j;
	int pwm_steps = (1 << bits);
	for(i = 0; i < rows / 2; i++)
	{
		GPIO_HI(GPIO_OE);
		set_address(i);
		GPIO_LO(GPIO_OE);
		for(j = 0; j < pwm_steps; j++)
		{
			clock_out(frame + i * pwm_steps * columns + j * columns, columns);
		}
	}
}

int main(int argc, char** argv)
{
	if(llgpio_init() != 0)
		return -EPERM;
	llgpio_setup();

	GPIO_LO(GPIO_OE);
	
	int len = ROWS * COLUMNS;

	uint16_t data_red[len];
	uint16_t data_green[len];
	uint16_t data_blue[len];


	float max_distance = sqrt(pow(ROWS, 2) + pow(COLUMNS, 2));
	int i;
	int j;
	int pwm_max = (1 << PWM_BITS) - 1;
	for(i = 0; i < ROWS; i++)
	{
		for(j = 0; j < COLUMNS; j++)
		{
			float distance = sqrt(pow(i, 2) + pow(j, 2));
			data_red[i * COLUMNS + j] = 0;//(uint16_t)(pwm_max * (distance / max_distance));
			data_green[i * COLUMNS + j] = 0;//(uint16_t)(pwm_max * (distance / max_distance));
			data_blue[i * COLUMNS + j] = 0;//(uint16_t)(pwm_max * (distance / max_distance));
		}
	}
	for(j = 0; j < COLUMNS; j++)
	{	
		data_red[3 * COLUMNS + j] = 65535;
		data_green[3 * COLUMNS + j] = 65535;
		data_blue[3 * COLUMNS + j] = 65535;
	}
	for(j = 0; j < COLUMNS; j++)
	{	
		data_red[11 * COLUMNS + j] = 65535;
		data_green[11 * COLUMNS + j] = 65535;
		data_blue[11 * COLUMNS + j] = 65535;
	}
	int rowdata_len = ROWS / 2 * COLUMNS * (1 << PWM_BITS);

	struct panel_io rowdata[rowdata_len];

	prerender_frame(rowdata, data_red, data_green, data_blue, PWM_BITS, ROWS, COLUMNS);

	for(i = 0; i < rowdata_len; i++)
	{
		struct panel_io cio = rowdata[i];
		if(cio.R1 || cio.G1 || cio.B1)
			printf("%d: Row select 1\n", i);
		if(cio.R2 || cio.G2 || cio.B2)
			printf("%d: Row select 2\n", i);
	}

	signal(SIGINT, signalhandler);
	signal(SIGCHLD, signalhandler);

	fork_child();

	while(run)
	{
		show_frame(rowdata, PWM_BITS, ROWS, COLUMNS);
		updates++;
	}

	while(numchld > 0)
		sleep(1);

	struct panel_io data[COLUMNS];
	memset(data, 0, COLUMNS * sizeof(struct panel_io));
	for(i = 0; i < ROWS / 2; i++)
	{
		*((uint32_t*)(&data[i])) |= (i << GPIO_ADDR_OFFSET) & GPIO_HI_ADDR_MASK;
		data[i].E = i >> 4;
		clock_out(data, COLUMNS);
	}
	return 0;
}