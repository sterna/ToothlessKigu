/*
 *	extFetCtrl.h
 *
 *	Created on: Jun 22, 2018
 *		Author: Sterna
 */
#ifndef EXTFETCTRL_H_
#define EXTFETCTRL_H_

#include <stdint.h>
#include <stdbool.h>
#include "stm32f10x.h"

#define FET_MAX_PWM	1000

enum
{
	FET1_NUM=1,
	FET2_NUM=2,
	FET3_NUM=3,
	FET_ALL=255
};

void extFetInit(void);
void extFetSetDuty(uint8_t fetNum, uint16_t duty);
void extFetSetState(uint8_t fetNum, FunctionalState st);

#endif /* EXTFETCTRL_H_ */
