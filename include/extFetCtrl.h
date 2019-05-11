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

#define FET_LP_MAX_PWM	1000
#define FET_HP_MAX_PWM	1000

enum
{
	FET_LP_NUM=1,
	FET_HP_NUM=2,
	FET_ALL=255
};

void extFetInit(void);
void extFetSetDuty(uint8_t fetNum, uint16_t duty);
void extFetSetState(uint8_t fetNum, FunctionalState st);

#endif /* EXTFETCTRL_H_ */
