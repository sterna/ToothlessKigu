

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
	SMODE_BLUE_FADE_YLW_PULSE=0,
	SMODE_CYAN_FADE_YLW_PULSE,
	SMODE_RED_FADE_YLW_PULSE,
	SMODE_YLW_FADE_PURPLE_PULSE,
	SMODE_YLW_FADE_GREEN_PULSE,
	SMODE_GREEN_FADE_PURPLE_PULSE,
	SMODE_CYAN_FADE_NO_PULSE,
	SMODE_YLW_FADE_NO_PULSE,
	SMODE_RED_FADE_NO_PULSE,
	SMODE_BLUE_TO_RED_NO_PULSE,
	SMODE_BLUE_TO_RED_GREEN_PULSE,
	SMODE_CYAN_TO_PURPLE_NO_PULSE,
	SMODE_WHITE_FADE_RAINBOW_PULSE,
	SMODE_WHITE_FADE_PAN_PULSE,
	SMODE_WHITE_FADE_ITALY_PULSE,
	SMODE_RANDOM,
	SMODE_DISCO,
	SMODE_PRIDE_WHEEL,
	SMODE_PAN_WHEEL,
	SMODE_BATTERY_DISP,
	SMODE_OFF,
	SMODE_STAD_I_LJUS,
	SMODE_NOF_MODES
}simpleModes_t;

static void dummyLedTask();
void displayBattery(uint8_t channel, uint8_t segment, uint16_t startLED);
uint8_t getBatteryLevel(uint8_t channel);

void handleApplicationSimple();

//Sets if the program goes into the staff
#define GLOBAL_SETTING		2
#define INTENSITY_CHANGE_PERIOD			1000
#define GLITTER_TO_PULSE_CHANGE_TIME	5000

#define PULSE_FAST_PIXEL_TIME	1
#define PULSE_NORMAL_PIXEL_TIME	2
#define PULSE_PIXELS_PER_ITERATION_NORMAL	3
#define PULSE_PIXELS_PER_ITERATION_FAST		6

#define GLITTER_PIXELS_PER_ITERATION_NORMAL		5
#define GLITTER_PIXELS_PER_ITERATION_FAST		10
#define GLITTER_NORMAL_PIXEL_TIME		2000

#define FADE_FAST_TIME		300
#define FADE_NORMAL_TIME	700

uint8_t segmentArmLeft=LEDSEG_MAX_SEGMENTS+1;
uint8_t segmentArmRight=LEDSEG_MAX_SEGMENTS+1;
uint8_t segmentHead=LEDSEG_MAX_SEGMENTS+1;
uint8_t segmentEyes=LEDSEG_MAX_SEGMENTS+1;
uint8_t segmentTail=LEDSEG_MAX_SEGMENTS+1;
uint8_t segmentBatteryIndicator=LEDSEG_MAX_SEGMENTS+1;
uint8_t segmentLegRight=LEDSEG_MAX_SEGMENTS+1;
uint8_t segmentLegLeft=LEDSEG_MAX_SEGMENTS+1;
uint8_t segmentBottomFull=LEDSEG_MAX_SEGMENTS+1;
uint8_t segmentTopFull=LEDSEG_MAX_SEGMENTS+1;
uint8_t globalSetting=GLOBAL_SETTING;

uint8_t segmentTest1=0;
uint8_t segmentTest2=0;
uint8_t segmentTest3=0;
uint8_t segmentTest4=0;

volatile uint16_t batteryIndicatorStartLed=151;	//There are 156 and 157 LEDs on each side

//Define true is partial is used (will re-write the states)
const bool partial=false;

/*
 * Number of LEDs for the various parts of the suit:
 * Tail, First (right) part: 1-79
 * Tail, Second (left) part: 80-183
 * Leg, right, part1: 1-74
 * Leg, right, part2: 75-89	(last in part2 is the same as first in part3. This LED is #89)
 * Leg, right, part3: 89-213 (including sacrificial LED (#213))
 * Leg, left, whole: 214-385 (including last led, which is the same as the first on next segment)
 */

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

#define STRIP_LEN_BOTTOM_FULL	385
#define STPIP_LEN_TOP			309
#define STRIP_LEN_TAIL			181
//Includes eyes
#define STRIP_LEN_HEAD			52

typedef enum
{
	BEAT_NOT_ACTIVE,
	BEAT_RECORDING,
	BEAT_RECORDING_FINISHED,
	BEAT_ACTIVE,
}beatModeState_t;
const uint8_t beatModeMaxEvents=7;


/*
 * General task for handling simple modes
 */
void handleApplicationSimple()
{
	static bool setupDone=false;

	static ledSegmentPulseSetting_t pulse;
	static ledSegmentFadeSetting_t fade;
	static ledSegmentFadeSetting_t fadeEyes;

	//Used for fading to black
	static bool pulseIsActiveSaved=true;
	static uint32_t fadeTimeSaved=0;
	static uint32_t fadeToZeroStartTime=0;
	static bool lightIsActive=true;
	static bool isFadingToBlack=false;
	static bool savedActiveAnimSeqs[ANIM_SEQ_MAX_SEQS];

	//This is a loop for a simple user interface, with not as much control
	static simpleModes_t smode=SMODE_DISCO;
	static bool pulseIsActive=true;
	static bool pause=false;
	static uint32_t nextDiscoUpdate=0;
	static uint8_t stadILjusState=0;
	static uint32_t stadILjusTime=0;
	static bool stadILjusIsLoaded=false;
	static bool isPulseMode=false;

	static volatile uint16_t startTmp=1;
	static volatile uint16_t stopTmp=2;
	static volatile uint8_t segTmp=0;
	static volatile uint8_t globalTmp=GLOBAL_SETTING;
	static volatile RGB_t rgbTmp={100,100,100};

	static uint8_t animSequence1=0;
	static uint8_t animSequenceRainbowFade=0;
	static uint8_t animSequencePanFade=0;

	//Variables for beat control
	static eventTimeList beatEvents;
	static uint8_t beatAnimSequence=0;
	static beatModeState_t beatMode=BEAT_NOT_ACTIVE;

	//Eyes are located as LED 54 and 55, if counted including one sacrificial LED on the head. Eyes are at 361 and 362, if connected after upper body (no extra LED)
	//Arm ends at 310. This last LED is the same as the next LED (as per standard). Use a sacrificial LED here and index Head from 311.

	if(!setupDone)
	{
		for(uint8_t i=0;i<ANIM_SEQ_MAX_SEQS;i++)
		{
			savedActiveAnimSeqs[i]=false;
		}

		animLoadLedSegPulseColour(SIMPLE_COL_GREEN,&pulse,255);
		pulse.cycles =0;
		pulse.ledsFadeAfter = 0;
		pulse.ledsFadeBefore = 5;
		pulse.ledsMaxPower = 150;
		pulse.mode = LEDSEG_MODE_GLITTER_BOUNCE;
		pulse.pixelTime = GLITTER_NORMAL_PIXEL_TIME;
		pulse.pixelsPerIteration = 5;
		pulse.startDir =1;
		pulse.startLed = 1;
		pulse.globalSetting=0;
		pulse.colourSeqNum=0;
		animLoadLedSegFadeColour(SIMPLE_COL_YELLOW,&fade,100,255);
		fade.cycles=0;
		fade.mode = LEDSEG_MODE_BOUNCE;
		fade.startDir = -1;
		fade.fadeTime = 700;
		fade.globalSetting=0;
		fade.syncGroup=0;
		//segmentBottomFull=ledSegInitSegment(1,1,STRIP_LEN_BOTTOM_FULL,false,false,&pulse,&fade);

		//TOdo: Test ledseg cycle counter etc
		if(0) //Test to add multiple segments on a short strip
		{
			fade.syncGroup=2;
			pulse.mode=LEDSEG_MODE_LOOP_END;
			pulse.ledsMaxPower=4;
			pulse.ledsFadeBefore=5;
			pulse.ledsFadeAfter=3;
			pulse.pixelsPerIteration =1;
			pulse.pixelTime=3;
			pulse.cycles=0;
			pulse.startDir=-1;
			pulse.startLed = -5;
			segmentTest1=ledSegInitSegment(1,1,19,false,true,&pulse,&fade);
			animLoadLedSegFadeColour(SIMPLE_COL_BLUE,&fade,100,255);
			animLoadLedSegPulseColour(SIMPLE_COL_RED,&pulse,255);
			pulse.mode = LEDSEG_MODE_LOOP;
	//		pulse.cycles=9;
			pulse.startLed = -3;
			segmentTest2=ledSegInitSegment(1,20,39,false,true,&pulse,&fade);
			animLoadLedSegFadeColour(SIMPLE_COL_WHITE,&fade,100,255);
			animLoadLedSegPulseColour(SIMPLE_COL_YELLOW,&pulse,255);
			pulse.mode = LEDSEG_MODE_BOUNCE;
			pulse.pixelTime=1;
			//pulse.pixelsPerIteration=1;
	//		pulse.cycles=10;
			pulse.startLed = 5;
			segmentTest3=ledSegInitSegment(1,40,59,false,true,&pulse,&fade);
			animLoadLedSegFadeColour(SIMPLE_COL_PURPLE,&fade,100,255);
			animLoadLedSegPulseColour(SIMPLE_COL_CYAN,&pulse,255);
			pulse.mode = LEDSEG_MODE_BOUNCE;
	//		pulse.cycles=10;
			pulse.startLed = 5;
			segmentTest4=ledSegInitSegment(1,60,80,false,true,&pulse,&fade);
		}
		else
		{
			fade.syncGroup=1;
			if(!partial)
			{
				segmentTopFull=ledSegInitSegment(2,1,STPIP_LEN_TOP,false,false,&pulse,&fade); //Max: 350 (or actually less). 370 is for series with head
				segmentHead=ledSegInitSegment(2,STPIP_LEN_TOP+1,STPIP_LEN_TOP+1+STRIP_LEN_HEAD-2,false,false,&pulse,&fade); //Max: 350 (or actually less). 370 is for series with head
				segmentBottomFull=ledSegInitSegment(1,1,1+STRIP_LEN_BOTTOM_FULL,false,false,&pulse,&fade); //For partial, no upper body is used
			}
			else
			{
				segmentHead=ledSegInitSegment(2,1,1+STRIP_LEN_HEAD-2,false,false,&pulse,&fade); //For partial, no upper body is used
				segmentBottomFull=ledSegInitSegment(1,1,1+STRIP_LEN_TAIL,false,false,&pulse,&fade); //For partial, no upper body is used
			}
			//Setup eyes and head
			memcpy(&fadeEyes,&fade,sizeof(ledSegmentFadeSetting_t));
			animLoadLedSegFadeColour(SIMPLE_COL_WHITE,&fadeEyes,50,255);
			fadeEyes.syncGroup=0;
			if(!partial)
			{
				segmentEyes=ledSegInitSegment(2,STPIP_LEN_TOP+STRIP_LEN_HEAD+1,STPIP_LEN_TOP+STRIP_LEN_HEAD+3,false,true,0,&fadeEyes); //Max: 350 (or actually less). 370 is for series with head
			}
			else
			{
				segmentEyes=ledSegInitSegment(2,STRIP_LEN_HEAD+1,STRIP_LEN_HEAD+3,false,true,0,&fadeEyes); //Max: 350 (or actually less). 370 is for series with head
			}
		}
//		segmentArmLeft=ledSegInitSegment(2,1,350,&pulse,&fade);
//		segmentTail=ledSegInitSegment(1,1,185,&pulse,&fade);	//Todo: change back number to the correct number (150-isch)
//		segmentArmLeft=ledSegInitSegment(2,1,185,&pulse,&fade);	//Todo: change back number to the correct number (150-isch)

		//Setup animations
		animSeqPoint_t pt;
		//General settings:
		//Pulse

////		pulse.mode = LEDSEG_MODE_LOOP_END;
//		pulse.mode = LEDSEG_MODE_LOOP_END;
//		pulse.cycles=1;
//		pulse.ledsFadeAfter = 10;
//		pulse.ledsFadeBefore = 10;
////		pulse.ledsMaxPower = 100;
//		pulse.ledsMaxPower = 100;
//		pulse.startDir =1;
//		pulse.startLed =1;
//		//pulse.rainbowColour=true;
//		pulse.pixelTime=PULSE_NORMAL_PIXEL_TIME;
////		animLoadLedSegPulseColour(SIMPLE_COL_RED,&pulse,230);
////		pulse.pixelTime=1400;

		//Fade
//		fade.syncGroup=0;
//		fade.fadeTime=900;
//		fade.cycles=1;
//		fade.mode = LEDSEG_MODE_BOUNCE;
//		fade.startDir =1;
//		uint8_t yScale=100;
//		uint8_t rScale=100;
//		animLoadLedSegFadeBetweenColours(SIMPLE_COL_YELLOW,SIMPLE_COL_RED,&fade,yScale,rScale);
//
//		//Generate first point and anim seq
////		colIndex=animLoadNextRainbowWheel(&fade,LEDSEG_MAX_SEGMENTS+1,colIndex);	//Using this to load the rainbow sequence. The loading will be performed, but not segment will be set.
//		pulse.pixelsPerIteration = 2;
////		pulse.pixelsPerIteration = 15;
//		animSeqFillPoint(&pt,&fade,NULL,250,false,false,false,false,true,false);
//		animSequence1=animSeqInit(segmentTopFull,false,3,&pt,1);	//segmentTopFull
//		//Generate each point
//		for(uint8_t i=0;i<5;i++)
//		{
//			//yScale+=10;
//			rScale+=20;
//			animLoadLedSegFadeBetweenColours(SIMPLE_COL_YELLOW,SIMPLE_COL_RED,&fade,yScale,rScale);
////			colIndex=animLoadNextRainbowWheel(&fade,LEDSEG_MAX_SEGMENTS+1,colIndex);
////			pulse.pixelsPerIteration+=1;
////			pulse.pixelTime-=200;
//			fade.fadeTime-=125;
//			animSeqFillPoint(&pt,&fade,NULL,250,false,false,false,false,true,false);
//			animSeqAppendPoint(animSequence1,&pt);
//		}
//		pulse.colourSeqNum = PRIDE_COL_NOF_COLOURS;
//		pulse.colourSeqPtr = (RGB_t*)coloursPride;
//		animSeqFillPoint(&pt,NULL,&pulse,0,false,false,false,true,false,false);
//		animSeqAppendPoint(animSequence1,&pt);
//
//		//animSeqSetRestart(animSequence1);
//		//Restore fade mode to bounce
//		fade.mode = LEDSEG_MODE_BOUNCE;
//		fade.syncGroup=1;

		//Allocate animation sequences

		animSequenceRainbowFade=animGenerateFadeSequence(ANIM_SEQ_MAX_SEQS, LEDSEG_ALL,1,0,PRIDE_COL_NOF_COLOURS,(RGB_t*)coloursPride,500,100,255,false);
		animSequencePanFade=animGenerateFadeSequence(ANIM_SEQ_MAX_SEQS, LEDSEG_ALL,1,0,PAN_COL_NOF_COLOURS,(RGB_t*)coloursPan,500,100,255,false);

		//Load a dummy point to init and allocate an animation sequence for beat mode
		beatAnimSequence=animSeqInit(LEDSEG_ALL,false,0,&pt,1);
		animSeqSetActive(LEDSEG_ALL,false);

		segTmp=segmentEyes;
		setupDone=true;
		fade.cycles=0;
	}
	//Change mode
	if(swGetFallingEdge(1) && !pause)
	{
		bool fadeAlreadySet=false;
		bool pulseAlreadySet=false;
		//Handles if something needs to be done when changing from a state
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
			case SMODE_PRIDE_WHEEL:
			{
				//Turn of pride-wheel
				animSeqSetActive(animSequenceRainbowFade,false);
				break;
			}
			case SMODE_PAN_WHEEL:
			{
				animSeqSetActive(animSequencePanFade,false);
				break;
			}
			case SMODE_OFF:
			{
				//Restore power to LEDs
				pwrMgtSetLEDPwr(APA_ALL_STRIPS,true);
				fade.cycles=0;
				break;
			}
			case SMODE_STAD_I_LJUS:
			{
				//Reset fade and pulse speeds to normal
				pulse.pixelTime=PULSE_NORMAL_PIXEL_TIME;
				fade.fadeTime=FADE_NORMAL_TIME;
				break;
			}
			case SMODE_WHITE_FADE_RAINBOW_PULSE:
			{
				pulse.colourSeqNum=0;
				if(isPulseMode)
				{
					pulse.pixelTime=PULSE_NORMAL_PIXEL_TIME;
					pulse.pixelsPerIteration=3;
					pulse.ledsMaxPower = 20;
				}
				else
				{
					pulse.ledsMaxPower = 150;
					pulse.mode = LEDSEG_MODE_GLITTER_BOUNCE;
					pulse.pixelTime = 10000;
					pulse.pixelsPerIteration = 15;
				}
				break;
			}
			case SMODE_WHITE_FADE_PAN_PULSE:
			{
				pulse.colourSeqNum=0;
				if(isPulseMode)
				{
					pulse.pixelTime=PULSE_NORMAL_PIXEL_TIME;
					pulse.pixelsPerIteration=3;
					pulse.ledsMaxPower = 20;
				}
				else
				{
					pulse.ledsMaxPower = 150;
					pulse.mode = LEDSEG_MODE_GLITTER_BOUNCE;
					pulse.pixelTime = 10000;
					pulse.pixelsPerIteration = 15;
				}
				break;
			}
			case SMODE_CYAN_TO_PURPLE_NO_PULSE:
			{
				animSeqSetActive(animSequence1,false);
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

		smode++;
		if(smode>=SMODE_NOF_MODES)
		{
			smode=0;
		}
		switch(smode)
		{
			case SMODE_BLUE_FADE_YLW_PULSE:
				animSetModeChange(SIMPLE_COL_BLUE,&fade,LEDSEG_ALL,true,50,200,true);
				fadeAlreadySet=true;
				animLoadLedSegPulseColour(SIMPLE_COL_YELLOW,&pulse,255);
				pulseIsActive=true;
				break;
			case SMODE_CYAN_FADE_YLW_PULSE:
				animSetModeChange(SIMPLE_COL_CYAN,&fade,LEDSEG_ALL,true,50,200,true);
				fadeAlreadySet=true;
				animLoadLedSegPulseColour(SIMPLE_COL_YELLOW,&pulse,255);
				pulseIsActive=true;
				break;
			case SMODE_RED_FADE_YLW_PULSE:
				animSetModeChange(SIMPLE_COL_RED,&fade,LEDSEG_ALL,true,50,200,true);
				fadeAlreadySet=true;
				animLoadLedSegPulseColour(SIMPLE_COL_YELLOW,&pulse,255);
				pulseIsActive=true;
				break;
			case SMODE_YLW_FADE_PURPLE_PULSE:
				animSetModeChange(SIMPLE_COL_YELLOW,&fade,LEDSEG_ALL,true,50,200,true);
				fadeAlreadySet=true;
				animLoadLedSegPulseColour(SIMPLE_COL_PURPLE,&pulse,255);
				pulseIsActive=true;
				break;
			case SMODE_YLW_FADE_GREEN_PULSE:
				animSetModeChange(SIMPLE_COL_YELLOW,&fade,LEDSEG_ALL,true,50,200,true);
				fadeAlreadySet=true;
				animLoadLedSegPulseColour(SIMPLE_COL_GREEN,&pulse,255);
				pulseIsActive=true;
				break;
			case SMODE_GREEN_FADE_PURPLE_PULSE:
				animSetModeChange(SIMPLE_COL_GREEN,&fade,LEDSEG_ALL,true,50,200,true);
				fadeAlreadySet=true;
				//animLoadLedSegPulseColour(SIMPLE_COL_PURPLE,&pulse,255);
				pulseIsActive=false;
				break;
			case SMODE_CYAN_FADE_NO_PULSE:
				animSetModeChange(SIMPLE_COL_CYAN,&fade,LEDSEG_ALL,true,50,200,true);
				fadeAlreadySet=true;
				pulseIsActive=false;
				break;
			case SMODE_YLW_FADE_NO_PULSE:
				animSetModeChange(SIMPLE_COL_YELLOW,&fade,LEDSEG_ALL,true,50,200,true);
				fadeAlreadySet=true;
				pulseIsActive=false;
				break;
			case SMODE_RED_FADE_NO_PULSE:
				animSetModeChange(SIMPLE_COL_RED,&fade,LEDSEG_ALL,true,50,200,true);
				fadeAlreadySet=true;
				pulseIsActive=false;
				break;
			case SMODE_BLUE_TO_RED_NO_PULSE:
				animLoadLedSegFadeBetweenColours(SIMPLE_COL_BLUE,SIMPLE_COL_RED,&fade,200,200);
				animSetModeChange(SIMPLE_COL_NO_CHANGE,&fade,LEDSEG_ALL,false,0,0,false);
				fadeAlreadySet=true;
				pulseIsActive=false;
				break;
			case SMODE_BLUE_TO_RED_GREEN_PULSE:
				animLoadLedSegFadeBetweenColours(SIMPLE_COL_BLUE,SIMPLE_COL_RED,&fade,200,200);
				animSetModeChange(SIMPLE_COL_NO_CHANGE,&fade,LEDSEG_ALL,false,0,0,false);
				fadeAlreadySet=true;
				animLoadLedSegPulseColour(SIMPLE_COL_GREEN,&pulse,255);
				pulseIsActive=true;
				break;
			case SMODE_CYAN_TO_PURPLE_NO_PULSE:
				//Todo: Testing out animation here
				//To not update anything
				fadeAlreadySet=true;
				pulseAlreadySet=true;
				animSeqSetRestart(animSequence1);
				/*animLoadLedSegFadeBetweenColours(SIMPLE_COL_CYAN,SIMPLE_COL_PURPLE,&fade,200,200);
				fade.fadeTime=2*FADE_NORMAL_TIME;
				animSetModeChange(SIMPLE_COL_NO_CHANGE,&fade,LEDSEG_ALL,false,0,0,false);
				fadeAlreadySet=true;
				pulseIsActive=false;*/
				break;
			case SMODE_WHITE_FADE_RAINBOW_PULSE:
				animSetModeChange(SIMPLE_COL_WHITE,&fade,LEDSEG_ALL,false,100,255,true);
				fadeAlreadySet=true;
				pulseIsActive=true;
				pulse.colourSeqLoops = 0;
				pulse.colourSeqNum=PRIDE_COL_NOF_COLOURS;
				pulse.colourSeqPtr=(RGB_t*)coloursPride;
				if(isPulseMode)
				{
					pulse.pixelTime=PULSE_NORMAL_PIXEL_TIME;
					pulse.pixelsPerIteration=2;
					pulse.ledsMaxPower = 250;
					pulse.mode = LEDSEG_MODE_LOOP;
				}
				else
				{
					pulse.ledsMaxPower = 250;
					pulse.pixelTime = 2000;
					pulse.pixelsPerIteration = 10;
				}
				break;
			case SMODE_WHITE_FADE_PAN_PULSE:
				animSetModeChange(SIMPLE_COL_WHITE,&fade,LEDSEG_ALL,false,100,255,true);
				fadeAlreadySet=true;
				pulseIsActive=true;
				pulse.colourSeqLoops = 3;
				pulse.colourSeqNum=PAN_COL_NOF_COLOURS;
				pulse.colourSeqPtr=(RGB_t*)coloursPan;
				if(isPulseMode)
				{
					pulse.pixelTime=PULSE_NORMAL_PIXEL_TIME;
					pulse.pixelsPerIteration=2;
					pulse.ledsMaxPower = 200;
					pulse.mode = LEDSEG_MODE_LOOP;
				}
				else
				{
					pulse.ledsMaxPower = 250;
					pulse.pixelTime = 2000;
					pulse.pixelsPerIteration = 10;
				}
				break;
			case SMODE_WHITE_FADE_ITALY_PULSE:
				animSetModeChange(SIMPLE_COL_WHITE,&fade,LEDSEG_ALL,false,100,255,true);
				fadeAlreadySet=true;
				pulseIsActive=true;
				pulse.colourSeqLoops = 3;
				pulse.colourSeqNum=ITALY_COL_NOF_COLOURS;
				pulse.colourSeqPtr=(RGB_t*)coloursItaly;
				if(isPulseMode)
				{
					pulse.pixelTime=PULSE_NORMAL_PIXEL_TIME;
					pulse.pixelsPerIteration=2;
					pulse.ledsMaxPower = 200;
					pulse.mode = LEDSEG_MODE_LOOP;
				}
				else
				{
					pulse.ledsMaxPower = 250;
					pulse.pixelTime = 2000;
					pulse.pixelsPerIteration = 10;
				}
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
			case SMODE_PRIDE_WHEEL:
			{
				animSeqSetRestart(animSequenceRainbowFade);
				//pulseIsActive=false;
				pulseAlreadySet=true;
				fadeAlreadySet=true;
				break;
			}
			case SMODE_PAN_WHEEL:
			{
				animSeqSetRestart(animSequencePanFade);
				//pulseIsActive=false;
				pulseAlreadySet=true;
				fadeAlreadySet=true;
				break;
			}
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
			case SMODE_STAD_I_LJUS:
			{
				//Special mode for Stad i ljus performance
				stadILjusState=1;
				fade.cycles=0;
				fade.startDir=-1;
				animSetModeChange(SIMPLE_COL_YELLOW,&fade,LEDSEG_ALL,false,100,200,true);
				fadeAlreadySet=true;
				pulseIsActive=false;
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
			if(!partial)
			{
				ledSegSetPulseActiveState(segmentTopFull,false);
				ledSegSetFadeActiveState(segmentTopFull,false);
				displayBattery(1,segmentTopFull,batteryIndicatorStartLed);
			}
			else
			{
				ledSegSetPulseActiveState(segmentBottomFull,false);
				ledSegSetFadeActiveState(segmentBottomFull,false);
				displayBattery(1,segmentBottomFull,batteryIndicatorStartLed);
			}
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
		if(smode==SMODE_STAD_I_LJUS)
		{
			stadILjusState++;
			stadILjusIsLoaded=false;
		}
		else if(smode==SMODE_OFF)
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
			//Todo: Remove later
			ledSegRestart(segmentTest1,true,pulseIsActive);
			ledSegRestart(segmentTest2,true,pulseIsActive);
			ledSegRestart(segmentTest3,true,pulseIsActive);
			ledSegRestart(segmentTest4,true,pulseIsActive);
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
		if(!lightIsActive)
		{
			static bool eyesActive=true;
			if(eyesActive)
			{
				animSetModeChange(SIMPLE_COL_OFF,&fadeEyes,segmentEyes,false,0,0,false);
				eyesActive=false;
			}
			else
			{
				animSetModeChange(SIMPLE_COL_NO_CHANGE,&fadeEyes,segmentEyes,false,0,0,false);
				eyesActive=true;
			}
		}
		else if(animSeqTrigReady(animSequence1))
		{
			animSeqTrigTransition(animSequence1);
		}
		else
		{
			if(!pause)
			{
				ledSegSetFadeActiveState(LEDSEG_ALL,false);
				ledSegSetPulseActiveState(LEDSEG_ALL,false);
				ledSegSetFadeActiveState(segmentEyes,false);
				ledSegSetPulseActiveState(segmentEyes,false);
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
				ledSegSetFadeActiveState(segmentEyes,true);
				ledSegSetPulseActiveState(segmentEyes,pulseIsActive);
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
		case SMODE_STAD_I_LJUS:
		{
			const uint32_t stadILjus2To3=3000;
			const uint32_t stadILjusFinalFade=1000;
			switch (stadILjusState)
			{
				case 1:
				{
					//Do nothing, first default state
					break;
				}
				case 2:	//@First Stad I ljus
				{
					//White glitter in bottom half, fade to stronger yellow
					//When done/waited for a bit, update state
					if(!stadILjusIsLoaded)
					{
						stadILjusTime=systemTime+stadILjus2To3;
						animLoadLedSegPulseColour(SIMPLE_COL_WHITE,&pulse,200);
						pulse.mode = LEDSEG_MODE_GLITTER_LOOP_PERSIST;
						pulse.startDir=1;
						pulse.pixelTime=4000;
						pulse.ledsMaxPower=200;
						pulse.pixelsPerIteration=10;
						//ledSegSetFade(segmentBottomFull,&fade);
						ledSegSetPulse(segmentBottomFull,&pulse);
						ledSegSetPulseActiveState(segmentBottomFull,true);
						stadILjusIsLoaded=true;
					}
					if(systemTime>stadILjusTime)
					{
						stadILjusState=3;
						stadILjusIsLoaded=false;
					}
					break;
				}
				case 3:	//@Automatic from state 2
				{
					//White glitter in top half and head, stronger yellow
					if(!stadILjusIsLoaded)
					{
						stadILjusTime=systemTime+stadILjus2To3;
						animLoadLedSegPulseColour(SIMPLE_COL_WHITE,&pulse,200);
						pulse.mode = LEDSEG_MODE_GLITTER_LOOP_PERSIST;
						pulse.startDir=1;
						pulse.pixelTime=3000;
						pulse.ledsMaxPower=200;
						pulse.pixelsPerIteration=10;
						//ledSegSetFade(segmentTopFull,&fade);
						ledSegSetPulse(segmentTopFull,&pulse);
						ledSegSetPulseActiveState(segmentTopFull,true);
						stadILjusIsLoaded=true;
					}
					break;
				}
				case 4:	//@After "Föds på nytt", during trumpet solo
				{
					//Fade back to yellow, low power. Fade up and down.
					if(!stadILjusIsLoaded)
					{
						pulse.startDir=-1;
						//ledSegSetFade(segmentBottomFull,&fade);
						ledSegSetPulse(segmentBottomFull,&pulse);
						ledSegSetPulseActiveState(segmentBottomFull,true);
						//ledSegSetFade(segmentTopFull,&fade);
						ledSegSetPulse(segmentTopFull,&pulse);
						ledSegSetPulseActiveState(segmentTopFull,true);
						stadILjusIsLoaded=true;
					}
					break;
				}
				case 5: //@When second verse starts
				{
					//Increase power slightly
					if(!stadILjusIsLoaded)
					{
						globalSetting+=2;
						apa102SetDefaultGlobal(globalSetting);
						stadILjusIsLoaded=true;
					}
					break;
				}
				case 6: //@Start of second refrain
				{
					//Go to fade white glitter again, stronger yellow (same as 2). Bottom half.
					//When done/waited for a bit, update state
					if(!stadILjusIsLoaded)
					{
						stadILjusTime=systemTime+stadILjus2To3;
						animLoadLedSegPulseColour(SIMPLE_COL_WHITE,&pulse,200);
						pulse.mode = LEDSEG_MODE_GLITTER_LOOP_PERSIST;
						pulse.startDir=1;
						pulse.pixelTime=4000;
						pulse.ledsMaxPower=300;
						pulse.pixelsPerIteration=10;
						//ledSegSetFade(segmentBottomFull,&fade);
						ledSegSetPulse(segmentBottomFull,&pulse);
						ledSegSetPulseActiveState(segmentBottomFull,true);
						stadILjusIsLoaded=true;
					}
					if(systemTime>stadILjusTime)
					{
						stadILjusState=7;
						stadILjusIsLoaded=false;
					}
					break;
				}
				case 7: //@Automatic from state 6
				{
					//White glitter in top half and head, stronger yellow
					if(!stadILjusIsLoaded)
					{
						stadILjusTime=systemTime+stadILjus2To3;
						animLoadLedSegPulseColour(SIMPLE_COL_WHITE,&pulse,200);
						pulse.mode = LEDSEG_MODE_GLITTER_LOOP_PERSIST;
						pulse.startDir=1;
						pulse.pixelTime=3000;
						pulse.ledsMaxPower=300;
						pulse.pixelsPerIteration=10;
						//ledSegSetFade(segmentTopFull,&fade);
						ledSegSetPulse(segmentTopFull,&pulse);
						ledSegSetPulseActiveState(segmentTopFull,true);
						stadILjusIsLoaded=true;
					}
					break;
				}
				case 8: //@Crescendo!
				{
					//Fade to final colour
					if(!stadILjusIsLoaded)
					{
						stadILjusTime=systemTime+stadILjusFinalFade;
						globalSetting+=1;
						apa102SetDefaultGlobal(globalSetting);
						stadILjusIsLoaded=true;
					}
					if(systemTime>stadILjusTime && globalSetting < 15)
					{
						stadILjusTime=systemTime+stadILjusFinalFade;
						globalSetting+=1;
						apa102SetDefaultGlobal(globalSetting);
					}
					break;
				}
				default:
				{
					stadILjusState=0;
					globalSetting=GLOBAL_SETTING;
				}
			}	//End of internal switch clause in stad i ljus
			break;
		} 	//End of case stad i ljus
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
			ledSegSetLed(segment,startLED+i,0,200,0);
		}
		else
		{
			ledSegSetLed(segment,startLED+i,200,0,0);
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
			//dummyLedTask();
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
