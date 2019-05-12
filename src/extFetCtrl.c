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

#define FET1_PIN_NUM	7
#define FET1_PIN		utilGetGPIOPin(FET1_PIN_NUM)
#define FET1_PORT		GPIOA	//This only works if all are on the same port, which they most likely are
#define FET1_TIMER		TIM3
#define FET1_CHAN		2

#define FET2_PIN_NUM	0
#define FET2_PIN		utilGetGPIOPin(FET2_PIN_NUM)
#define FET2_PORT		GPIOB	//This only works if all are on the same port, which they most likely are
#define FET2_TIMER		TIM3
#define FET2_CHAN		3

#define FET3_PIN_NUM	1
#define FET3_PIN		utilGetGPIOPin(FET3_PIN_NUM)
#define FET3_PORT		GPIOB	//This only works if all are on the same port, which they most likely are
#define FET3_TIMER		TIM3
#define FET3_CHAN		4

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
	utilSetClockGPIO(FET1_PORT,ENABLE);
	utilSetClockGPIO(FET2_PORT,ENABLE);
	utilSetClockGPIO(FET3_PORT,ENABLE);
	utilSetClockTIM(FET1_TIMER,ENABLE);
	utilSetClockTIM(FET2_TIMER,ENABLE);
	utilSetClockTIM(FET3_TIMER,ENABLE);

	//Setup GPIO
	GPIO_InitStruct.GPIO_Mode	= GPIO_Mode_AF_PP;
	GPIO_InitStruct.GPIO_Speed	= GPIO_Speed_10MHz;
	GPIO_InitStruct.GPIO_Pin 	= FET1_PIN;
	GPIO_Init(FET1_PORT,&GPIO_InitStruct);
	GPIO_InitStruct.GPIO_Pin 	= FET2_PIN;
	GPIO_Init(FET2_PORT,&GPIO_InitStruct);
	GPIO_InitStruct.GPIO_Pin 	= FET3_PIN;
	GPIO_Init(FET3_PORT,&GPIO_InitStruct);

	// Setup timer to 1Khz Prescaler/period (1.000.000 / 1000)
	TIMBaseInitStruct.TIM_Period 		= FET_MAX_PWM;
	TIMBaseInitStruct.TIM_Prescaler 	= PrescalerValue;
	TIMBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;
	TIMBaseInitStruct.TIM_CounterMode 	= TIM_CounterMode_Up;
	TIM_TimeBaseInit(FET1_TIMER,&TIMBaseInitStruct);

	TIMBaseInitStruct.TIM_Period 		= FET_MAX_PWM;
	TIM_TimeBaseInit(FET2_TIMER,&TIMBaseInitStruct);

	TIMBaseInitStruct.TIM_Period 		= FET_MAX_PWM;
	TIM_TimeBaseInit(FET3_TIMER,&TIMBaseInitStruct);

	// Same setting for all timer channels
	TIM_OCStructInit(&TIMOCInitStruct);
	TIMOCInitStruct.TIM_OCMode 		= TIM_OCMode_PWM1;
	TIMOCInitStruct.TIM_OutputState = TIM_OutputState_Enable;
	TIMOCInitStruct.TIM_OCPolarity 	= TIM_OCPolarity_High;
	TIMOCInitStruct.TIM_Pulse 		= 0;

	//Inits the timer PWMs (all channels have the same setting)
	utilTIMOCInit(FET1_TIMER,FET1_CHAN,&TIMOCInitStruct);
	utilTIMOCInit(FET2_TIMER,FET2_CHAN,&TIMOCInitStruct);
	utilTIMOCInit(FET3_TIMER,FET3_CHAN,&TIMOCInitStruct);


	if(FET1_TIMER == TIM1 || FET1_TIMER==TIM8)
	{
		TIM_CtrlPWMOutputs(FET1_TIMER,ENABLE);
	}
	if(FET2_TIMER == TIM1 || FET2_TIMER==TIM8)
	{
		TIM_CtrlPWMOutputs(FET2_TIMER,ENABLE);
	}
	if(FET3_TIMER == TIM1 || FET3_TIMER==TIM8)
	{
		TIM_CtrlPWMOutputs(FET3_TIMER,ENABLE);
	}

	TIM_Cmd(FET1_TIMER,ENABLE);
	TIM_Cmd(FET2_TIMER,ENABLE);
	TIM_Cmd(FET3_TIMER,ENABLE);

}

/*
 * Sets the duty cycle for a MOSFET
 */
void extFetSetDuty(uint8_t fetNum, uint16_t duty)
{
	TIM_TypeDef* tmpTim=0;
	uint8_t tmpChan=0;
	if(duty>FET_MAX_PWM)
	{
		duty=FET_MAX_PWM;
	}
	switch(fetNum)
	{
		case FET1_NUM:
		{
			tmpTim=FET1_TIMER;
			tmpChan=FET1_CHAN;
			break;
		}
		case FET2_NUM:
		{
			tmpTim=FET2_TIMER;
			tmpChan=FET2_CHAN;
			break;
		}
		case FET3_NUM:
		{
			tmpTim=FET3_TIMER;
			tmpChan=FET3_CHAN;
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
		case FET1_NUM:
			tmpTim=FET1_TIMER;
		break;
		case FET2_NUM:
			tmpTim=FET2_TIMER;
		break;
		case FET3_NUM:
			tmpTim=FET3_TIMER;
			break;
		default:
			return;
	}
	TIM_Cmd(tmpTim,st);
}
