

// ----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include "diag/Trace.h"

#include "time.h"
#include "uart.h"
#include "xprintf.h"
#include "ledPwm.h"
#include "sw.h"
#include "apa102.h"
#include "utils.h"
#include "ledSegment.h"
#include "extFetCtrl.h"
#include "onboardLedCtrl.h"
#include "adc.h"
#include "powerManagement.h"
#include "advancedAnimations.h"

bool poorMansOS();
void poorMansOSRunAll();

// Sample pragmas to cope with warnings. Please note the related line at
// the end of this function, used to pop the compiler diagnostics status.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"


typedef enum
{
	SMODE_BLUE_FADE_CYAN_PULSE=0,
	SMODE_BLUE_FADE_CYAN_PULSE_2,
	SMODE_WHITE_FADE_CYAN_PULSE,
	SMODE_BLUE_TO_CYAN_NO_PULSE,
	SMODE_RANDOM,
	SMODE_DISCO,
	SMODE_BATTERY_DISP,
	SMODE_OFF,
	SMODE_NOF_MODES
}simpleModes_t;

static void dummyLedTask();
void displayBattery(uint8_t channel, uint8_t segment, uint16_t startLED);
uint8_t getBatteryLevel(uint8_t channel);

void handleApplicationSimple();

//Default global setting
#define GLOBAL_SETTING		10
#define INTENSITY_CHANGE_PERIOD			1000
#define GLITTER_TO_PULSE_CHANGE_TIME	5000

#define PULSE_FAST_PIXEL_TIME	1
#define PULSE_NORMAL_PIXEL_TIME	2
#define PULSE_PIXELS_PER_ITERATION_NORMAL	2
#define PULSE_PIXELS_PER_ITERATION_FAST		6

#define GLITTER_PIXELS_PER_ITERATION_NORMAL		5
#define GLITTER_PIXELS_PER_ITERATION_FAST		10
#define GLITTER_NORMAL_PIXEL_TIME		2000

#define FADE_FAST_TIME		500
#define FADE_NORMAL_TIME	2000

//The length of one side of the kigu
#define SINGLE_STRIP_LENGTH	51

//Segment number holders
//Init them as MAX_SEGS+1, to ensure they aren't used un-initialized
uint8_t segmentLeft=LEDSEG_MAX_SEGMENTS+1;
uint8_t segmentRight=LEDSEG_MAX_SEGMENTS+1;

//Holds the current global setting
uint8_t globalSetting=GLOBAL_SETTING;

//Start position of battery indicator LEDs
//Todo: Put the somewhere on the tip of the tail (possibly the two last fins)
volatile uint16_t batteryIndicatorStartLed=40;	//Should be the 4th to last fin

int main(int argc, char* argv[])
{
	SystemCoreClockUpdate();
	timeInit();
	swInit();
	//extFetInit();
	pwrMgtInit();
	adcInit();
	apa102SetDefaultGlobal(globalSetting);
	apa102Init(1,500);
	apa102Init(2,500);
	apa102UpdateStrip(APA_ALL_STRIPS);
	utilRandSeed(adcGetBatVolt(1));

	while(1)
	{
		poorMansOS();
	}


}	//End of main()

typedef enum
{
	BEAT_NOT_ACTIVE,
	BEAT_RECORDING,
	BEAT_RECORDING_FINISHED,
	BEAT_ACTIVE,
}beatModeState_t;
const uint8_t beatModeMaxEvents=7;

/*
 * Standard toothles mode:
 * 	Slow fade
 * 	Somewhat large span
 * 	Cyan or pure blue
 * 	No pulse
 * 	High max brightness
 *
 * 	Segments: 2 segments (first, second)
 * 	Båda är lika långa
 *
 * 	Segment lengths:
 * 	51 (tror jag)
 * 	Fins are as follows (from the top)
 *	Body: (3+6+7+7+6)
 *	Tail: (5+5+4+3+3+2)
 *	Left is first
 *	No sacrificial LED
 *
 */



/*
 * General task for handling simple modes
 */
void handleApplicationSimple()
{
	static bool setupDone=false;

	static ledSegmentPulseSetting_t pulse;
	static ledSegmentFadeSetting_t fade;

	//Used for fading to black
	static bool pulseIsActiveSaved=true;
	static uint32_t fadeTimeSaved=0;
	static uint32_t fadeToZeroStartTime=0;
	static bool lightIsActive=true;
	static bool isFadingToBlack=false;
	static bool savedActiveAnimSeqs[ANIM_SEQ_MAX_SEQS];

	//This is a loop for a simple user interface, with not as much control
	static simpleModes_t smode=SMODE_BLUE_FADE_CYAN_PULSE;
	static bool pulseIsActive=true;
	static bool pause=false;
	static uint32_t nextDiscoUpdate=0;
	static bool isPulseMode=false;

	static volatile uint16_t startTmp=1;
	static volatile uint16_t stopTmp=2;
	static volatile uint8_t segTmp=0;
	static volatile uint8_t globalTmp=GLOBAL_SETTING;
	static volatile RGB_t rgbTmp={100,100,100};

	static uint8_t animSequence1=0;
	static uint8_t animSequenceRainbowFade=0;
	static uint8_t animSequencePanFade=0;
	/*
	 * 2 points (loop):
	 *	1. blue fade, cyan pulse. 1 pulse cycle. Transition immediately.
	 *	2. blue fade. Timed transition on 3 seconds
	 */
	static uint8_t animSeqNormal=0;

	//Variables for beat control
	static eventTimeList beatEvents;
	static uint8_t beatAnimSequence=0;
	static beatModeState_t beatMode=BEAT_NOT_ACTIVE;

	if(!setupDone)
	{
		for(uint8_t i=0;i<ANIM_SEQ_MAX_SEQS;i++)
		{
			savedActiveAnimSeqs[i]=false;
		}

		animLoadLedSegPulseColour(SIMPLE_COL_CYAN,&pulse,255);
		pulse.mode=LEDSEG_MODE_LOOP_END;
		pulse.cycles =0;
		pulse.ledsFadeAfter = 4;
		pulse.ledsFadeBefore = 4;
		pulse.ledsMaxPower = 8;
		pulse.pixelTime = PULSE_NORMAL_PIXEL_TIME;
		pulse.pixelsPerIteration = PULSE_PIXELS_PER_ITERATION_NORMAL;
		pulse.startDir = 1;
		pulse.startLed = 1;
		pulse.globalSetting=0;
		pulse.colourSeqNum=0;

		animLoadLedSegFadeColour(SIMPLE_COL_BLUE,&fade,50,250);
		fade.cycles=0;
		fade.mode = LEDSEG_MODE_BOUNCE;
		fade.startDir = -1;
		fade.fadeTime = FADE_NORMAL_TIME;
		fade.globalSetting=0;

		isPulseMode=true;
		//segmentBottomFull=ledSegInitSegment(1,1,STRIP_LEN_BOTTOM_FULL,false,false,&pulse,&fade);

		//Todo: Test ledseg cycle counter etc
		if(0) //Test to add multiple segments on a short strip
		{

		}
		else
		{
			fade.syncGroup=1;
			segmentLeft=ledSegInitSegment(1,1,SINGLE_STRIP_LENGTH,false,false,&pulse,&fade); //Max: 350 (or actually less). 370 is for series with head
			segmentRight=ledSegInitSegment(1,SINGLE_STRIP_LENGTH+1,2*SINGLE_STRIP_LENGTH,false,false,&pulse,&fade); //Max: 350 (or actually less). 370 is for series with head
		}
		//Setup animations
		animSeqPoint_t pt;

		//Allocate animation sequences

		animSequenceRainbowFade=animGenerateFadeSequence(ANIM_SEQ_MAX_SEQS, LEDSEG_ALL,1,0,PRIDE_COL_NOF_COLOURS,(RGB_t*)coloursPride,500,100,255,false);
		animSequencePanFade=animGenerateFadeSequence(ANIM_SEQ_MAX_SEQS, LEDSEG_ALL,1,0,PAN_COL_NOF_COLOURS,(RGB_t*)coloursPan,500,100,255,false);

		//Load a dummy point to init and allocate an animation sequence for beat mode
		beatAnimSequence=animSeqInit(LEDSEG_ALL,false,0,&pt,1);
		animSeqSetActive(LEDSEG_ALL,false);

		//Setup normal fade
		animSeqPoint_t normalPt;
		pulse.cycles=1;
		fade.cycles=4;
		animSeqFillPoint(&normalPt,&fade,&pulse,0,false,false,false,false,false,false);
		animSeqNormal=animSeqInit(LEDSEG_ALL,false,0,&normalPt,1);
		animSeqSetActive(animSeqNormal,true);
		pulse.cycles=0;
		fade.cycles=0;
		segTmp=segmentLeft;
		setupDone=true;
		animSeqSetRestart(animSeqNormal);
	}
	//Change mode
	if(swGetFallingEdge(1) && !pause)
	{
		bool fadeAlreadySet=false;
		bool pulseAlreadySet=false;
		//Handles if something needs to be done when changing from a state (exit a mode)
		switch(smode)
		{
			case SMODE_DISCO:
			{
				if(isPulseMode)
				{
					pulse.pixelTime=PULSE_NORMAL_PIXEL_TIME;	//This is valid in glitter mode, as glitter mode will cap to the highest possible
					pulse.pixelsPerIteration = PULSE_PIXELS_PER_ITERATION_NORMAL;
					fade.fadeTime=FADE_NORMAL_TIME;
				}
				else
				{
					pulse.pixelTime=GLITTER_NORMAL_PIXEL_TIME;	//This is valid in glitter mode, as glitter mode will cap to the highest possible
					pulse.pixelsPerIteration = GLITTER_PIXELS_PER_ITERATION_NORMAL;
					fade.fadeTime=FADE_NORMAL_TIME;
				}
				break;
			}
			case SMODE_OFF:
			{
				//Restore power to LEDs
				pwrMgtSetLEDPwr(APA_ALL_STRIPS,true);
				fade.cycles=0;
				break;
			}
			case SMODE_BLUE_FADE_CYAN_PULSE:
			{
				animSeqSetActive(animSeqNormal,false);
				break;
			}
			default:
			{
				//Do nothing for default
			}
		}

		if(beatMode!=BEAT_NOT_ACTIVE)
		{
			beatMode=BEAT_NOT_ACTIVE;
			animSeqSetActive(beatAnimSequence,false);
		}

		//Enter a mode
		smode++;
		if(smode>=SMODE_NOF_MODES)
		{
			smode=0;
		}
		switch(smode)
		{
			case SMODE_BLUE_FADE_CYAN_PULSE:
				animLoadLedSegFadeColour(SIMPLE_COL_BLUE,&fade,100,255);
				animSetModeChange(SIMPLE_COL_BLUE,&fade,LEDSEG_ALL,false,100,255,true);
				animLoadLedSegPulseColour(SIMPLE_COL_CYAN,&pulse,255);
				fade.cycles=0;
				pulse.cycles=0;
				fadeAlreadySet=true;
				pulseIsActive=true;
			break;
			case SMODE_BLUE_FADE_CYAN_PULSE_2:
				animSeqSetRestart(animSeqNormal);
				animSeqSetActive(animSeqNormal,true);
				fadeAlreadySet=true;
				pulseAlreadySet=true;
				pulseIsActive=true;
				break;
			case SMODE_WHITE_FADE_CYAN_PULSE:
				animLoadLedSegFadeColour(SIMPLE_COL_WHITE,&fade,100,255);
				fade.cycles=0;
				animSetModeChange(SIMPLE_COL_WHITE,&fade,LEDSEG_ALL,false,100,255,true);
				fadeAlreadySet=true;
				animLoadLedSegPulseColour(SIMPLE_COL_CYAN,&pulse,255);
				pulse.cycles=0;
				pulseIsActive=true;
				break;
			case SMODE_BLUE_TO_CYAN_NO_PULSE:
				animLoadLedSegFadeBetweenColours(SIMPLE_COL_BLUE,SIMPLE_COL_CYAN,&fade,200,200);
				animSetModeChange(SIMPLE_COL_NO_CHANGE,&fade,LEDSEG_ALL,false,100,255,true);
				fadeAlreadySet=true;
				pulseIsActive=false;
				break;
			case SMODE_DISCO:
				if(isPulseMode)
				{
					pulse.pixelTime=PULSE_FAST_PIXEL_TIME;	//This is valid in glitter mode, as glitter mode will cap to the highest possible
					pulse.pixelsPerIteration = PULSE_PIXELS_PER_ITERATION_FAST;
					fade.fadeTime=FADE_FAST_TIME;
				}
				else
				{
					pulse.pixelTime=PULSE_FAST_PIXEL_TIME;	//This is valid in glitter mode, as glitter mode will cap to the highest possible
					pulse.pixelsPerIteration = GLITTER_PIXELS_PER_ITERATION_FAST;
					fade.fadeTime=FADE_FAST_TIME;
				}	//The break is omitted by design, since SMODE_DISCO does the same thing as SMODE_RANDOM
			case SMODE_RANDOM:
				animLoadLedSegFadeColour(SIMPLE_COL_RANDOM,&fade,150,250);
				animLoadLedSegPulseColour(SIMPLE_COL_RANDOM,&pulse,255);
				pulseIsActive=true;
				break;
			case SMODE_BATTERY_DISP:
			{
				//Do nothing here
				break;
			}
			case SMODE_OFF:	//turn LEDs off
			{
				fade.r_min=0;
				fade.r_max=0;
				fade.g_min=0;
				fade.g_max=0;
				fade.b_min=0;
				fade.b_max=0;
				fade.cycles=1;
				pulse.r_max=0;
				pulse.g_max=0;
				pulse.b_max=0;
				animSetModeChange(SIMPLE_COL_NO_CHANGE,&fade,LEDSEG_ALL,false,0,0,false);
				fadeAlreadySet=true;
				break;
			}
			case SMODE_NOF_MODES:	//Should never happen
			{
				smode=0;
				break;
			}
		}//End of switch clause for all the modes

		//Update all segements
		if(!fadeAlreadySet)
		{
			ledSegSetFade(LEDSEG_ALL,&fade);
		}
		if(!pulseAlreadySet)
		{
			ledSegSetPulse(LEDSEG_ALL,&pulse);
			ledSegSetPulseActiveState(LEDSEG_ALL,pulseIsActive);
		}

		if(smode == SMODE_BATTERY_DISP)
		{
			//Pause The other segment (possible arm)
			//Set LEDs in the correct place to the right colours, corresponding to battery level
			ledSegSetPulseActiveState(segmentLeft,false);
			ledSegSetFadeActiveState(segmentLeft,false);
			displayBattery(1,segmentLeft,batteryIndicatorStartLed);
		}
		//To ensure that the statemachine for fade to black/light is not fucked up
		lightIsActive=true;
	}	//End of change mode clause

	//Switches between pulse mode and glitter mode
	if(swGetActiveForMoreThan(1,GLITTER_TO_PULSE_CHANGE_TIME))
	{
		if(isPulseMode)
		{
			//Switch to glitter mode
			pulse.mode = LEDSEG_MODE_GLITTER_BOUNCE;
			pulse.cycles =0;
			pulse.ledsFadeAfter = 0;
			pulse.ledsFadeBefore = 5;
			pulse.ledsMaxPower = 150;
			pulse.pixelTime = 2000;
			pulse.pixelsPerIteration = GLITTER_PIXELS_PER_ITERATION_NORMAL;
			pulse.startDir =1;
			pulse.startLed = 1;
			isPulseMode=false;
		}
		else
		{
			//Switch to pulse mode
			pulse.mode=LEDSEG_MODE_LOOP_END;
			pulse.cycles =0;
			pulse.ledsFadeAfter = 10;
			pulse.ledsFadeBefore = 10;
			pulse.ledsMaxPower = 20;
			pulse.pixelTime = PULSE_NORMAL_PIXEL_TIME;
			pulse.pixelsPerIteration = PULSE_PIXELS_PER_ITERATION_NORMAL;
			pulse.startDir =1;
			pulse.startLed = 1;
			isPulseMode=true;
		}
		ledSegSetFade(LEDSEG_ALL,&fade);
		ledSegSetPulse(LEDSEG_ALL,&pulse);
		ledSegSetPulseActiveState(LEDSEG_ALL,pulseIsActive);
	}
	//Generate a pulse
	if(swGetRisingEdge(2))
	{
		if(smode==SMODE_OFF)
		{
			//Toggle fan
			pwrMgtToogleFan();
		}
		else if(pause)
		{
			//Debug function to try out colours (only usable while in Debug)
			ledSegSetRangeWithGlobal(segTmp,startTmp,stopTmp,rgbTmp.r,rgbTmp.g,rgbTmp.b,globalTmp);
		}
		else
		{
			apa102SetDefaultGlobal(globalSetting*4);
			ledSegRestart(LEDSEG_ALL,true,pulseIsActive);
			//Force update on all strips
			apa102UpdateStrip(APA_ALL_STRIPS);

			//If we're in beat recording mode, record one event here. The event system will tell when we've gone over the number of allowed recordings
			if(beatMode==BEAT_RECORDING)
			{
				if(eventTimedSendTrig(&beatEvents,true))
				{
					beatMode=BEAT_RECORDING_FINISHED;
				}
			}
			else if(beatMode==BEAT_ACTIVE)
			{
				//Todo:Check which beatsegment is currently active, and restart it
				animSeqSetRestart(beatAnimSequence);
			}
		}
	}
	if(swGetFallingEdge(2))
	{
		apa102SetDefaultGlobal(globalSetting);
	}

	//Enable beat detection mode
	if(swGetActiveForMoreThan(2,GLITTER_TO_PULSE_CHANGE_TIME) || beatMode==BEAT_RECORDING_FINISHED)
	{
		//Start recording, if we're not already
		if(beatMode==BEAT_NOT_ACTIVE || beatMode==BEAT_ACTIVE)
		{
			eventTimedInit(&beatEvents,true,beatModeMaxEvents,false);
			beatMode=BEAT_RECORDING;
			animSeqSetActive(beatAnimSequence,false);
			ledSegSetFade(LEDSEG_ALL,&fade);
			ledSegSetPulse(LEDSEG_ALL,&pulse);
			ledSegSetPulseActiveState(LEDSEG_ALL,pulseIsActive);
		}
		//Recording finishes when switch is again held for a long time
		else if(beatMode==BEAT_RECORDING || beatMode==BEAT_RECORDING_FINISHED)
		{
			eventTimedStopRecording(&beatEvents);
			//Recording is finished, generate a sequence and start it
			if(animSeqIsActive(animSequenceRainbowFade))
			{
				animSeqModifyToBeat(animSequenceRainbowFade,&beatEvents,false);
				animSeqSetRestart(animSequenceRainbowFade);
			}
			else if(animSeqIsActive(animSequencePanFade))
			{
				animSeqModifyToBeat(animSequencePanFade,&beatEvents,false);
				animSeqSetRestart(animSequencePanFade);
			}
			else
			{
				beatAnimSequence=animGenerateBeatSequence(beatAnimSequence,LEDSEG_ALL,0,0,eventTimeGetNofEventsRecorded(&beatEvents),&fade,&pulse,true,pulseIsActive,globalSetting*4,&beatEvents,false);
				animSeqSetActive(LEDSEG_ALL,false);
				animSeqSetRestart(beatAnimSequence);
			}
			beatMode=BEAT_ACTIVE;
		}
		apa102SetDefaultGlobal(globalSetting);
	}
	//Set lights on/off
	if(swGetFallingEdge(3))
	{
		if(animSeqTrigReady(animSequence1))
		{
			animSeqTrigTransition(animSequence1);
		}
		else
		{
			if(!pause)
			{
				ledSegSetFadeActiveState(LEDSEG_ALL,false);
				ledSegSetPulseActiveState(LEDSEG_ALL,false);
				pause=true;
				if(smode==SMODE_OFF)
				{
					pwrMgtSetLEDPwr(APA_ALL_STRIPS,true);
				}
			}
			else
			{
				ledSegSetFadeActiveState(LEDSEG_ALL,true);
				ledSegSetPulseActiveState(LEDSEG_ALL,pulseIsActive);
				pause=false;
				if(smode==SMODE_OFF)
				{
					pwrMgtSetLEDPwr(APA_ALL_STRIPS,false);
				}
			}
		}
	}
	//Change light intensity (global setting)
	if(swGetActiveForMoreThan(3,INTENSITY_CHANGE_PERIOD))
	{
		globalSetting++;
		if(globalSetting>APA_MAX_GLOBAL_SETTING/2)
		{
			globalSetting=0;
		}
		apa102SetDefaultGlobal(globalSetting);
	}
	if(swGetRisingEdge(4))
	{
		//Measure the time of the press and connect to speed of fade
		fadeToZeroStartTime=systemTime;
	}
	if(swGetFallingEdge(4))
	{
		if(lightIsActive)
		{
			//Save current state
			pulseIsActiveSaved = pulseIsActive;
			//Setup lights fade down
			fadeTimeSaved=fade.fadeTime;
			fade.fadeTime=systemTime-fadeToZeroStartTime;
			animSetModeChange(SIMPLE_COL_OFF,&fade,LEDSEG_ALL,false,0,0,false);
			pulseIsActive=false;
			ledSegSetPulseActiveState(LEDSEG_ALL,pulseIsActive);
			//Save and de-activete active animation sequences
			for(uint8_t i=0;i<ANIM_SEQ_MAX_SEQS;i++)
			{
				if(animSeqIsActive(i))
				{
					savedActiveAnimSeqs[i]=true;
					animSeqSetActive(i,false);
				}
				else
				{
					savedActiveAnimSeqs[i]=false;
				}
			}
			lightIsActive=false;
			isFadingToBlack=true;
		}
		else
		{
			//Restore state and fade lights up
			fade.fadeTime=systemTime-fadeToZeroStartTime;
			animSetModeChange(SIMPLE_COL_NO_CHANGE,&fade,LEDSEG_ALL,true,0,0,false);
			//Save and de-activete active animation sequences
			for(uint8_t i=0;i<ANIM_SEQ_MAX_SEQS;i++)
			{
				if(savedActiveAnimSeqs[i])
				{
					animSeqSetActive(i,true);
				}
			}
			lightIsActive=true;
			isFadingToBlack=true;
		}
	}

	//End of handling various switches

	//Check if we're currently fading to/from black and if we're done
	if(isFadingToBlack && ledSegGetFadeSwitchDone(LEDSEG_ALL))
	{
		isFadingToBlack=false;
		if(lightIsActive)
		{
			//We faded from black, so restore the fade time and restore pulse.
			fade.fadeTime=fadeTimeSaved;
			animSetModeChange(SIMPLE_COL_NO_CHANGE,&fade,LEDSEG_ALL,true,0,0,false);
			pulseIsActive=pulseIsActiveSaved;
			ledSegSetPulseActiveState(LEDSEG_ALL,pulseIsActive);
			ledSegRestart(LEDSEG_ALL,false,true);
		}
	}

	//Handle special modes
	switch(smode)
	{
		case SMODE_DISCO:
		{
			if(systemTime>nextDiscoUpdate)
			{
				nextDiscoUpdate=systemTime+FADE_FAST_TIME;
				animLoadLedSegFadeColour(SIMPLE_COL_RANDOM,&fade,150,250);
				animLoadLedSegPulseColour(SIMPLE_COL_RANDOM,&pulse,250);
				ledSegSetFade(LEDSEG_ALL,&fade);
				ledSegSetPulse(LEDSEG_ALL,&pulse);
				ledSegSetPulseActiveState(LEDSEG_ALL,pulseIsActive);
			}
			break;
		}
		case SMODE_OFF:
		{
			//Turn off power to LED strips once the end has been reached (and only do it when any strip is active)
			if(ledSegGetFadeDone(LEDSEG_ALL) && pwrMgtGetLedPwrState(APA_ALL_STRIPS))
			{
				pwrMgtSetLEDPwr(APA_ALL_STRIPS,false);
			}
			break;
		}
		default:
		{
			//Do nothing for default
			break;
		}
	}	//End of special mode handling
}	//End of simple main loop mode handling

//The different battery levels in mV
//The first is the lowest battery level
#define NOF_BATTERY_LEVELS	5
const uint16_t batteryLevels[NOF_BATTERY_LEVELS] ={3300,3500,3700,3900,4100};

/*
 * Displays the battery state on a given segment with a given start LED (takes a total of 5 LEDs)
 */
void displayBattery(uint8_t channel, uint8_t segment, uint16_t startLED)
{
	volatile uint16_t voltage=0;
	voltage=adcGetBatVolt(channel);
	for(uint8_t i=0;i<NOF_BATTERY_LEVELS;i++)
	{
		if(voltage>batteryLevels[i])
		{
			ledSegSetLed(segment,startLED+i,0,250,0);
		}
		else
		{
			ledSegSetLed(segment,startLED+i,250,0,250);
		}
	}
}

/*
 * Returns the current battery level
 */
uint8_t getBatteryLevel(uint8_t channel)
{
	volatile uint16_t voltage=0;
	voltage=adcGetBatVolt(channel);
	for(uint8_t i=0;i<NOF_BATTERY_LEVELS;i++)
	{
		if(voltage<batteryLevels[i])
		{
			return i;
		}
	}
}

/*
 * Dummy task to blink LEDs
 */
static void dummyLedTask()
{
	static uint32_t nextCallTime=0;
	if(systemTime>nextCallTime)
	{
		nextCallTime=systemTime+250;
		LED_BOARD_TOGGLE();
	}
}

#define MODE_HANDLER_CALL_PERIOD	25

//Some switches might have more than one function depending on mode
#define SW_MODE			1
#define SW_COL_UP		2
#define SW_PAUSE		2
#define SW_SETUP_SYNC 	3
#define SW_ONOFF		4
/*
 * Todo: Rethink this completely
 *
 * The "main" loop of the program. Will read the buttons and change modes accordingly
 * Called from poorManOS
 *
 * General idea for control modes:
 * Modes:
 * 		Set fade colour
 * 			SW1, beat synchronizer, if active. Otherwise, cycle mode.
 * 			SW2, colour fade up (we have a random colour, an off colour, and it will loop)
 * 			SW3, beat synch start/stop.
 * 			SW4, toggle fade on/off
 * 		Set pulse colour
 * 			SW1, beat synchronizer, if active. Otherwise, cycle mode.
 * 			SW2, colour fade up (we have a random colour, an off colour, and it will loop)
 * 			SW3, beat synch start/stop.
 * 			SW4, toggle pulse on/off
 * 		Pause
 * 			SW1, Cycle mode
 * 			SW2, freeze/unfreeze colour
 *
 *	Colours are fetched from the disco_led_fade struct (use max as is, divided by 4. Use min as new max divided by 3)
 *	If colour is set to DISCO_NOF_COLOURS, a random colour will be used
 */
void handleModes()
{

}


static volatile bool mutex=false;
#define OS_NOF_TASKS 4
/*
 * Semi-OS, used for tasks that are not extremely time critical and might take a while to perform
 */
bool poorMansOS()
{
	static uint8_t task=0;
	if(mutex)
	{
		return false;
	}
	mutex=true;
	switch(task)
	{
		case 0:
			ledSegRunIteration();
		break;
		case 1:
			swDebounceTask();
		break;
		case 2:
			handleApplicationSimple();
			dummyLedTask();
		break;
		case 3:
			animTask();
		break;
		default:
		break;
	}
	task++;
	if(task>=OS_NOF_TASKS)
	{
		task=0;
	}
	mutex=false;
	return true;
}

/*
 * Runs a full iteration of all tasks in the "OS"
 */
void poorMansOSRunAll()
{
	for(uint8_t i=0;i<=OS_NOF_TASKS;i++)
	{
		while(!poorMansOS()){}
	}
}
#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------
