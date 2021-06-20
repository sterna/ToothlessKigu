/*
 *	sw.h
 *
 *	Created on: 5 okt 2014
 *		Author: Sterna
 */
#ifndef SW_H_
#define SW_H_

#include "stm32f10x.h"
#include <stdint.h>
#include <stdbool.h>

//Used for switch on discovery board. Not used here
#define SW (GPIOA->IDR & (1<<0))

#define SW1_4_PORT			GPIOB
#define SW1_4_PORT_REG 		SW1_4_PORT->IDR

/*
 * Switches are on
 * SW1 - PB15
 * SW2 - PB10
 * SW3 - A3
 * SW4 - C13
 */


#define SW1_PIN			15
#define SW1_PORT 		GPIOB
#define SW1_PORT_REG	SW1_PORT->IDR

#define SW2_PIN			10
#define SW2_PORT 		GPIOB
#define SW2_PORT_REG	SW2_PORT->IDR

#define SW3_PIN			3
#define SW3_PORT 		GPIOA
#define SW3_PORT_REG	SW3_PORT->IDR

#define SW4_PIN			14
#define SW4_PORT 		GPIOC
#define SW4_PORT_REG	SW4_PORT->IDR

#define SW1 (!(SW1_PORT_REG &(1<<SW1_PIN)))
#define SW2 (!(SW2_PORT_REG &(1<<SW2_PIN)))
#define SW3 (!(SW3_PORT_REG &(1<<SW3_PIN)))
#define SW4 (!(SW4_PORT_REG &(1<<SW4_PIN)))

#define SW_NOF_SWITCHES	4

//The interval at which the debounce function is run (in ms)
#define SW_DEBOUNCE_PERIOD 33
#define SW_DEBOUNCE_COUNTER_TOP 3

void swInit();
void swDebounceTask();

bool swGetState(uint8_t sw);
bool swGetRisingEdge(uint8_t sw);
bool swGetFallingEdge(uint8_t sw);
bool swGetActiveForMoreThan(uint8_t sw, uint32_t ms);

#endif /* SW_H_ */
