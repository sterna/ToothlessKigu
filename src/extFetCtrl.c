/*
 *	extFetCtrl.c
 *
 *	Created on: Jun 22, 2018
 *		Author: Sterna
 *
 *	Handles external MOSFETs
 */

#include "extFetCtrl.h"
#include "stm32f10x.h"
#include "string.h"
#include "utils.h"

/*
*********************************************************************************
*	Defined constants
*********************************************************************************
*/

#define RED_LOW_BYTE 		0
#define RED_HIGH_BYTE 		1
#define GREEN_LOW_BYTE 		2
#define GREEN_HIGH_BYTE 	3
#define BLUE_LOW_BYTE 		4
#define BLUE_HIGH_BYTE 		5

#define PAYLOAD_SIZE		6

#define FET_LP_PIN_NUM	0
#define FET_LP_PIN		utilGetGPIOPin(FET_LP_PIN_NUM)
#define FET_LP_PORT		GPIOA	//This only works if all are on the same port, which they most likely are
#define FET_LP_TIMER	TIM2
#define FET_LP_CHAN		1

#define FET_HP_PIN_NUM	11
#define FET_HP_PIN		utilGetGPIOPin(FET_HP_PIN_NUM)
#define FET_HP_PORT		GPIOA	//This only works if all are on the same port, which they most likely are
#define FET_HP_TIMER	TIM1
#define FET_HP_CHAN		4

/*
*********************************************************************************
*	Type definitions & enums
*********************************************************************************
*/

/*
*********************************************************************************
*	Variables
*********************************************************************************
*/

/*
*********************************************************************************
*	Functions
*********************************************************************************
*/

/*
 *********************************************************************************
 *  External Functions
 *********************************************************************************
 */

/*
 * Inits the control for all the onboard MOSFETs
 */
void extFetInit(void)
{
	//Init LED PWM
	GPIO_InitTypeDef 		GPIO_InitStruct;
	TIM_TimeBaseInitTypeDef TIMBaseInitStruct;
	TIM_OCInitTypeDef 		TIMOCInitStruct;
	uint16_t PrescalerValue = 0;

	SystemCoreClockUpdate();
	PrescalerValue = (uint16_t) (SystemCoreClock / 1000000) - 1; // Timer base frequency is 1MHz
	//Enable all clocks used
	utilSetClockAFIO(ENABLE);
	utilSetClockGPIO(FET_LP_PORT,ENABLE);
	utilSetClockGPIO(FET_HP_PORT,ENABLE);
	utilSetClockTIM(FET_LP_TIMER,ENABLE);
	utilSetClockTIM(FET_HP_TIMER,ENABLE);

	//Setup GPIO
	GPIO_InitStruct.GPIO_Mode	= GPIO_Mode_AF_PP;
	GPIO_InitStruct.GPIO_Speed	= GPIO_Speed_10MHz;
	GPIO_InitStruct.GPIO_Pin 	= FET_LP_PIN;
	GPIO_Init(FET_LP_PORT,&GPIO_InitStruct);

	GPIO_InitStruct.GPIO_Pin 	= FET_HP_PIN;
	GPIO_Init(FET_HP_PORT,&GPIO_InitStruct);

	// Setup timer to 1Khz Prescaler/period (1.000.000 / 1000)
	TIMBaseInitStruct.TIM_Period 		= FET_LP_MAX_PWM;
	TIMBaseInitStruct.TIM_Prescaler 	= PrescalerValue;
	TIMBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;
	TIMBaseInitStruct.TIM_CounterMode 	= TIM_CounterMode_Up;
	TIM_TimeBaseInit(FET_LP_TIMER,&TIMBaseInitStruct);

	TIMBaseInitStruct.TIM_Period 		= FET_HP_MAX_PWM;
	TIM_TimeBaseInit(FET_HP_TIMER,&TIMBaseInitStruct);

	// Same setting for all timer channels
	TIM_OCStructInit(&TIMOCInitStruct);
	TIMOCInitStruct.TIM_OCMode 		= TIM_OCMode_PWM1;
	TIMOCInitStruct.TIM_OutputState = TIM_OutputState_Enable;
	TIMOCInitStruct.TIM_OCPolarity 	= TIM_OCPolarity_High;
	TIMOCInitStruct.TIM_Pulse 		= 0;

	//Inits the timer PWMs (all channels have the same setting)
	utilTIMOCInit(FET_LP_TIMER,FET_LP_CHAN,&TIMOCInitStruct);
	utilTIMOCInit(FET_HP_TIMER,FET_HP_CHAN,&TIMOCInitStruct);

	if(FET_LP_TIMER == TIM1 || FET_LP_TIMER==TIM8)
	{
		TIM_CtrlPWMOutputs(FET_LP_TIMER,ENABLE);
	}
	if(FET_HP_TIMER == TIM1 || FET_HP_TIMER==TIM8)
	{
		TIM_CtrlPWMOutputs(FET_HP_TIMER,ENABLE);
	}

	TIM_Cmd(FET_LP_TIMER,ENABLE);
	TIM_Cmd(FET_HP_TIMER,ENABLE);
}

/*
 * Sets the duty cycle for a MOSFET
 */
void extFetSetDuty(uint8_t fetNum, uint16_t duty)
{
	TIM_TypeDef* tmpTim=0;
	uint8_t tmpChan=0;
	switch(fetNum)
	{
		case FET_LP_NUM:
		{
			if(duty>FET_LP_MAX_PWM)
			{
				duty=FET_LP_MAX_PWM;
			}
			tmpTim=FET_LP_TIMER;
			tmpChan=FET_LP_CHAN;
			break;
		}
		case FET_HP_NUM:
		{
			if(duty>FET_HP_MAX_PWM)
			{
				duty=FET_HP_MAX_PWM;
			}
			tmpTim=FET_HP_TIMER;
			tmpChan=FET_HP_CHAN;
			break;
		}
		default:
			return;
	}
	utilTIMOCWrite(tmpTim,tmpChan,duty);
}

/*
 * Enable/disable the timer for a FET
 */
void extFetSetState(uint8_t fetNum, FunctionalState st)
{
	TIM_TypeDef* tmpTim=0;
	switch(fetNum)
	{
		case FET_LP_NUM:
			tmpTim=FET_LP_TIMER;
		break;
		case FET_HP_NUM:
			tmpTim=FET_HP_TIMER;
		break;
		default:
			return;
	}
	TIM_Cmd(tmpTim,st);
}
