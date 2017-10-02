#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/dwt.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uart.h"

static void gpio_setup(void)
{
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOC);

	gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_50_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
	gpio_set(GPIOC, GPIO13);	/* Switch off LED. */

	gpio_set_mode(GPIOA, GPIO_MODE_INPUT,
		      GPIO_CNF_INPUT_PULL_UPDOWN, GPIO0);
	gpio_set(GPIOA, GPIO0);
}

static void pwm_setup()
{
	rcc_periph_clock_enable(RCC_TIM2);
	rcc_periph_clock_enable(RCC_GPIOA);

	timer_set_mode(TIM2, TIM_CR1_CKD_CK_INT,
		       TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);

	timer_set_prescaler(TIM2, 72 - 1); /* 1MHz */
	timer_enable_preload(TIM2);
	timer_continuous_mode(TIM2);
	timer_set_period(TIM2, 1000000 / 50); /* 50Hz default refresh rate */

	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,
		      GPIO_CNF_OUTPUT_ALTFN_PUSHPULL,
		      GPIO_TIM2_CH2);

	timer_set_oc_mode(TIM2, TIM_OC2, TIM_OCM_PWM1);
	timer_enable_oc_output(TIM2, TIM_OC2);
	timer_enable_counter(TIM2);
}

static void pulse_width(int pulse_width)
{
     timer_set_oc_value(TIM2, TIM_OC2, pulse_width);
}

void delay_ms(int ms)
{
	unsigned deadline = DWT_CYCCNT + ms * 72000;
	while ((int)(deadline - DWT_CYCCNT) > 0);
}

static float test(int servo_zero, int servo_60, int rate)
{
	timer_set_period(TIM2, 1000000 / rate);
	gpio_clear(GPIOC, GPIO13); /* LED on */

	pulse_width(servo_60);

	int start = dwt_read_cycle_counter();
	while (gpio_get(GPIOA, GPIO0));
	int stop = dwt_read_cycle_counter();

 	pulse_width(servo_zero);
	gpio_set(GPIOC, GPIO13); /* LED off */

	float tm = (float)(stop - start) / 72000000;
	delay_ms(2 * tm * 1000);
	return tm;
}

int main(void)
{
	rcc_clock_setup_in_hse_8mhz_out_72mhz();
	dwt_enable_cycle_counter();
	gpio_setup();
	pwm_setup();
	uart1_init();
	setvbuf(stdout, NULL, _IOLBF, 80);

	int servo_0 = 1500,
	   servo_60 = 2100,
	 servo_rate = 333;

	while (1) {
		char buf[80];
		printf("> ");
		fflush(stdout);
		fgets(buf, sizeof(buf), stdin);
		for (char *p = buf; *p; p++)
			if (*p == '\n' || *p == '\r')
				*p = 0;
		int run = 0;

		if (strcmp(buf, "narrow") == 0) {
			servo_0 = 780;
			servo_60 = 400;
			servo_rate = 560;
		} else if (strcmp(buf, "normal") == 0) {
			servo_0 = 1500;
			servo_60 = 2100;
			servo_rate = 333;
		} else {
			int c = 0;
			int new_0 = -1, new_60 = -1;
			c += sscanf(buf, "servo_0 %d", &new_0);
			c += sscanf(buf, "servo_60 %d", &new_60);
			c += sscanf(buf, "rate %d", &servo_rate);
			c += sscanf(buf, "run %d", &run);
			if (c == 0)
				printf("bad command <%s>\n", buf);

			if (new_0 > 0) {
				servo_0 = new_0;
				pulse_width(servo_0);
			}
			if (new_60 > 0) {
				servo_60 = new_60;
				pulse_width(servo_60);
			}
		}

		printf("\n\nrate   - %dHz\n", servo_rate);
		printf("0 deg  - %dms\n60 deg - %dms\n", servo_0, servo_60);

		if (run) {
			pulse_width(servo_0);
			delay_ms(500);

			float avg = 0;
			for (int i = 0; i < run; i++)
				avg += test(servo_0, servo_60, servo_rate);
			avg /= run;
			printf("average speed %.5f\n", avg);
		}
	}
}
