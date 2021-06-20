/*
 *	sw.c
 *
 *	Created on: 5 okt 2014
 *		Author: Sterna
 */

#include "sw.h"
#include "time.h"
#include "utils.h"

static bool getSwRaw(uint8_t sw);

typedef struct
{
	uint8_t counter;				//Counter to keep track of debouncing
	bool active;					//Indicates that the switch is currently active, and properly debounced
	bool fallingEdgeActive;			//Indicates that a falling edge has happened. Will be reset once read.
	bool risingEdgeActive;			//Indicates that a rising edge has happened. Will be reset once read.
	uint32_t activationStartTime;	//Records the systemTime at which the switch had a rising edge
}swState_t;

static swState_t switches[SW_NOF_SWITCHES];

/*
 * Inits the ports for the switches
 */
void swInit()
{
	GPIO_InitTypeDef GPIOInitStruct;

	utilSetClockGPIO(SW1_PORT,ENABLE);
	utilSetClockGPIO(SW2_PORT,ENABLE);
	utilSetClockGPIO(SW3_PORT,ENABLE);
	utilSetClockGPIO(SW4_PORT,ENABLE);

	GPIOInitStruct.GPIO_Mode = GPIO_Mode_IPU;
	GPIOInitStruct.GPIO_Speed = GPIO_Speed_2MHz;
	GPIOInitStruct.GPIO_Pin =  (1<<SW1_PIN);
	GPIO_Init(SW1_PORT,&GPIOInitStruct);

	GPIOInitStruct.GPIO_Pin =  (1<<SW2_PIN);
	GPIO_Init(SW2_PORT,&GPIOInitStruct);

	GPIOInitStruct.GPIO_Pin =  (1<<SW3_PIN);
	GPIO_Init(SW3_PORT,&GPIOInitStruct);

	GPIOInitStruct.GPIO_Pin =  (1<<SW4_PIN);
	GPIO_Init(SW4_PORT,&GPIOInitStruct);
}

/*
 * Handles debounce for all switches
 * This function has its own time gate. Call it as often as possible
 */
void swDebounceTask()
{
	static uint32_t nextCallTime=0;
	if(systemTime>nextCallTime)
	{
		nextCallTime=systemTime+SW_DEBOUNCE_PERIOD;
		for(uint8_t i=0;i<SW_NOF_SWITCHES;i++)
		{
			if(getSwRaw(i))
			{
				if(switches[i].counter<SW_DEBOUNCE_COUNTER_TOP)
				{
					switches[i].counter++;
				}
			}
			else
			{
				if(switches[i].counter>0)
				{
					switches[i].counter--;
				}
			}
			if(switches[i].counter>=SW_DEBOUNCE_COUNTER_TOP)
			{
				if(switches[i].active==false)
				{
					switches[i].risingEdgeActive = true;
					switches[i].activationStartTime=systemTime;
					switches[i].fallingEdgeActive = false;
				}
				switches[i].active=true;
			}
			else if(switches[i].counter==0)
			{
				if(switches[i].active==true)
				{
					switches[i].risingEdgeActive = false;
					switches[i].fallingEdgeActive = true;
					switches[i].activationStartTime=UINT32_MAX;
				}
				switches[i].active=false;
			}
		}
	}
}

/*
 * Returns the state of a switch (true if active, false if not)
 * Will return false if the switch does not exist
 */
bool swGetState(uint8_t sw)
{
	if(sw>SW_NOF_SWITCHES)
	{
		return false;
	}
	return switches[sw-1].active;
}

/*
 * Returns true if a rising has occured
 * Will reset the rising edge state
 */
bool swGetRisingEdge(uint8_t sw)
{
	bool tmp=switches[sw-1].risingEdgeActive;
	switches[sw-1].risingEdgeActive=false;
	return tmp;
}

/*
 * Returns true if a rising has occured
 * Will reset the rising edge state
 */
bool swGetFallingEdge(uint8_t sw)
{
	bool tmp=switches[sw-1].fallingEdgeActive;
	switches[sw-1].fallingEdgeActive=false;
	return tmp;
}


/*
 * Returns true if a switch has been active for more than ms milliseconds
 * If true is ever returned, the state will be restarted treating it as if the switch has been released for a very short period of time
 */
bool swGetActiveForMoreThan(uint8_t sw, uint32_t ms)
{
	if(!swGetState(sw) || systemTime<ms)
	{
		return false;
	}
	else if((systemTime - ms) > switches[sw-1].activationStartTime)
	{
		switches[sw-1].activationStartTime=systemTime;
		return true;
	}
	else
	{
		return false;
	}
}

/*
 * Returns the raw value of a switch
 * If switch does not exist, it returns false
 */
static bool getSwRaw(uint8_t sw)
{
	sw++; //I'm so fucking weird and stupid :/
	switch(sw)
	{
	case 1:
		return SW1;
		break;
	case 2:
		return SW2;
		break;
	case 3:
		return SW3;
		break;
	case 4:
		return SW4;
		break;
//	case 5:
//		return SW5;
//		break;
//	case 6:
//		return SW6;
//		break;
//	case 7:
//		return SW7;
//		break;
//	case 8:
//		return SW8;
//		break;
	default:
		return false;
		break;
	}
}
