/*
 * powerManagement.c
 *
 *  Created on: 6 Jun 2020
 *      Author: Sterna
 *
 *  Handles general power management
 */


/*
*********************************************************************************
*	Include files
*********************************************************************************
*/

#include "powerManagement.h"
#include "APA102Conf.h"

/*
*********************************************************************************
*	Defined constants
*********************************************************************************
*/

#define PWR_MGT_CYCLE_TIME	1000

//Port definitions
#define LED_PWR_PORT			GPIOC
#define LED_PWR_CH1_PIN_NUM		13
#define LED_PWR_CH1_PIN			utilGetGPIOPin(LED_PWR_CH1_PIN_NUM)
#define LED_PWR_CH2_PIN_NUM		14
#define LED_PWR_CH2_PIN			utilGetGPIOPin(LED_PWR_CH2_PIN_NUM)

#define FAN_PWR_PORT			GPIOB
#define FAN_PWR_PIN_NUM			1
#define FAN_PWR_PIN				utilGetGPIOPin(FAN_PWR_PIN_NUM)

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
 * Inits power management
 */
void pwrMgtInit()
{
	//Init fan control
	GPIO_InitTypeDef 		GPIO_InitStruct;
	utilSetClockAFIO(ENABLE);
	// Setup GPIO used for RGB led
	utilSetClockGPIO(FAN_PWR_PORT,ENABLE);
	utilSetClockGPIO(LED_PWR_PORT,ENABLE);

	GPIO_InitStruct.GPIO_Mode	= GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Speed	= GPIO_Speed_2MHz;
	GPIO_InitStruct.GPIO_Pin 	= FAN_PWR_PIN;
	GPIO_Init(FAN_PWR_PORT,&GPIO_InitStruct);
	GPIO_InitStruct.GPIO_Mode	= GPIO_Mode_Out_OD;
	GPIO_InitStruct.GPIO_Pin 	= LED_PWR_CH1_PIN | LED_PWR_CH2_PIN;
	GPIO_Init(LED_PWR_PORT,&GPIO_InitStruct);
	pwrMgtSetFan(true);
	pwrMgtSetLEDPwr(1,true);
	pwrMgtSetLEDPwr(2,true);
}

/*
 * Sets the fan on/off
 */
void pwrMgtSetFan(bool active)
{
	GPIO_WriteBit(FAN_PWR_PORT,FAN_PWR_PIN,active);
}

/*
 * Gets the current fan state
 */
bool pwrMgtGetFanState()
{
	return GPIO_ReadOutputDataBit(FAN_PWR_PORT,FAN_PWR_PIN);
}

/*
 * Toggle the current fan state
 */
void pwrMgtToogleFan()
{
	pwrMgtSetFan(!pwrMgtGetFanState());
}

/*
 * Gets the current state of if a strip has power
 * If LESSEG_ALL is used, it will return true if any strip has power
 */
bool pwrMgtGetLedPwrState(uint8_t strip)
{
	bool tmp=false;
	switch (strip)
	{
		case 1:
		{
			tmp=GPIO_ReadOutputDataBit(LED_PWR_PORT,LED_PWR_CH1_PIN);
		}
		break;
		case 2:
		{
			tmp=GPIO_ReadOutputDataBit(LED_PWR_PORT,LED_PWR_CH1_PIN);
			break;
		}
		case APA_ALL_STRIPS:
		{
			tmp=pwrMgtGetLedPwrState(1) || pwrMgtGetLedPwrState(2);
			break;
		}
	//Note: Channel 3 cannot be turned off, since the logic uses that channel
	}
	return tmp;
}

/*
 * Sets the power to LED channels on/off
 */
void pwrMgtSetLEDPwr(uint8_t strip, bool active)
{
	SPI_TypeDef* tmpSPI=0;
	switch (strip)
	{
		case 1:
		{
			GPIO_WriteBit(LED_PWR_PORT,LED_PWR_CH1_PIN,active);
			tmpSPI=APA_SPI;
		}
		break;
		case 2:
		{
			GPIO_WriteBit(LED_PWR_PORT,LED_PWR_CH2_PIN,active);
			tmpSPI=APA2_SPI;
		break;
		}
		case APA_ALL_STRIPS:
		{
			pwrMgtSetLEDPwr(1,active);
			pwrMgtSetLEDPwr(2,active);
		break;
		}
		//Note: Channel 3 cannot be turned off, since the logic uses that channel
	}
	if(tmpSPI)
	{
		SPI_Cmd(tmpSPI,active);
	}
}

/*
 * The power management task
 * This task keep track of the current voltage level and warns and turns LED strips off if it goes too low
 */
void pwrMgtTask()
{
	static uint32_t nextCallTime=0;
	if(systemTime<nextCallTime)
	{
		return;
	}
	nextCallTime=systemTime+PWR_MGT_CYCLE_TIME;
}


