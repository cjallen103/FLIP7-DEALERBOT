#include <Arduino.h>
#include "Config.h"

#pragma region LICENSE
// SPDX-License-Identifier: MIT
// Original code © 2024 Crunchlabs LLC (DEALR Code)
// Refactored April 2025 by WKoA
// Modified August 2025 by Chris Allen
// See LICENSE.txt for full original license text.
#pragma endregion LICENSE

#pragma region LIBRARIES
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
INCLUDED LIBRARIES
All the below, with the exception of NHY3274TH.h, are common external libraries.
NHY3274TH is a custom color sensor that uses a custom library.
*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Servo.h>                // Controls the card-feeding servo.
#include <Wire.h>                 // Enables I2C, which we use for communicating with the 14-segment LED display.
#include <Adafruit_GFX.h>         // Used for the 14-segment display.
#include <Adafruit_LEDBackpack.h> // Used for the 14-segment display.
#include <EEPROM.h>               // Helps us save information to EEPROM, which is like a tiny hard drive on the Nano. This lets us save values even when power-cycling.
#include <avr/pgmspace.h>         // Lets us store values to flash memory instead of SRAM. Filling SRAM completely causes issue with program operation.
#include <NHY3274TH.h>            // A custom library for interacting with the color sensor.

// Other Card Dealer Components
#include "Enums.h"
#include "Game.h"
#include "GameRegistry.h"
#include "Definitions.h"
#include "Faces.h"
#include "ColorNames.h"

#pragma endregion LIBRARIES

#pragma region LIB OBJECTS
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
LIBRARY OBJECT ASSIGNMENTS:
Initialize objects for external libraries.
*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Servo feedCard;                                    // Instantiate a Servo object called "feedCard" for controlling a servo motor.
NHY3274TH sensor;                                  // Instantiate an NHY3274TH object called "sensor" for interfacing with the NHY3274TH color sensor.
Adafruit_AlphaNum4 display = Adafruit_AlphaNum4(); // Instantiate an Adafruit_AlphaNum4 object called "display" for controlling the 14-segment display.

#pragma endregion LIB OBJECTS

#pragma region ENUMERATIONS
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
ANIMATIONS:
The "DisplayAnimation" struct allows for new animations to be made more easily.
*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const DisplayAnimation* currentAnimation = &initialBlinking;
uint8_t currentFrameIndex = 0;
unsigned long lastFrameTime = 0;

#pragma endregion ENUMERATIONS

#pragma region CONSTANTS
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
CONSTANTS:
Fixed values such as motor speeds, timeouts, and default thresholds.
*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// EEPROM VARIABLES
// These variables store the memory addresses of any information that needs to be saved through system reboots
#define EEPROM_VERSION_ADDR 0
#define EEPROM_VERSION 1
#define UV_THRESHOLD_ADDR (TOTAL_COLORS * sizeof(RGBColor) + 2)

// TOOL MENUS INCLUDED
const uint8_t numToolMenus = 4;        // Number of *index positions* for pre-programmed tuning routines (so "number of tool menus" - 1). If you add or subtract one, change this number.
const char toolsMenu[][26] PROGMEM = { // "26" defines the max number of characters you can use in these menu titles.
    "*1-DEAL ONE CARD",             // Deals a single card (useful for debugging card dealing)
    "*2-COLOR TUNER",                  // Place tags under sensor to "reset" color values for each tag
    "*3-UV TUNER",              // Deals 5 cards, and takes the highest reflectance value, adds a buffer, and calls that the "marked card threshold"
    "*4-RESET DEFAULT COLORS",           // Resets color and UV values to factory defaults
    "*5-COLOR SENSOR"
}; 

// STARTING STATES AND STATE UPDATE TAGS:
dealState currentDealState = IDLE;                         // Current state of the dealing interaction, starting with IDLE on boot.
dealState previousDealState;                               // Previous state of the dealing interaction. Necessary for detecting a change in deal states.
displayState currentDisplayState = SCROLL_PLACE_TAGS_TEXT; // Current state of the display, starting with SCROLL_PLACE_TAGS_TEXT animation.
displayState previousDisplayState;                         // Previous state of the display. Necessary for detecting a change in display state.

// MOTOR CONTROL CONSTANTS

const uint8_t highSpeed = 255;   // Default value for "high speed" movement (max is 255).
const uint8_t mediumSpeed = 220; // Default value for "medium speed" movement.
const uint8_t lowSpeed = 180;    // Low speed is a little above half speed. Any lower and torque is so low that the rotation stalls.

const uint8_t flywheelMaxSpeed = 255; // Default to top speed for flywheel motor

// UV SENSOR CONSTANTS
const uint16_t defaultUVThreshold = 11; // Initial UV value indicating a marked card.
const uint8_t numberOfReadings = 6;     // Number of readings of each marked card to establish an average value.

// TIMING CONSTANTS
const unsigned long errorTimeout = 6000;        // For rotations where we should have found a tag, but didn't, we throw an error after this amount of time.
const unsigned long throwExpiration = 4000;     // If, when trying to deal a card, we take longer than this amount of time, throw an error.
const unsigned long reverseFeedTime = 400;      // Amount of time to reverse the feed servo after a deal (successful or unsuccessful).
const unsigned long min360Interval = 1000;      // This variable ensures there's no chance of double-reading the red tag while initializing a tagless deal.

#pragma endregion CONSTANTS

#pragma region GLOBAL VARIABLES
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
GLOBAL VARIABLES
*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// GAMEPLAY VARIABLES
int8_t currentGame = 0;            // Variable for holding the current game being selected.
int8_t previousGame = -1;          // Variable for holding the previous game that was selected (to detect change). We initialize to an impossible number.
int8_t currentToolsMenu = 0;       // Variable for holding the current tools routine being selected.
int8_t previousToolsMenu = -1;     // Variable for holding the previous tools menu selected. We initialize to an impossible number.
uint8_t initialRoundsToDeal = 0;   // Assigned when a game is selected and used as a multiplier to determine how many revolutions to make.
uint8_t remainingRoundsToDeal = 0; // Decrements as we deal out cards each round.
int8_t postCardsToDeal = 52;       // Stores number of cards to deal in the post-game, and decrements per hand.

// FEED SERVO CONTROL
int8_t slideStep = 0;          // These are the steps the feed servo follows when dealing a card (advance, retract, stop).
int8_t previousSlideStep = -1; // Used to detect when slideStep changes. Initialize to an impossible number.

// UV SENSOR VALUES
uint16_t uvReaderValue = 0;                      // Variable for storing reading from UV sensor.
uint16_t storedUVThreshold = defaultUVThreshold; // Value starts as the default value, but can be updated using a built-in tool.

// VARIABLES RELATED TO TIMINGS (DEBOUNCES, TIMEOUTS, AND TAGS)
unsigned long overallTimeoutTag = 0;        // Tag for marking last human interaction. After a while, we can start the blinking screensaver.
unsigned long errorStartTime = 0;           // For logging when we start dealing a card, so we know if too long has elapsed and there's an error.
unsigned long throwStart = 0;               // Tag for when we start dealing a card.
unsigned long retractStartTime = 0;         // Variable for storing when we begin retracting a card during a throw error.
unsigned long lastDealtTime = 0;            // Variable for storing the last time a card was dealt.
unsigned long initializationStart = 0;      // Variable for storing when initialization began, so if we exceed that amount of time we can throw an error.
unsigned long expressionStarted = 0;        // Variable for storing the start time of any given expression
unsigned long adjustStart = 0;              // Tag for when a fine adjustment process starts after seeing a burst of unknown color

// TEXT AND ANIMATION TIMINGS
uint16_t scrollDelayTime = 0;     // Variable for switching between scrolling and waiting intervals.
unsigned long lastScrollTime = 0; // Tag for when we last shifted text over in scrolling animations.
int scrollIndex = -1;             // Start at -1 to hold the first frame longer.
uint8_t messageRepetitions = 0;   // Variable for storing the number of times we have repeated scrolling text.
char message[30];                 // We use "char" to save RAM. This is the max scroll text length, but it can be increased if necessary.
int8_t messageLine = 0;           // For messages that scroll several lines, this variable holds which line we're scrolling.
const char* customFace = "0  0";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
COLOR MANAGEMENT STRUCT AND VARIABLES
The color sensor outputs red, green, blue, and "brightness" (c) values, which we package into a "struct" and use in different functions.
*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// DEFAULT COLOR VALUES (can be updated with onboard color tuning function)
RGBColor colors[TOTAL_COLORS]; // Declare an array of RGBColor objects to store color values for the color tuner.

// VARIABLES FOR FINDING AND DEBOUNCING COLOR READINGS
uint8_t activeColor = 0;                    // This is the color the sensor is currently seeing. There can be some "wobble" as we transition between colors, so this needs processing.
uint8_t previousActiveColor = -1;           // Initialize to a value that is not possible so that activeColor != previousActiveColor on boot
uint8_t stableColor = 0;                    // The stable color detected, which is what we get after processing "activeColor" a bit by averaging it over time.
float totalColorValue = 0;                  // Variable for holding the value of all detected colors (R, G, and B) added together.
const int8_t numSamples = 10;               // Number of samples for averaging color value.
const uint8_t debounceCount = 3;            // Number of consecutive readings to confirm a color. More readings increases precision, but covers more radial distance. Too many readings can exceed tag width.
uint8_t colorBuffer[debounceCount] = { 0 }; // Buffer to store the last few colors.

// COLOR-MANAGING ARRAYS AND VARIABLES
int8_t colorsSeen[TOTAL_COLORS] = { -1 };                 // Array to record colors present, which holds colors seen in their default order.
int8_t colorsSeenIndexValue = 0;                       // Index for colorsSeen array. Ordered the same every time according to RGBColor array.
uint8_t activeColorIndexValue = 0;                     // Variable for holding the index value of the active color.
int8_t totalCardsToDeal = 0;                           // Variable for holding number of cards that should be dealt in tagless game.
int8_t colorStatus[NUM_PLAYER_COLORS];               // -1 means color not seen, 0 or more means "seen" and indicates number of cards dealt to the player of that tag.
int8_t colorLeftOfDealer = 0;                          // To store the color index number for the player left of the dealer, which can change each game.

#pragma endregion GLOBAL VARIABLES

#pragma region STATE MACHINE FLAGS
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
STATE MACHINE FLAGS:
This code uses a state machine to establish what DEALR should be doing at any point (e.g., dealing, advancing, waiting for player decision). Some
of the state machine logic is controlled by the "enums" above, which iterate through these broader states. But there are lots of smaller, specific
states that can be handled by "boolean operators," or simple true/false variables. We toggle certain values "on" and "off" to enable and disable
these states, like whether the game is rigged or not. We also use these bools to set and check other conditions, like which direction DEALR is spinning,
whether or not a deal has been initialized, or whether or not we are checking for marked cards, to name just a few examples. There are *lots* of states
to check, but using bools is an easy way to both set and check different dealing conditions on the fly.
*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

GameRegistry gameRegistry;
Game* currentGamePtr = nullptr;
Flags1 flags1;
Flags2 flags2;
Flags3 flags3;
Flags4 flags4;
Flags5 flags5;

bool stopped = true;                       // Indicates when rotation is stopped.
bool cardInCraw = true;                    // "Card-In-Craw" means there is a card in the mouth of the DEALR. The IR sensor reads "high" when not active and "low" when a card is in the beam.
bool previousCardInCraw = true;            // Flag for holding previous card-in-craw state


#pragma endregion STATE MACHINE FLAGS

#pragma region FUNCTION PROTOTYPES
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
FUNCTION PROTOTYPES
Function Prototypes let a program know what functions are going to be defined later on. This isn't always necessary in every IDE, but it's good practice, and
can serve as a kind of table of contents for what to expect later.
*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Setup Functions
void startRoutine(); // Routine that runs in "setup". Includes motor test and initial blink animation.

// State Machine Functions
void checkState();                         // Function for constantly checking the current dealing state of DEALR.
void handleIdleState();                    // Handles what happens in the "IDLE" dealing state.
void handleAdvancingState();               // Handles how to proceed when advancing from one player to another.
void handleInitializingStateInAdjust();    // Handles when a fine-adjustment is called during initialization to the red tag.
void handleAdvancingStateInAdjust();       // This is where advancing decisions happen. We have detected an "unknown color," then fine-adjusted to see what color it is.
void handleDealingState();                 // Handles what happens when we enter the "dealing" state.
void handleAdvancingOnePlayer();           // This function is used during post-deals when we only want to advance a single player during post-deal.
void handleAwaitingPlayerDecision();       // Displays the correct scroll text during "Awaiting Player Decision" deal state.
void handleResetDealrState();              // Handles what happens when we enter the "reset" state.
//inline void dealAndAdvance();              // A convenient wrapper function for dealing and then advancing

// Helper functions for handleDealingState
void handleStandardGameDecisionsAfterFineAdjust(); // This is where dealing decisions are made after DEALR has fine-adjusted and confirmed the color of a tag.
void handleStandardGameRemainderCards();           // This is where decisions are made regarding "remainder" cards, or cards that are dealt after a main deal completes.
void handlePostDealGameplay();                     // This is where decisions are made during a post-deal, or the cards that are dealt after a main deal completes.
void handleToolsDealing();                         // This is where we handle dealing decisions in DEALR's "tools" subroutines
void handleToolsAdvancingDecisions();              // This is where we decide which direction to advance in during DEALR's "tools" subroutines

// Functions related to dealing cards
void dealSingleCard(uint8_t amount=1);        // Safe wrapper function for cardDispensingActions. Includes some pre- and post-processing steps.
void cardDispensingActions(uint8_t &amount); // The series of actions that deal a single card.
void prepareForDeal();        // The steps that get us ready to deal a single card.

// Functions related to DEALR rotation and color sensing
void initializeToRed();                                      // Function for initializing to the red tag before a deal.
void fineAdjustCheck();                                      // fineAdjustCheck() starts after a "bump" in color is seen by the color sensor. It instructs Motor 2 to backtrack slowly until it can confirm the exact color.
void handleRotationAdjustments();                            // Handles switching directions when doing a "fine adjustment", no matter what direction we were going.
void moveOffActiveColor(bool rotateClockwise);               // Function that moves us to a part of the circle that doesn't have tags in it, like during a card flip operation. Takes a direction to rotate in as an input.
void returnToActiveColor(bool rotateClockwise);              // Function that moves us back onto the active color after having moved off.
void colorScan();                                            // Wrapper function for color scanning functions.
bool checkForColorSpike(uint16_t c, uint16_t blackBaseline); // Bool that checks to see if the color sensor detects a "spike" in color value with respect to the baseline

// Funcations related to gameplay mechanics
void handleFlipCard(); // Moves to an unused area, displays "FLIP", and then deals a card.
void handleGameOver(); // Handles when "game over" has been declared by initiating a reset.

// Buttons and Other Sensor Function Prototypes
void checkButton(int buttonPin, unsigned long& lastPress, int& lastButtonState, unsigned long& pressTime, bool& longPressFlag, uint16_t longPressDuration, void (*onRelease)(), void (*onLongPress)());
void checkButtons();                    // Wrapper function for checkButton. Checks whether or not buttons have been pressed.
void onButton1Release();                // Function for isolating when button one is released.
void onButton1LongPress();              // Function for isolating when button one is long-pressed.
void onButton2Release();                // Function for isolating when button two is released.
void onButton2LongPress();              // Function for isolating when button two is long-pressed.
void onButton3Release();                // Function for isolating when button three is released.
void onButton3LongPress();              // Function for isolating when button three is long-pressed.
void onButton4Release();                // Function for isolating when button four is released.
void onButton4LongPress();              // Function for isolating when button four is long-pressed.
void resetTagsOnButtonPress();          // Convenience function that resets some state machine tags on each button press.
void pollCraw();                        // Checks the IR sensor to see whether or not a card is in the mouth ("craw") of DEALR. Useful for determining whether or not cards have been successfully dealt.
void colorRead(uint16_t blackBaseline); // Function for using the color-reading sensor to detect color underneath it.
uint16_t calculateBlackBaseline();      // Retrieves the RGB value of "black" from EEPROM. We can compare readings against this to quickly detect spikes in brightness indicating tags.
void logBlackBaseline();                // Reads RGB values of "black" for storing to EEPROM.

// Display-related function prototypes
void showGame();                                       // Function for writing the game being selected onto the display.
void showTool();                                       // Function for writing the tools menu being selected onto the display.
void startPreGameAnimation();                          // Starts the pre-game animation, which transitions into the blinking animation.
void startScrollText(const char* text, uint16_t start, // Starts scrolling text. Inputs are: text to scroll, length of time to hold while starting...
    uint16_t delay,
    uint16_t end);                                          // ...scroll frame delay time, and length of time to hold while ending.
void updateScrollText();                                    // Updates scrolling text as we loop.
void stopScrollText();                                      // Interrupts and stops text from scrolling
void displayFace(const char* word);                         // Function for showing a single four-character word (or image) on the display.
void scrollMenuText(const char* text);                      // Helper function that receives text from "showGame()" and "showTool()."
void updateDisplay();                                       // Function that's called when we want to update the display.
void displayErrorMessage(const char* message);              // Displays an error message, then attempts to reset DEALR.
void getProgmemString(const char* progmemStr, char* buffer, // Helper function for progmem.
    size_t bufferSize);
void runAnimation(const DisplayAnimation& animation);

// UI Manipulation Function Prototypes
void advanceMenu();     // Function for progressing when Button 1 (green) has been pressed.
void goBack();          // Function for regressing when Button 4 (red button) has been pressed.
void decreaseSetting(); // Function for what happens when Button 2 (blue) is pressed.
void increaseSetting(); // Function for what happens when Button 3 (yellow) is pressed.
void checkTimeouts();   // Function for checking to see whether device has been idle a long time, and starting the idle animation.

// Motor Control Functions
void slideCard(uint8_t &amount);                                   // Function for sliding a card into the flywheel for dealing.
void rotate(uint8_t rotationSpeed, bool direction); // Function for rotating. Takes speed and direction as inputs.
void rotateStop();                                  // Function for stopping rotation.
void flywheelOn(bool direction);                    // True = "forward"; False = "reverse".
void flywheelOff();                                 // Turns flywheel off.
void switchRotationDirection();                     // Reverses direction of yaw rotation.

// Tools and Their Helper Functions
void colorTuner();                 // Controls the "color tuning" operation that locks down RGB values for specific color tags.
void recordColors(int startIndex); // Helper function for colorTuner().
void uvSensorTuner();              // Controls the "UV tuning" operation that locks down the threshold visible light value for a card to be determined "marked".
void recordUVThreshold();          // Helper function for uvSensorTuner().
void resetEEPROMToDefaults();      // Function for resetting EEPROM values to defaults.

// Error-and-Timeout-handling Functions
void handleThrowingTimeout(unsigned long currentTime); // Handles timeouts while dealing cards.
void handleFineAdjustTimeout();                        // Handles timeout for fine adjustment moves.
void resetFlags();                                     // Resets all state machine flags when called.
void resetColorsSeen();                                // Function used in the "reset" tool to reset colors seen.

// Reading and Writing to EEPROM Functions
void initializeEEPROM();                                 // Function for checking whether EEPROM values were set by the user or are factory defaults.
void writeColorToEEPROM(int index, RGBColor color);      // Used for writing RGB values to EEPROM.
void loadColorsFromEEPROM();                             // Whatever colors have been saved to EEPROM get loaded at startup using this function.
void loadStoredUVValueFromEEPROM(uint16_t& uvThreshold); // Loads stored UV threshold values from EEPROM on boot.
RGBColor readColorFromEEPROM(int index);                 // Helper function used in loadColorsFromEEPROM.
RGBColor getBlackColorFromEEPROM();                      // Lets us check the value of our "baseline" luminance when no tag visible.


#pragma endregion FUNCTION PROTOTYPES

#pragma region SETUP
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
SETUP FUNCTION
In the setup for this build, we make sure our sensors are working and run a few startup routines.
*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
   // if (useSerial) {
       //Serial.begin(115200);
       // Serial.println(F("Beginning HP_DEALR_2_2_4 02/2025"));
    //}

   // if (verbose) {
    //    Serial.print(F("Registered games: "));
    //    Serial.println(gameRegistry.getGameCount());
   // }

    if (sensor.begin()) {
       // if (verbose) {
       //     Serial.println(F("Found NHY3274TH sensor"));
        //}
    } else {
        //if (verbose) {
      //      Serial.println(F("No NHY3274TH sensor found ... check your connections"));
       // }
    }
    delay(300);

    // COLOR SENSOR ATTACHMENT
    /*
  A NOTE ON COLOR SENSING:

  The DEALR robot rotates over colored tags and reads the different colors as it goes in order to know what player it's facing. The more time it
  is able to "read" each tag--i.e. the longer the integration time--the more accurate the results. This creates an interesting dynamic, because we
  also want to rotate as fast as possible in order to deal faster. Accordingly, we must strike a balance between integration time, tag width, and
  rotation speed.

  Default integration time is set to 0x1, which equates to 8ms. It takes DEALR about 4300 ms to make a full revolution at max speed, and the sensor
  covers about 387mm in linear distance in this time, with each tag being about 10.7mm in arc length, or 1/36th of the circle. So we spend a little
  over 120ms rotating over each color, and so should in theory be able to read each tag 15 times. Realistically, as the sensor crosses from black to red,
  for example, the numerical values need to interpolate from full black to full red, so only the middle 10 or so readings will reflect the actual
  color. We take a running average of these readings to get even more accurate results.

  If we jump to the next sensor level, 33ms per reading, we get much greater color accuracy, but are only able to read each tag three times before we've
  zoomed past it. This leads to a much greater chance that we'll fully skip tags. And so we settle for lower color accuracy with faster read times.
  Because accuracy is lower, it benefits us to sometimes perform a check we've called "fineAdjustCheck." When we see a spike in color data ("c" value, or brightness),
  we stop the rotation motors, then *reverse* for a moment to check the color at a slower speed. This helps us bridge the gap between accuracy and speed.
  */

    sensor.begin();                 // Start color sensor
    sensor.setIntegrationTime(0x1); // Sets the integration time of the NHY3274TH sensor. 0x0 = 2ms; 0x1 = 8ms; 0x2 = 33ms; 0x3 = 132ms
    sensor.setGain(0x20);           // Sets gain of color sensor

    // PIN ASSIGNMENTS
    pinMode(MOTOR_1_PIN_1, OUTPUT);      // Assign "Motor_1_pin_1" as an output
    pinMode(MOTOR_1_PIN_2, OUTPUT);      // Assign "Motor_1_pin_2" as an output
    pinMode(MOTOR_2_PIN_1, OUTPUT);      // Assign "Motor_2_pin_1" as an output
    pinMode(MOTOR_2_PIN_2, OUTPUT);      // Assign "Motor_2_pin_2" as an output
    pinMode(MOTOR_1_PWM, OUTPUT);        // Assign "Motor_1_pwm" as the PWM pin for motor 1
    pinMode(MOTOR_2_PWM, OUTPUT);        // Assign "Motor_2_pwm" as the PWM pin for motor 2
    pinMode(STNDBY, OUTPUT);             // Assign the "standby" pin as an output so we can write it HIGH to prevent the module from sleeping
    pinMode(BUTTON_PIN_1, INPUT_PULLUP); // Set "Button_Pin_1" as a pull-up input
    pinMode(BUTTON_PIN_2, INPUT_PULLUP); // Set "Button_Pin_2" as a pull-up input
    pinMode(BUTTON_PIN_3, INPUT_PULLUP); // Set "Button_Pin_3" as a pull-up input
    pinMode(BUTTON_PIN_4, INPUT_PULLUP); // Set "Button_Pin_4" as a pull-up input
    pinMode(CARD_SENS, INPUT);           // Set the card_sens pin as an input
    pinMode(UV_READER, INPUT);           // Set the UV_reader pin as an input
    pinMode(RIG_SWITCH, INPUT);          // Set the rig_switch as an input
    pinMode(LED_BUILTIN, OUTPUT);        // Set the built-in LED on the Nano as an output

    digitalWrite(LED_BUILTIN, LOW);     //turn off built-in LED

    feedCard.attach(FEED_SERVO_PIN); // Attach the FEED_SERVO_PIN servo as a servo object
    digitalWrite(STNDBY, HIGH);      // The standby pin for the motor driver must be HIGH or the board will sleep
    delay(50);

    for (uint8_t i = 0; i < NUM_PLAYER_COLORS; i++) // This for-loop resets each of the "colors seen" to -1, setting us up for card-counting next deal
    {
        colorStatus[i] = -1;
    }

    unsigned long seed = 0; // We do some pseudorandom number generation (PRNG) when chaotically dealing, and this "seed" determines the pseudorandom number starting point.

    seed += analogRead(A7); // Normally we would read an unused analog pin, grabbing its randomly fluctuating voltage in order to augment the randomness of our seed value.
                            // In this case, all our analog pins are in use, so we pick the one that fluctuates the most: the UV sensor pin.
    randomSeed(seed);

    display.begin(0x70);      // Initialize the display with its I2C address.
    display.setBrightness(5); // Brightness can be set between 0 and 7.

    resetColorsSeen();

    //checkIfRigged(); // When starting up, poll rig-switch to see if game is rigged on boot.

    initializeEEPROM();       // This function checks to see whether EEPROM was set by the user, or is factory defaults, and loads those values.
    loadColorsFromEEPROM();   // This function loads the above EEPROM values.
    calculateBlackBaseline(); // Read the color values for black from EEPROM and sum them to create a baseline for the color black.

    loadStoredUVValueFromEEPROM(storedUVThreshold); // If we have never run the UV Tuning tool, the storedUVThreshold will be the default value.

    //if (verbose) {
        //Serial.println(F("Colors loaded from EEPROM are "));
        //printStoredColors();

        //Serial.println(F("UV Values loaded from EEPROM are "));
       // printStoredUVThreshold();
       //printDealtCardsInfo();
    //}

    if (motorStartRoutine) {
        startRoutine(); // The start routine moves each of the motors a little to ensure they're working.
    }

    while (digitalRead(CARD_SENS) == LOW) // This while-loop activates if the card-sensing IR circuit is triggered on boot.
    {
        flags4.errorInProgress = true;
        while (!flags2.scrollingComplete && digitalRead(CARD_SENS) == LOW) {
            displayErrorMessage("EROR TUNE IR"); // If this error activates, there is either a card in the DEALR's mouth, or we need to tune the IR sensor screw.
        }
        flags2.scrollingComplete = false;
        messageRepetitions = 0;
    }
    flags4.errorInProgress = false;

    displayFace(" HI ");
    delay(1000);

    currentDealState = IDLE;
    currentDisplayState = INTRO_ANIM;
    memset(&flags1, 0, sizeof(flags1));
    memset(&flags2, 0, sizeof(flags2));
    memset(&flags3, 0, sizeof(flags3));
    memset(&flags4, 0, sizeof(flags4));
    memset(&flags5, 0, sizeof(flags5));
}

#pragma endregion SETUP

#pragma region LOOP

// MAIN LOOP
void loop() {
    checkState();    // This function checks what state the DEALR is in (idle, dealing, awaiting player input, etc.) and lets the dealr behave accordingly.
    checkButtons();  // This function checks to see if buttons are being pressed
    checkTimeouts(); // This function tracks a few overall time-out circumstances (like, when the DEALR should go to sleep because it's bored!)
}

#pragma endregion LOOP

#pragma region FUNCTIONS

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
GAMEPLAY FLOW AND DEAL STATE HANDLING FUNCTIONS
"checkState" is actually a wrapper function for a series of "handle" functions that control different dealing states.
*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region State Handling

// Always running to make sure DEALR does the right things in the right states.
void checkState() {
    unsigned long currentTime = millis(); // Update time in ms every loop.

    // If we change from one state to another, this block lets us do anything that should only happen once during that transition
    if (currentDealState != previousDealState) {
        flags2.newDealState = true;
        overallTimeoutTag = currentTime; // Every time the dealState changes, update overall timeout tag.
        previousDealState = currentDealState;
    }

    // At any point, we can set "flags3.gameOver" to "true" and the handleGameOver function will help us exit cleanly.
    if (flags3.gameOver) {
        handleGameOver();
    }

    // At any point, we can set "flags4.errorInProgress" to "true" and enter into the RESET_DEALR state.
    if (flags4.errorInProgress) {
       // if (verbose) {
       //     Serial.println(F("Error in progress (main loop)."));
       // }
        flags4.errorInProgress = false;
        currentDealState = RESET_DEALR;
        updateDisplay();
    }

    switch (currentDealState) {
        case DEALING: // DEALING handles the process of dealing one or more cards.
            handleDealingState();
            break;

        case ADVANCING: // ADVANCING handles movement from one color tag to the next.
            handleAdvancingState();
            break;

        case INITIALIZING: // INITALIZING handles moving to "red" when starting deal.
            initializeToRed();
            break;

        case IDLE: // IDLE handles what happens when card dealer resets, is in a menu, or isn't in use.
            handleIdleState();
            break;

        case AWAITING_PLAYER_DECISION: // APD handles what happens when we're waiting for a player to make a decision.
            handleAwaitingPlayerDecision();
            break;

        case RESET_DEALR:
            handleResetDealrState(); // Handles resetting state flags when exiting a game or dealing with an error.
            break;

    }
}

void startRoutine() {
    // This routine both tests the motors and also retracts any card that might be in the craw (the mouth of the dealer)
    analogWrite(MOTOR_1_PWM, 255);      // Set flywheel PWM to high speed.
    analogWrite(MOTOR_2_PWM, lowSpeed); // Set yaw motor to low speed (give a small wiggle to show the motor works).
    digitalWrite(MOTOR_1_PIN_1, HIGH);  // Retract with flywheel.
    feedCard.write(30);                 // Retract with feed wheel.
    delay(500);                         // Wait a half second.
    feedCard.write(90);                 // Stop feed motor.
    digitalWrite(MOTOR_1_PIN_1, LOW);   // Stop flywheel.
    delay(100);                         // Wait a tenth of a second.
    digitalWrite(MOTOR_2_PIN_1, HIGH);  // Rotate a little clockwise (CW).
    delay(300);                         // Wait three tenths of a second.
    digitalWrite(MOTOR_2_PIN_1, LOW);   // Rotate a little counter-clockwise (CCW).
    digitalWrite(MOTOR_2_PIN_2, HIGH);
    delay(300);
    digitalWrite(MOTOR_2_PIN_2, LOW); // Turn yaw motor off.
    delay(100);
}

void handleDealingState() {
    // Handles what happens when we enter the "dealing" state.

    if (flags3.postDeal) {
        handlePostDealGameplay(); // In this version of the code we don't distinguish between rigged and unrigged post-deals.
        return;
    }

    if (!flags1.dealInitialized) {
        // If we command a "deal," but we're not initialized, initialize to red first.
        initializeToRed();
        return;
    }

    if (flags1.dealInitialized && !flags1.cardDealt) {
        // If we command a "deal" and are initialized but haven't yet dealt a card, deal a card.
        if (flags4.toolsMenuActive) {
            // If we're using one of the tools menu tools, handle that separately.
            handleToolsDealing();
        } else {
            dealSingleCard();
        }
    }

    if (flags1.dealInitialized && flags1.cardDealt) {
        flags1.cardDealt = false;

        if (flags4.toolsMenuActive) {
            // If toolsMenu is active, we will never advance to either rigged or straight game dealing operations
            handleToolsAdvancingDecisions();
            return;
        }
        currentDealState = ADVANCING;
    }
}

// Deal a single card, by default. Can deal multiple if a number is passed.
void dealSingleCard(uint8_t amount) {
    static uint8_t consecutiveDeals = 0; // Static variable to track consecutive deals. In some circumstances we can deal several cards in a row, but may want to limit how many.

    if (currentDisplayState == LOOK_LEFT || currentDisplayState == LOOK_RIGHT) {
        currentDisplayState = LOOK_STRAIGHT;
        updateDisplay();
    }

    // When "chaotically dealing" in rigged games, we can deal several cards in a row, but want to avoid dealing more than three in a row (suspicious).
    if (consecutiveDeals < 3) {
        while (!flags1.cardDealt) {
            cardDispensingActions(amount);
            if (flags4.errorInProgress) {
                return;
            }
        }
        flywheelOff();

    } else {
        // Serial.println(F("Consecutive deal limit reached. ADVANCING."));
        consecutiveDeals = 0;
        currentDealState = ADVANCING;
        return;
    }
}

// This is the series of things that actually have to happen to dispense a single card.
// Takes in a reference number how many cards to print
void cardDispensingActions(uint8_t &amount) {
    unsigned long currentTime = millis();

    // If there's an error in progress, get us out of the dispensing function.
    // if (flags4.errorInProgress) {
    //     // Serial.println("Error in progress!");
    //     return;
    // }

    // If we're not yet throwing a card, fire up the motors that allow us to throw a card.
    if (!flags1.throwingCard) {
        prepareForDeal();
    } else {
        // for (uint8_t i = amount; i > 0; i--) {
            pollCraw();  // Continuously poll the craw (card-sensing IR sensor) to see if card goes through.
            slideCard(amount); // If slideCard executes fully, flags1.cardDealt = true
        // }

        if (currentDisplayState != CUSTOM_FACE && currentTime - expressionStarted > expressionDuration && !flags5.handlingFlipCard) {
            // Display DEALR's struggling face for at least "expressionDuration" amount of time
            stopScrollText();
            currentDisplayState = STRUGGLE;
            updateDisplay();
        }
    }

    handleThrowingTimeout(currentTime); // If we're trying to throw a card but too much time elapses, throw an error and exit to a main menu.
}

// Turns on the flywheel
void prepareForDeal() {
    unsigned long currentTime = millis();

    flags1.throwingCard = true; // Set flags1.throwingCard tag to "true".
    flywheelOn(true);    // Run flywheel forward.

    delay(100);               // Time for flywheel to speed up.
    flags3.cardLeftCraw = false;     // Flag for whether or not the card has exited the DEALR's mouth.
    throwStart = currentTime; // Tag the start of the throw to track deal time-out.
    slideStep = 0;            // If starting new deal, reset feed motor switch case step to 0.
    previousSlideStep = -1;   // Reset previousSlideStep to be different from slideStep.
}

void initializeToRed() // When we start dealing, we don't know where we are, so we rotate until we find "red" first.
{
    unsigned long currentTime = millis();

    if (!flags1.rotatingCCW && !flags5.adjustInProgress) // If we're not rotating CCW AND we're not fine-adjusting, start rotating top speed CCW.
    {
        rotate(mediumSpeed, CCW);
        initializationStart = currentTime; // Mark the start of our initialization state with a timestamp.
    }

    colorScan();

    if (flags2.baselineExceeded && !flags5.adjustInProgress) // If color spikes, run a check to see what color we saw. Schedule this by setting flags5.adjustInProgress to true.
    {
        adjustStart = millis();
        flags5.adjustInProgress = true;
    }

    if (flags5.adjustInProgress == true) // Here is where we check to see what color we actually saw.
    {
        fineAdjustCheck();
    }

    if (currentTime - initializationStart > errorTimeout) // If it takes too long for us to initialize, throw and error.
    {
        rotateStop();
        errorStartTime = currentTime;
        currentDisplayState = ERROR;
        currentDealState = IDLE;
        updateDisplay();
    }
}

// Executes when currentDealState = ADVANCING.
void handleAdvancingState() {
    // In this block we can do anything we only want to do each time we enter the "advancing" state from a different state
    if (flags2.newDealState) {
        flags5.adjustInProgress = false;
        flags2.newDealState = false;
        if (activeColor != 0) {
            previousActiveColor = activeColor; // As we start moving away from a color, log it as the "previous color" so we can track the difference as we advance.
        }

        if (!flags4.postDealRemainderHandled && !flags3.postDeal) { // && !separatingCards && !shufflingCards) {
            // If there are no more rounds to deal, set "flags3.postDeal" to true and enter into that state.
            if (remainingRoundsToDeal == 0) {
                flags3.postDeal = true;
                flags4.rotatingBackwards = false; // In post-deal we always rotate forwards, so in any mode that has us rotating backwards, switch to forward rotation.
            }
        }

        if (flags4.rotatingBackwards) {
            moveOffActiveColor(CCW);
        } else {
            moveOffActiveColor(CW);
        }
    }

    // While we're seeing black and not adjusting, rotate at high speed until we hit the next color spike.
    if (activeColor == 0 && !flags5.adjustInProgress) {
        if (flags4.rotatingBackwards) {
            rotate(highSpeed, CCW);
        } else {
            rotate(highSpeed, CW);
        }
    }

    colorScan();

    // If color spikes, run fineAdjustCheck. This "adjustment" helps us reverse and re-check the color in case we accidentally zoomed past the tag.
    if (flags2.baselineExceeded && !flags5.adjustInProgress) {
        adjustStart = millis();
        flags5.adjustInProgress = true;
    }

    if (flags5.adjustInProgress == true) {
        fineAdjustCheck(); // This fineAdjustCheck makes sure we correctly read the colored tag.
        delay(10);         // Slight smoothing delay
    }
}

// fineAdjustCheck() starts after a bump in color is seen. It instructs Motor 2 to backtrack until it confirms the color.
void fineAdjustCheck() {
    unsigned long currentTime = millis(); // Update time

    if (!flags2.fineAdjustCheckStarted) // the first time we enter fineAdjust, set adjustStart = to millis()
    {
        adjustStart = currentTime;
        flags2.fineAdjustCheckStarted = true;
        if (flags1.rotatingCCW) {
            rotate(lowSpeed, CCW);
        } else {
            rotate(lowSpeed, CW);
        }
    }

    while (activeColor < 1) // While we're seeing no tags (black), rotate at low speed to detect what color we saw spike.
    {
        colorScan();
        handleRotationAdjustments(); // Handles which direction we correct towards.
        handleFineAdjustTimeout();
    }

    rotateStop(); // At this point we have a stable active color. We stop and then make decisions about whether or not we deal a card, depending on game states.

    if (!flags1.dealInitialized) {
        handleInitializingStateInAdjust(); // If we're still in the initializing phase before a deal, this handles decision-making after the fine-adjust.
    }

    else {
        handleAdvancingStateInAdjust(); // If we're in either a main deal or a tools function, this handles decision-making after the fine-adjust.
    }
}

void handleInitializingStateInAdjust() // Handles when a fine-adjustment is called during initialization to the red tag.
{
    if (activeColor > 1 && colorStatus[activeColor - 1] == -1) // If we're seeing a non-red color that has not yet been seen (a *new, non-red color*)...
    {
        adjustStart = millis();
        while (activeColor != 0) // Active color wasn't red, but we're looking for red to initialize! So we keep rotating, first until we hit black again, and then we carry on.
        {
            colorScan();
            rotate(highSpeed, CCW);
        }
        flags2.fineAdjustCheckStarted = false;
        flags5.adjustInProgress = false;
        flags1.correctingCW = false;
        flags1.correctingCCW = false;
    } else if (activeColor == 1) // At this point we have stabilized on the red tag. Flip the flags1.dealInitialized tag to "true" so we can carry on to the main deal.
    {
        // Serial.println(F("Deal Initialized!"));
        previousActiveColor = activeColor;
        rotateStop();
        flags1.dealInitialized = true;
        adjustStart = millis();
        flags2.fineAdjustCheckStarted = false;
        flags5.adjustInProgress = false;
        flags1.correctingCW = false;
        flags1.correctingCCW = false;
        resetColorsSeen();           // This is probably redundant since we should have reset colorsSeen before the deal.
        totalCardsToDeal = 0;        // Reset cards-to-deal to zero.
        flags3.notFirstRoundOfDeal = false; // We have reset and are initialized and about to commence the first round of a deal.
        currentDealState = ADVANCING; // In most cases, after initializing to red, this line switches us into the ADVANCING state so we move off the red tag to the player left of dealer.
    }
}

// This function handles the decision-making on how we advance after a color has been confirmed following the fineAdjustCheck process.
void handleAdvancingStateInAdjust() {
    flags5.adjustInProgress = false;       // Resets a tag used in the fine adjustment operation
    flags2.fineAdjustCheckStarted = false; // Resets a tag used in the fine adjustment operation
    flags1.correctingCW = false;           // Resets a tag used in the fine adjustment operation
    flags1.correctingCCW = false;          // Resets a tag used in the fine adjustment operation

    if (flags3.advanceOnePlayer) {
        handleAdvancingOnePlayer(); // If we were only supposed to advance one player during a post-deal, we run this function.
        return;
    }

    if (activeColor == 1 && previousActiveColor == 1 && !flags3.postDeal) // && !taglessGame     If we've done a full circle and hit red a second time in a row, we know we're missing tags! Throw an error.
    {
        flags4.errorInProgress = true;
        while (!flags2.scrollingComplete) {
            displayErrorMessage("EROR NEED TAGS");
        }
        flags2.scrollingComplete = false;
        messageRepetitions = 0;
        flags4.fullExit = true;
        currentDealState = RESET_DEALR;
        return;
    }
    handleStandardGameDecisionsAfterFineAdjust();
}

void handleRotationAdjustments() // This function handles switching directions when doing a "fine adjustment", no matter what direction we were going.
{
    if (flags1.rotatingCW && !flags1.correctingCW && !flags1.correctingCCW) // If we were spinning CW when fineAdjustCheck was called, and we were not already making a fine adjust correction...
    {
        rotateStop(); // Stop the rotation.
        delay(100);
        flags1.correctingCW = false;
        flags1.correctingCCW = true;
        rotate(lowSpeed, CCW);                                 // Travel counter-clockwise at low speed. Elsewhere we also read the color sensor to get a more accurate reading.
    } else if (flags1.rotatingCCW && !flags1.correctingCW && !flags1.correctingCCW) // If we were spinning CCW when fineAdjustCheck was called, and we were not already correcting...
    {
        rotateStop(); // Stop the rotation.
        delay(100);
        flags1.correctingCCW = false;
        flags1.correctingCW = true;
        rotate(lowSpeed, CW); // Travel clockwise at low speed. Elsewhere we also read the color sensor to get a more accurate reading.
    }
}

// This function handles decision-making in non-rigged games after a color has been confirmed.
void handleStandardGameDecisionsAfterFineAdjust() {

   // if (verbose) {
   //     Serial.print(F("Handling standard game decisions after fine adjust. Active color: "));
   //     Serial.println(activeColor);
   // }

    // --- Reset Flags After Adjustment ---
    flags5.adjustInProgress = false;
    flags2.fineAdjustCheckStarted = false;
    flags1.correctingCW = false;
    flags1.correctingCCW = false;
    rotateStop(); // Ensure stopped after fine adjust

    // --- Logic for Detecting Round Completion ---
    // In standard games, a "round" is completed when we deal one card to each player tag.
    // We detect the end of a round by reaching the player tag *left of the dealer* (which is the first tag seen after Red)
    // after the very first round has been dealt.
    if (activeColor == 1) // If we're seeing the red tag...
    {
        if (colorStatus[activeColor - 1] == -1) // ...and this is the first time we've seen red in this deal sequence...
        {
            colorStatus[activeColor - 1] = 0; // Mark red as seen (value doesn't matter much in standard games)
            flags3.notFirstRoundOfDeal = true;       // Indicate we are now in the second or later round.
            //if (verbose) Serial.println(F("Reached red tag again (not first round)."));
        }
        // Note: In standard games, we don't deal to red first. The ADVANCING state moves us off red
        // to the player left of the dealer immediately after initialization.
    } else if (activeColor > 1) // If we're seeing a non-red tag...
    {
        if (!flags5.playerLeftOfDealerIdentified) // ...and we haven't identified the player left of dealer yet...
        {
            flags5.playerLeftOfDealerIdentified = true;
            colorLeftOfDealer = activeColor; // This is the first non-red tag seen after initialization to red.
          //  if (verbose) {
           //     Serial.print(F("Identified player left of dealer: color "));
           //     Serial.println(colorLeftOfDealer);
          //  }
        }

        // Original logic for War player count check - might be better in setup/tag scan.
        // Keeping it here for now as per original code structure.


        // Check if we've completed a full round by reaching the player left of dealer again
        if (activeColor == colorLeftOfDealer && flags3.notFirstRoundOfDeal) {
           // if (verbose) Serial.println(F("Completed a round (reached player left of dealer again)."));
            remainingRoundsToDeal--; // Decrement the number of rounds remaining to deal in the main deal.

           // if (verbose) {
           //     Serial.print(F("Rounds remaining in main deal: "));
          //      Serial.println(remainingRoundsToDeal);
           // }

            // --- Check if Main Deal is Complete ---
            if (remainingRoundsToDeal == 0 && !flags3.postDeal) {
               // if (verbose) Serial.println(F("Main deal complete. Entering post-deal phase."));
                flags3.postDeal = true; // Transition to the post-deal phase.

                // Notify the current game object that the main deal is finished
                if (currentGamePtr) {
                   // if (verbose) Serial.println(F("Notifying game object: onMainDealEnd()"));
                    currentGamePtr->_onMainDealEnd(); // Let the game handle specific end-of-main-deal logic.
                } else {
                   // if (verbose) Serial.println(F("WARN: Main deal finished but currentGamePtr is null."));
                }

                // Handle "remainder" cards like a flip card (e.g., Crazy Eights, Rummy)
                // This check uses the game object's definition.
                if (!flags3.gameOver && currentGamePtr && currentGamePtr->requiresFlipCard()) {
                   // if (verbose) Serial.println(F("Game requires a flip card. Handling flip..."));
                    handleFlipCard(); // Core function to perform the flip sequence (moves off tag, displays FLIP, deals, returns)
                    // Note: handleFlipCard() is expected to set flags4.postDealRemainderHandled = true; upon completion.
                    // It might also change the display state, but we'll set the DealState below.

                    // After the flip sequence, the next state is typically AWAITING_PLAYER_DECISION
                    if (!flags3.gameOver) { // Ensure game hasn't somehow ended during the flip
                        currentDealState = AWAITING_PLAYER_DECISION;
                      //  if (verbose) Serial.println(F("Flip handled. Transitioning to AWAITING_PLAYER_DECISION."));
                        return; // Exit this function as state is decided and changed
                    }
                } else if (!flags3.gameOver) {
                    // Main deal is over, no flip card needed, or game is over.
                    // If not game over, transition to AWAITING_PLAYER_DECISION for the post-deal phase.
                   // if (verbose) Serial.println(F("Main deal complete, no flip card. Transitioning to AWAITING_PLAYER_DECISION."));
                    flags4.postDealRemainderHandled = true; // Mark remainder handling complete (because there was no remainder to handle)
                    currentDealState = AWAITING_PLAYER_DECISION;
                    return; // Exit this function as state is decided and changed
                }
            }
        }
    }

    // --- Decide Next State If Not Already Changed ---
    // If we reached here, the state hasn't been set to AWAITING_PLAYER_DECISION or ERROR yet.
    // This means either the main deal is still ongoing, OR it's the post-deal
    // phase and we are currently dealing a remainder card (like the flip card).

    // The condition `!flags3.postDeal || !flags4.postDealRemainderHandled` means:
    // 1. We are NOT in the post-deal phase (main deal is ongoing), OR
    // 2. We ARE in the post-deal phase, BUT the remainder (like the flip card) has NOT been handled yet.
    if (!flags3.postDeal || !flags4.postDealRemainderHandled) {
       // if (verbose) Serial.println(F("Main deal ongoing or remainder needs handling. Transitioning to DEALING."));
        currentDealState = DEALING; // Proceed to deal the card at the current tag.
    }
    // If flags3.postDeal is true AND flags4.postDealRemainderHandled is true, and game is not over,
    // the state should have already been set to AWAITING_PLAYER_DECISION above.
    // If flags3.gameOver is true, checkState() loop will handle the reset.
}

void handleStandardGameRemainderCards() // This function handles how community cards are distributed after a main deal.
{
    if (currentGamePtr && currentGamePtr->requiresFlipCard()) // Crazy Eights and Rummy have a card that is flipped after the main deal.
    {
        handleFlipCard(); // This function handles the dealing of an extra card and the populating of the word "FLIP" on the display.
    } else {
        flags4.postDealRemainderHandled = true; // The post-deal remainder refers specifically to "community cards" that are dealt after the main deal.
    }
}

// This function handles the post-deal portion of games that do not end after the main deal.
void handlePostDealGameplay() {
        flags5.postDealStartOnRed = false;
        while (activeColor < 2 && previousActiveColor != 1) // Return to non-red color to the left of red:
        {
            colorScan();
            rotate(mediumSpeed, CW);
        }
        rotateStop();

    if (!flags4.postDealRemainderHandled) // If we haven't flipped a card for Crazy Eights or Rummy, handle this remainder. Otherwise, mark remainder as handled.
    {
        handleStandardGameRemainderCards();
    } else {
        currentDealState = AWAITING_PLAYER_DECISION;
    }
}

// inline void dealAndAdvance() // This is a convenient wrapper function for dealing a card and then advancing.
// {
//     dealSingleCard();
//     currentDealState = ADVANCING;
// }

void handleToolsDealing() // This function is specifically *only* dealing, not advancing, which is why it's comparatively simple.
{
    if (currentToolsMenu == 0) // If we're using the "deal single card tool" and are in the "dealing" state, simply deal a card.
    {
        dealSingleCard();
    }
}

void handleToolsAdvancingDecisions() // This function deals with "advancing" decisions when using the first tools.
{
    if (currentToolsMenu == 0) {
        displayFace("DELT");
        delay(1000);
        currentDealState = IDLE;
        currentDisplayState = SELECT_TOOL;
        flags3.insideDealrTools = false;
        updateDisplay();
    }
}

void handleResetDealrState() {
    // Handles what happens when we enter the "reset" state.

    currentGamePtr = nullptr;
    resetFlags();
    if (flags4.fullExit) // In full exit, we exit all the way to the intro screen.
    {
        currentGame = 0;
        currentToolsMenu = 0;
        displayFace("EXIT");
        delay(1000);
        flags3.buttonInitialization = false;
        flags4.toolsMenuActive = false;
        if (scrollInstructions) {
            currentDisplayState = SCROLL_PLACE_TAGS_TEXT;
        } else {
            flags2.initialAnimationComplete = false;
            currentDisplayState = INTRO_ANIM;
        }
        flags4.fullExit = false;
    } else if (flags4.gamesExit) // In a "games exit," we just exit to the select game menu.
    {
        currentToolsMenu = 0;
        displayFace("GAME");
        delay(1000);
        flags3.buttonInitialization = true;
        flags4.toolsMenuActive = false;
        currentDisplayState = SELECT_GAME;
        flags4.gamesExit = false;
    } else if (flags4.toolsExit) // In a "tools exit," we just exit to the select tool menu.
    {
        currentGame = 0;
        displayFace("TOOL");
        delay(1000);
        flags3.buttonInitialization = true;
        flags4.toolsMenuActive = true;
        currentDisplayState = SELECT_TOOL;
        flags4.toolsExit = false;
    }
    currentDealState = IDLE;
    updateDisplay();
}

void handleIdleState() // Handles what happens in the "IDLE" dealing state.
{
    if (flags2.newDealState == true) {
       // if (verbose) {
        //    Serial.println(F("Entered IDLE state from other state."));
        //}
        flags2.newDealState = false;
        flywheelOff();
        if (!stopped) {
            rotateStop();
        }
        if (flags1.dealInitialized == true) {
            flags1.dealInitialized = false; // Reset initialization (re-initialize each game).
        }

        resetColorsSeen();
        flags3.postDeal = false;
        flags3.notFirstRoundOfDeal = false;
        postCardsToDeal = 52;
        flags1.cardDealt = false;
        flags1.numCardsLocked = false;
        flags1.correctingCW = false;
        flags1.correctingCCW = false;
    }

    updateDisplay();

    if (slideStep != 0) // Ensures that the feed servo is primed to deal a card.
    {
        previousSlideStep = -1;
        slideStep = 0;
    }
}

void handleGameOver() // Handles when "game over" has been declared by initiating a reset.
{
    moveOffActiveColor(CW); // Rotate clockwise
    currentGamePtr = nullptr;
    flags3.gameOver = false;
    flags4.toolsMenuActive = false; // Switching to select game menu. Deactivating tool menu.
    flags4.gamesExit = true;
    currentDealState = RESET_DEALR;
    currentDisplayState = SELECT_GAME;
    updateDisplay();
}

// Function that moves DEALR off the active tag, ideally into an empty portion of the circle.
void moveOffActiveColor(bool rotateClockwise) {
    while (activeColor != 0) {
        if (flags4.errorInProgress) {
            return;
        }
        colorScan();
        if (rotateClockwise) {
            rotate(lowSpeed, CW);
        } else {
            rotate(lowSpeed, CCW);
        }
    }
    rotateStop();
}

void returnToActiveColor(bool rotateClockwise) // Function that returns us to last active color.
{
    while (activeColor == 0) {
        if (flags4.errorInProgress) {
            return;
        }
        colorScan();
        if (rotateClockwise) {
            rotate(lowSpeed, CW);
        } else {
            rotate(lowSpeed, CCW);
        }
    }
    rotateStop();
}

void handleFlipCard() // Moves to an unused area, displays "FLIP", and then deals a card.
{
    flags5.handlingFlipCard = true;
    moveOffActiveColor(CCW);
    delay(20);
    currentDisplayState = FLIP;
    updateDisplay();
    dealSingleCard();
    flags1.cardDealt = false;
    flags4.postDealRemainderHandled = true;
    returnToActiveColor(CW);
    flags5.handlingFlipCard = false;
}

// This function is used during post-deals when we only want to advance a single player at a time.
void handleAdvancingOnePlayer() {
    while (activeColor < 1) // While we're looking at black, rotate and scan.
    {
        rotate(mediumSpeed, CW);
        colorScan();
    }
    rotateStop();

    adjustStart = millis();
    flags2.fineAdjustCheckStarted = false;
    flags1.correctingCW = false;
    flags1.correctingCCW = false;
    flags5.adjustInProgress = false;

    // If we were told to advance more turns at a time, do that
    currentGamePtr->turnsToAdvance--;
    if (currentGamePtr->turnsToAdvance > 0) {
        // Move off the active color to avoid double registering this one
        moveOffActiveColor(CW);
        // Reset flags to move to next
        previousActiveColor = activeColor;
        flags3.advanceOnePlayer = true;
        currentDealState = ADVANCING;
        return; // Exit early so that this is called again
    }

    flags3.advanceOnePlayer = false;
    currentDealState = AWAITING_PLAYER_DECISION;
}
#pragma endregion State Handling

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
BUTTONS AND SENSOR-HANDLING FUNCTIONS
*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region Buttons and Sensors

void checkButton(int buttonPin, unsigned long& lastPress, int& lastButtonState, unsigned long& pressTime, bool& longPressFlag, uint16_t longPressDuration, void (*onRelease)(), void (*onLongPress)()) // This demanding function handles everything related to button-pushing in DEALR.
{
    int currentButtonState = digitalRead(buttonPin); // Read the current button state.

    if (currentButtonState == LOW) // If the button has been pressed...

    {
        if (lastButtonState == HIGH) // ... and if the button wasn't already being pressed...
        {
            pressTime = millis(); // Record press time.
        }

        if (!longPressFlag && millis() - pressTime >= longPressDuration) // Check for long press.
        {
            longPressFlag = true;
            if (onLongPress != nullptr) {
                resetTagsOnButtonPress();
                onLongPress(); // Trigger the long press action.
            }
        }
    }

    if (lastButtonState == LOW && currentButtonState == HIGH) // Handle button release.
    {
        lastPress = millis();

        if (!longPressFlag && onRelease != nullptr) // If not a long press, trigger the normal release action.
        {
            resetTagsOnButtonPress();
            onRelease(); // Trigger the release action.
            if (currentDisplayState != SCROLL_PLACE_TAGS_TEXT) {
                flags3.buttonInitialization = true; // A button has been pressed, so we know not to start the screensaver blinking animation, except when we've hit "back" all the way to the beginning.
            }
        }
        longPressFlag = false; // Reset the long-press flag after release
    }

    lastButtonState = currentButtonState; // Update the last button state
}

// Go back. TODO: move to differnet region.
void exitButtonAction() {
    if (currentDealState != IDLE) {
        if (flags4.toolsMenuActive) {
            flags4.toolsExit = true;
        } else {
            flags4.gamesExit = true;
        }
        displayFace("EXIT");
        rotateStop();
        flywheelOff();
        delay(1000);
        currentDealState = RESET_DEALR;
    } else {
        goBack();
    }
}

void onButton1Release() {
    // This function handles what happens when we release Button 1 (green). It matters that the action happens on "release" so we can use long-click in some cases.

    // If we're playing a game and awaiting a player decision, do one of these things based on the game:
    if (currentDealState == AWAITING_PLAYER_DECISION) {
        if (currentGamePtr) {
            currentGamePtr->_handleButtonPress(BUTTON_PIN_1); // TODO: I'd like to change this from passing the button pin to passing a button enum of the color
        }
    } else {
        advanceMenu(); // If we're just in one of the menus, Button 1 is the "confirm and advance" button.
    }
}

void onButton2Release() {
    // This function handles what happens when we release Button 2 (blue).
    if (currentDealState == AWAITING_PLAYER_DECISION) {
        if (currentGamePtr) {
            currentGamePtr->_handleButtonPress(BUTTON_PIN_2);
        } 
    }

    if (currentDisplayState == SCREENSAVER || currentDisplayState == SCROLL_PLACE_TAGS_TEXT || currentDisplayState == SCROLL_PICK_GAME_TEXT) {
        advanceMenu();
    } else if (currentDisplayState == SELECT_TOOL || currentDisplayState == SELECT_GAME) {
        increaseSetting();
    }
}

void onButton3Release() {
    // This function handles what happens when we release Button 3 (yellow).

    if (currentDealState == AWAITING_PLAYER_DECISION) {
        if (currentGamePtr) {
            currentGamePtr->_handleButtonPress(BUTTON_PIN_3); // If Yellow had a game function
        }
        // else if (taglessGame) { } // No action for Yellow in tagless await?
    } else if (currentDisplayState == SCREENSAVER || currentDisplayState == SCROLL_PLACE_TAGS_TEXT || currentDisplayState == SCROLL_PICK_GAME_TEXT) {
        advanceMenu();
    } else if (currentDisplayState == SELECT_GAME || currentDealState == IDLE) {
        decreaseSetting();
    }
}

void onButton4Release() {
    // Red button exits, unless we're in a game waiting for player decision, in which case we want to use this button for actions

    if (currentDealState == AWAITING_PLAYER_DECISION) {
        if (currentGamePtr) {
            currentGamePtr->_handleButtonPress(BUTTON_PIN_4); // If Yellow had a game function
        }
        // else if (taglessGame) { } // No action for Red in tagless await?
    } else {
        // Otherwise use it as the exit button
        exitButtonAction();
    }

}

void onButton1LongPress() {
    // Serial.println("Button 1 long-press"); //Currently there is no application of Button 1 long-press.
}

void onButton2LongPress() {
    overallTimeoutTag = millis();
    scrollDelayTime = 0;
    flags2.scrollingStarted = false;
    flags3.scrollingMenu = false;
    messageRepetitions = 0;
    scrollIndex = -1;

    flags4.toolsMenuActive = true;
    currentToolsMenu = 1;
    currentDisplayState = SELECT_TOOL;
    updateDisplay();
    flags3.buttonInitialization = true;
    flags2.blinkingAnimationActive = false;
    flags2.initialAnimationInProgress = false;
    flags2.scrollingComplete = true;
}

void onButton3LongPress() {
    overallTimeoutTag = millis();
    scrollDelayTime = 0;
    flags2.scrollingStarted = false;
    flags3.scrollingMenu = false;
    messageRepetitions = 0;
    scrollIndex = -1;
    flags3.buttonInitialization = true;

    // Serial.println("Shortcut to *3. UV SENSOR TUNER");
    flags4.toolsMenuActive = true;
    currentToolsMenu = 2;
    currentDisplayState = SELECT_TOOL;
    updateDisplay();
    flags3.buttonInitialization = true;
    flags2.blinkingAnimationActive = false;
    flags2.initialAnimationInProgress = false;
    flags2.scrollingComplete = true;
}

void onButton4LongPress() {
    if (currentDealState == AWAITING_PLAYER_DECISION) {
        // Preform exit action with long-press on APD, this allows us to use a single red button press for an action
        exitButtonAction();

    } else {
        overallTimeoutTag = millis();
        scrollDelayTime = 0;
        flags2.scrollingStarted = false;
        flags3.scrollingMenu = false;
        messageRepetitions = 0;
        scrollIndex = -1;

        // Serial.println("Shortcut to *1. DEAL SINGLE CARD");
        flags4.toolsMenuActive = true;
        currentToolsMenu = 0;
        currentDisplayState = SELECT_TOOL;
        updateDisplay();
        flags3.buttonInitialization = true;
        flags2.blinkingAnimationActive = false;
        flags2.initialAnimationInProgress = false;
        flags2.scrollingComplete = true;
    }
}

void resetTagsOnButtonPress() {
    overallTimeoutTag = millis();    // Reset tag for overall timeout every time button is pressed.
    scrollDelayTime = 0;             // Force any scrolling text to start scrolling immediately.
    flags2.scrollingStarted = false;        // Reset flags2.scrollingStarted tag.
    flags3.scrollingMenu = false;           // Reset scrolling menu tag each time button is pressed.
    messageLine = 0;                 // Reset line that's scrolling in cases where several lines scroll.
    messageRepetitions = 0;          // Every time a button is pressed, reset messageRepetitions tag.
    scrollIndex = -1;                // Reset the scroll index.
    flags2.blinkingAnimationActive = false; // Reset the screensaver animation, since a button press indicates a user is present
}

void checkButtons() {
    static unsigned long lastPress1 = millis(), lastPress2 = millis(),
                         lastPress3 = millis(), lastPress4 = millis();
    static int lastButtonState1 = HIGH, lastButtonState2 = HIGH,
               lastButtonState3 = HIGH, lastButtonState4 = HIGH;
    static unsigned long pressTime1 = 0, pressTime2 = 0,
                         pressTime3 = 0, pressTime4 = 0;
    static bool longPress1 = false, longPress2 = false,
                longPress3 = false, longPress4 = false;

    // Call the checkButton function for each button
    checkButton(BUTTON_PIN_1, lastPress1, lastButtonState1, pressTime1, longPress1, 3000, onButton1Release, onButton1LongPress);
    checkButton(BUTTON_PIN_2, lastPress2, lastButtonState2, pressTime2, longPress2, 3000, onButton2Release, onButton2LongPress);
    checkButton(BUTTON_PIN_3, lastPress3, lastButtonState3, pressTime3, longPress3, 3000, onButton3Release, onButton3LongPress);
    checkButton(BUTTON_PIN_4, lastPress4, lastButtonState4, pressTime4, longPress4, 3000, onButton4Release, onButton4LongPress);
}

// Checks the IR sensor to see whether or not a card is in the mouth of DEALR. Useful for determining whether or not cards have been dealt.
void pollCraw() {
    static unsigned long lastDebounceTime = 0; // Time when the sensor was last debounced
    const unsigned long debounceInterval = 40; // Debounce interval in milliseconds

    unsigned long currentTime = millis(); // Update time

    if (currentTime - lastDebounceTime >= debounceInterval) {
        cardInCraw = digitalRead(CARD_SENS);
        if (cardInCraw != previousCardInCraw) // detect a change in craw sensor
        {
            if (cardInCraw == LOW) {
                // Serial.println(F("New card in craw!"));
                flags3.cardLeftCraw = false;
                flags1.cardDealt = false;
            } else // If cardInCraw is newly pulled high (i.e. seeing no cards)
            {
                flags3.cardLeftCraw = true;
                // Serial.println(F("Card just left craw."));
            }
            previousCardInCraw = cardInCraw;
        }
        lastDebounceTime = currentTime;
    }
}

// Wrapper function for grabbing the black value from EEPROM and comparing it to color reading data from colorRead.
void colorScan() {
    uint16_t blackBaseline = calculateBlackBaseline(); // Retrieve the stored brightness of "no tag" (i.e. black) in order to compare color spikes.
    colorRead(blackBaseline);                          // Read color every loop (with respect to the brightness of "black") as we advance towards the next color.
}

// Checks the color sensing board to see what color we're looking at, and assigns that to be "activeColor".
void colorRead(uint16_t blackBaseline) {
    static uint8_t stableColorCount = 0; // Counter to track stability of color readings
    static uint8_t bufferIndex = 0;      // Index for the circular buffer

    uint16_t r, g, b, c;
    sensor.getRawData(&r, &g, &b, &c);
    totalColorValue = r + g + b;

    // Serial.print("Red: ");
    // Serial.print(r);
    // Serial.print(" Green: ");
    // Serial.print(g);
    // Serial.print(" Blue: ");
    // Serial.print(b);
    // Serial.print(" White: ");
    // Serial.println(c);

    flags2.baselineExceeded = checkForColorSpike(c, blackBaseline);

    float normalizedR = r / totalColorValue * 255.0;
    float normalizedG = g / totalColorValue * 255.0;
    float normalizedB = b / totalColorValue * 255.0;

    float distances[TOTAL_COLORS];
    for (uint8_t i = 0; i < TOTAL_COLORS; i++) {
        distances[i] = (normalizedR - colors[i].r) * (normalizedR - colors[i].r) +
            (normalizedG - colors[i].g) * (normalizedG - colors[i].g) +
            (normalizedB - colors[i].b) * (normalizedB - colors[i].b);
    }

    uint8_t closestColor = 0;
    float minDistance = distances[0];
    for (uint8_t i = 1; i < TOTAL_COLORS; i++) {
        if (distances[i] < minDistance) {
            minDistance = distances[i];
            closestColor = i;
        }
    }

    colorBuffer[bufferIndex] = closestColor;
    bufferIndex = (bufferIndex + 1) % debounceCount;

    bool isStable = true;
    for (uint8_t i = 0; i < debounceCount; i++) {
        if (colorBuffer[i] != closestColor) {
            isStable = false;
            break;
        }
    }

    if (isStable) {
        stableColor = closestColor;
        stableColorCount++;
    } else {
        stableColorCount = 0;
    }

    if (stableColorCount >= debounceCount) // If we're about to update activeColor
    {
        activeColor = stableColor;
        // Serial.print(F("Active color is "));
        // Serial.println(activeColor);
        delay(10);
    }
}

bool checkForColorSpike(uint16_t c, uint16_t blackBaseline) {
    if (!flags2.baselineExceeded && !flags5.adjustInProgress && float(c) >= float(blackBaseline) * 1.6) {
        // Serial.println(F("Baseline exceeded!"));
        flags2.baselineExceeded = true;
    } else if (flags2.baselineExceeded && float(c) < float(blackBaseline) * 1.6) {
        // Serial.println(F("Back below baseline."));
        flags2.baselineExceeded = false;
    }
    return flags2.baselineExceeded;
}

uint16_t calculateBlackBaseline() // Retrieves the RGB value of "black" from EEPROM
{
    static uint16_t blackBaseline = 0;
    static bool isCalculated = false;

    if (!isCalculated) // If we've never used the Color Tag Tuning Tool, set the default C value of "black" from EEPROM
    {
        RGBColor blackColor = getBlackColorFromEEPROM();
        blackBaseline = blackColor.avgC;
        isCalculated = true;
    }
    return blackBaseline;
}

void logBlackBaseline() // Reads RGB values of "black" for storing to EEPROM
{
    uint32_t totalR = 0, totalG = 0, totalB = 0, totalC = 0;
    for (int j = 0; j < numSamples; j++) {
        uint16_t r, g, b, c;
        sensor.getRawData(&r, &g, &b, &c);
        totalR += r;
        totalG += g;
        totalB += b;
        totalC += c;
        delay(10); // Small delay between samples
    }

    uint16_t avgR = totalR / numSamples;
    uint16_t avgG = totalG / numSamples;
    uint16_t avgB = totalB / numSamples;
    uint16_t avgC = totalC / numSamples;

    if (avgC > 255) // Total brightness can be much higher than the max value for a byte (255). To prevent rolling over, clip it at 255.
    {
        avgC = 255;
    }

    float totalColor = avgR + avgG + avgB;
    float percentageRed = avgR / totalColor;
    float percentageGreen = avgG / totalColor;
    float percentageBlue = avgB / totalColor;

    uint8_t proportionRed = round(percentageRed * 255);
    uint8_t proportionGreen = round(percentageGreen * 255);
    uint8_t proportionBlue = round(percentageBlue * 255);
    uint8_t totalLuminance = avgC;

    RGBColor newColor = { proportionRed, proportionGreen, proportionBlue, totalLuminance };
    colors[0] = newColor; // Store the black color at index 0
    writeColorToEEPROM(0, newColor);
}

#pragma endregion Buttons and Sensors

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
DISPLAY-RELATED FUNCTIONS
*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma region 14-Segment Display

void showGame() {
    // Displays the currently selected game.

    uint8_t totalGames = gameRegistry.getGameCount();
    char buffer[40];

    display.clear();

    if (currentGame < totalGames) { // Index points to a registered game
        const char* gameName = gameRegistry.getFormattedName(currentGame);
        if (gameName) {
            strncpy(buffer, gameName, sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = '\0'; // Ensure null termination
        } else {
            strncpy(buffer, "ERR", sizeof(buffer)); // Error getting name
        }
    } else if (currentGame == totalGames) {
        // Index points to the TOOLS option
        getProgmemString(toolsMenu[0] - 16, buffer, sizeof(buffer)); // Hacky: Need to adjust index based on how toolsMenu is structured relative to games
        // We want "*X-TOOLS" where X is totalGames + 1
        snprintf(buffer, sizeof(buffer), "*%d-TOOLS", totalGames + 1);
    } else {
        // Should not happen, but handle invalid currentGame index
        strncpy(buffer, "INV", sizeof(buffer));
    }

    // Display or scroll the buffer content
    if (strlen(buffer) > 4) {
        scrollMenuText(buffer); // Use the existing scroll function
    } else {
        flags3.scrollingMenu = false; // Stop scrolling if name fits
        for (uint8_t i = 0; i < 4 && i < strlen(buffer); i++) {
            display.writeDigitAscii(i, buffer[i]);
        }
        display.writeDisplay();
    }
}

void showTool() // Displays the currently selected tool.
{
    char buffer[40];
    getProgmemString(toolsMenu[currentToolsMenu], buffer, sizeof(buffer));
    display.clear();
    if (strlen(buffer) > 4) {
        scrollMenuText(buffer);
    } else {
        flags3.scrollingMenu = false;
        for (uint8_t i = 0; i < 4; i++) {
            display.writeDigitAscii(i, buffer[i]);
        }
        display.writeDisplay();
    }
    if (currentToolsMenu != previousToolsMenu) {
        previousToolsMenu = currentToolsMenu;
    }
}

void startPreGameAnimation() // Starts the pre-game animation, which transitions into the blinking animation
{
    if (!flags2.initialAnimationInProgress && !flags2.initialAnimationComplete) {
        currentDisplayState = INTRO_ANIM;
        flags2.initialAnimationInProgress = true;
        //if (verbose) {
          //  Serial.println(F("Starting pre-game animation..."));
       // }
    }
}

void startScrollText(const char* text, uint16_t start, uint16_t delay, uint16_t end) // Starts scrolling text. Inputs are: text to scroll, length of time to hold while starting, scroll frame delay time, and length of time to hold while ending.
{
    strncpy(message, text, sizeof(message) - 1);
    message[sizeof(message) - 1] = '\0';
    textStartHoldTime = start;
    textSpeedInterval = delay;
    textEndHoldTime = end;
    lastScrollTime = millis();
    flags2.scrollingComplete = false;
    scrollIndex = -1; // Reset the scroll index
}

// Updates scrolling text as we loop.
void updateScrollText() {
    if (currentDisplayState == SCREENSAVER)
        return;

    // if (!flags2.scrollingStarted)
    //   return;

    unsigned long currentTime = millis(); // Update time

    // Only shift the scroll position after scrollDelayTime
    if (currentTime - lastScrollTime >= scrollDelayTime) {
        lastScrollTime = currentTime; // Update the last scroll time
        if (scrollIndex == -1) {
            display.clear();
            display.writeDigitAscii(0, message[0]);
            display.writeDigitAscii(1, message[1]);
            display.writeDigitAscii(2, message[2]);
            display.writeDigitAscii(3, message[3]);
            display.writeDisplay();
            scrollDelayTime = textStartHoldTime; // Set delayTime to hold interval
            scrollIndex++;
        } else if (scrollIndex < static_cast<int>(strlen(message)) - 3) {
            // Scroll the text
            display.clear();
            display.writeDigitAscii(0, message[scrollIndex]);
            display.writeDigitAscii(1, message[scrollIndex + 1]);
            display.writeDigitAscii(2, message[scrollIndex + 2]);
            display.writeDigitAscii(3, message[scrollIndex + 3]);
            display.writeDisplay();
            scrollDelayTime = textSpeedInterval; // Set delayTime to scroll interval
            scrollIndex++;
        } else {
            // Hold the last frame for one second
            display.clear();
            display.writeDigitAscii(0, message[strlen(message) - 4]);
            display.writeDigitAscii(1, message[strlen(message) - 3]);
            display.writeDigitAscii(2, message[strlen(message) - 2]);
            display.writeDigitAscii(3, message[strlen(message) - 1]);
            display.writeDisplay();
            scrollDelayTime = textEndHoldTime; // Set delayTime to hold interval
            scrollIndex = -1;                  // Reset the scroll index to start again
            messageRepetitions++;              // How many times has the full message repeated, in messages that only repeat x times before advancing
            messageLine++;                     // When one message has several lines, used to increment through
            flags2.scrollingStarted = false;
            if (messageLine > 3) {
                messageLine = 0;
            }
            delay(10); // Helps smooth out function?
        }
    }
}

void stopScrollText() {
    display.clear();
    messageRepetitions = 0;
    messageLine = 0;
    lastScrollTime = millis();
    flags2.scrollingStarted = false;
    flags2.scrollingComplete = true;
    scrollIndex = -1; // Reset the scroll index
}

void displayFace(const char* word) // Displays a single 4-letter word.
{
    display.clear();
    for (uint8_t i = 0; i < 4; i++) {
        if (word[i] != '\0') // Check if the character is not the string terminator
        {
            display.writeDigitAscii(i, word[i]);
        }
    }
    display.writeDisplay();
}

void scrollMenuText(const char* text) // Helper function that receives text from "showGame()" and "showTool()"
{
    if (!flags3.scrollingMenu) {
        startScrollText(text, 1000, textSpeedInterval, 1000);
        flags3.scrollingMenu = true;
    }
    updateScrollText();
}

// A catch-all display-updating switch-case that controls what should be displayed on the 14-segment timer when called.
void updateDisplay() {
    if (currentDisplayState != previousDisplayState) {
        flags2.scrollingStarted = false;
        flags2.scrollingComplete = false;
        messageRepetitions = 0;
        scrollIndex = -1;
        currentFrameIndex = 0;
        lastFrameTime = millis();
        previousDisplayState = currentDisplayState;
        //if (verbose) {
          //  Serial.print(F("Display state changed to: "));
            //Serial.println(currentDisplayState);
       // }
    }

    switch (currentDisplayState) {
        case INTRO_ANIM:
            if (!flags2.initialAnimationComplete) {
                runAnimation(initialBlinking);
            } else {
                advanceMenu();
            }
            break;

        case SCROLL_PLACE_TAGS_TEXT:
            if (!flags2.scrollingStarted && !flags2.scrollingComplete) {
                startScrollText("PLACE TAGS, TAP G ", 0, textSpeedInterval, 1200);
                flags2.scrollingStarted = true;
            } else if (flags2.scrollingStarted && !flags2.scrollingComplete) {
                updateScrollText();
                if (messageRepetitions >= 2) {
                    messageRepetitions = 0;
                    flags2.scrollingComplete = true;
                }
            } else {
                advanceMenu();
            }
            break;

        case SCROLL_PICK_GAME_TEXT:
            if (!flags2.scrollingStarted && !flags2.scrollingComplete) {
                startScrollText("Y/B = PICK GAME ", 0, textSpeedInterval, textEndHoldTime);
                flags2.scrollingStarted = true;
            }

            if (!flags2.scrollingComplete) {
                updateScrollText();
                if (messageRepetitions >= 2) {
                    messageRepetitions = 0;
                    flags2.scrollingComplete = true;
                    flags2.scrollingStarted = false;
                    flags2.blinkingAnimationActive = true;
                }
            } else {
                advanceMenu();
            }
            break;

        case SCREENSAVER:
                runAnimation(screensaverBlinking);
            break;

        case SELECT_GAME:
            showGame();
            break;

        case SELECT_TOOL:
            if (!flags3.insideDealrTools) {
                showTool();
            }
            break;

        case DEAL_CARDS:
            displayFace("DEAL");
            break;

        case ERROR:
            displayFace("EROR");
            delay(1000);
            flags4.fullExit = true;
            currentDealState = RESET_DEALR;
            break;

        case STRUGGLE:
            expressionStarted = millis();
            displayFace(EFFORT);
            break;

        case LOOK_LEFT:
            displayFace(LEFT);
            break;

        case LOOK_RIGHT:
            displayFace(RIGHT);
            break;

        case LOOK_STRAIGHT:
            displayFace(LOOK_BIG);
            break;

        case FLIP:
            displayFace("FLIP");
            break;
        
        case CUSTOM_FACE:
            displayFace(customFace);
            break;
    }

    if (flags3.scrollingMenu) {
        updateScrollText();
    }
}

void displayErrorMessage(const char* message) // Displays an error message, then resets DEALR.
{
    if (!flags2.scrollingStarted) {
        flags2.scrollingStarted = true;
        startScrollText(message, 1000, textSpeedInterval, 1000);
    }
    while (!flags2.scrollingComplete) {
        updateScrollText();
        if (messageRepetitions > 0) {
            flags2.scrollingComplete = true;
        }
    }
    currentDealState = RESET_DEALR;
    updateDisplay();
}

void getProgmemString(const char* progmemStr, char* buffer, size_t bufferSize) // Helper function for getting strings to store in progmem.
{
    strncpy_P(buffer, progmemStr, bufferSize - 1);
    buffer[bufferSize - 1] = '\0'; // Ensure null-terminated
}

void runAnimation(const DisplayAnimation& animation) {
    unsigned long currentTime = millis();
    static unsigned long lastAnimationTime = 0;
    static uint8_t currentFrame = 0;
    static const DisplayAnimation* lastAnimation = nullptr;
    static uint8_t lastDisplayedFrame = 255; // Track last displayed frame

    if (lastAnimation != &animation) // If switching animations, reset frame tracking

    {
       // if (verbose) {
        //    Serial.println(F("New animation detected! Resetting frames."));
       // }
        currentFrame = 0;
        lastAnimation = &animation;
        lastAnimationTime = currentTime; // Ensure immediate update
        lastDisplayedFrame = 255;        // Force the first frame to print

        displayFace(animation.frames[currentFrame]); // Display the first frame immediately
       // if (verbose) {
     //       Serial.print(F("Frame: "));
      //      Serial.print(animation.frames[currentFrame]);
      //     Serial.print(F(" | Interval: "));
       //     Serial.println(animation.intervals[currentFrame]);
       //}

        lastDisplayedFrame = currentFrame; // Update last displayed frame
        return;                            // Exit to avoid waiting
    }

    // Use the current frame's interval before changing the frame
    if (currentTime - lastAnimationTime >= animation.intervals[currentFrame]) {
        lastAnimationTime = currentTime; // Update timing BEFORE incrementing frame

        if (&animation == &initialBlinking && currentFrame == animation.numFrames - 1) // If this is the last frame of `initialBlinking`, mark the animation as complete and stop updating
        {
          //  if (verbose) {
          //      Serial.println(F("Initial blinking animation complete!"));
          //  }
            currentFrame = 0;
            lastAnimation = nullptr;
            flags2.initialAnimationComplete = true;
            flags2.initialAnimationInProgress = false;
            return; // Prevent displaying frame 0 again
        }

        currentFrame = (currentFrame + 1) % animation.numFrames; // Advance frame AFTER using the interval

        displayFace(animation.frames[currentFrame]); // Display new frame

        if (currentFrame != lastDisplayedFrame) // Print debug before changing frames
        {
         //  if (verbose) {
           //     Serial.print(F("Frame: "));
          //      Serial.print(animation.frames[currentFrame]);
           //     Serial.print(F(" | Interval: "));
           //     Serial.println(animation.intervals[currentFrame]);
         //   }
            lastDisplayedFrame = currentFrame; // Update last displayed frame
        }
    }
}

void handleAwaitingPlayerDecision() {
    // Displays the correct scroll text during "Awaiting Player Decision" deal state.

    // Check if game should be over (excluding tagless special case)
    if (flags3.postDeal && postCardsToDeal <= 0){ // && !taglessGame) {
        if (currentGamePtr) currentGamePtr->onGameOver(); // Let game do final actions if needed
        flags3.gameOver = true;                                  // Signal main loop game is over
        return;
    }

    // If a game is active, let it handle its display
    if (currentGamePtr) {
        currentGamePtr->handleAwaitDecisionDisplay();
    } else if (flags3.postDeal) {  //taglessGame && 
        // Not sure what if anything is supposed to go here
        displayFace("?");
    } else {
        // This should never be hit
        displayFace("?");
    }
}
#pragma endregion 14 - Segment Display

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
UI MANIPULATION FUNCTIONS
*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region UI Manipulation

void advanceMenu() {
    // Advances menus in UI according to current selection.
   // if (verbose) Serial.println(F("Advancing menu."));
    
    flags2.scrollingStarted = false;
    flags2.scrollingComplete = false;
    flags3.scrollingMenu = false;
    uint8_t totalGames = gameRegistry.getGameCount();

    switch (currentDisplayState) {
        case INTRO_ANIM:
            if (scrollInstructions) {
                currentDisplayState = SCROLL_PLACE_TAGS_TEXT;
            } else {
                flags3.buttonInitialization = true;
                currentDisplayState = SELECT_GAME;
                flags4.toolsMenuActive = false; // Switching to select game menu. Deactivating Tools menu.
            }

            break;

        case SCROLL_PLACE_TAGS_TEXT: // If we're in SCROLL_PLACE_TAGS_TEXT when advanceMenu() is called, we advance to SCROLL_PICK_GAME_TEXT.
            currentDisplayState = SCROLL_PICK_GAME_TEXT;
            break;

        case SCROLL_PICK_GAME_TEXT:
            if (flags2.blinkingAnimationActive) {
                currentDisplayState = SCREENSAVER;
            } else {
                currentDisplayState = SELECT_GAME;
                flags4.toolsMenuActive = false; // Switching to select game menu. Deactivating Tools menu.
            }
            break;

        case SCREENSAVER:
            flags2.blinkingAnimationActive = false;
            currentAnimation = nullptr; // Reset the active animation so if the screensaver starts again, it starts from the top.
            if (scrollInstructions) {
                currentGame = 0;
                currentDisplayState = SCROLL_PLACE_TAGS_TEXT;
            } else {
                flags3.buttonInitialization = true;
                currentDisplayState = SELECT_GAME;
            }
            break;

        case SELECT_GAME:
            //Serial.println("advanceMenu:  In Select Game case");
            currentGamePtr = nullptr;

            if (currentGame < totalGames) { // A game is selected
                currentGamePtr = gameRegistry.getGame(currentGame);
                if (currentGamePtr) {
                    bool startDealing = currentGamePtr->initialize(); // Call game's setup method
                    if (startDealing) {
                            // Game selected, setup done, start dealing
                            currentDisplayState = DEAL_CARDS;
                            currentDealState = DEALING;
                            // Optionally call onDealStart here if needed right before dealing starts
                            // currentGamePtr->onDealStart();
                        //}
                    } else {
                            // Should not happen if initialize is false without requiring selection
                           // if (verbose) Serial.println(F("WARN: initialize false but no selection required?"));
                            currentDisplayState = SELECT_GAME; // Stay here? Or error?
                    }
                } else {
                    // Error getting game pointer
                 //   if (verbose) Serial.println(F("ERROR: Invalid game pointer selected!"));
                    currentDisplayState = ERROR; // Go to error state
                    currentDealState = IDLE;
                }
            } else if (currentGame == totalGames) { // TOOLS menu selected
                flags4.toolsMenuActive = true;             // Activate tools mode
                currentDisplayState = SELECT_TOOL;
                currentToolsMenu = 0; // Start at the first tool
            } else {
                // Should not happen
                //if (verbose) Serial.println(F("ERROR: Invalid currentGame index in advanceMenu!"));
                currentDisplayState = ERROR;
                currentDealState = IDLE;
            }
            break;

        case SELECT_TOOL:
            if (flags4.toolsMenuActive && currentToolsMenu == 0) // DEAL SINGLE CARD
            {
                flags1.dealInitialized = true; // Initialize wherever we are so we don't have to go looking for the red tag for this tool.
                currentDealState = DEALING;
                remainingRoundsToDeal = 0;
                currentDisplayState = STRUGGLE;
                updateDisplay();
            // } else if (flags4.toolsMenuActive && currentToolsMenu == 1) // SHUFFLE CARDS
            // {
            //  //   shufflingCards = true;
            //     remainingRoundsToDeal = 52;
            //     currentDealState = DEALING;
            //     currentDisplayState = DEAL_CARDS;
            //     updateDisplay();
            // } else if (flags4.toolsMenuActive && currentToolsMenu == 2) // SEPARATE MARKED/UNMARKED CARDS
            // {
            //  //   separatingCards = true;
            //     remainingRoundsToDeal = 52;
            //     currentDealState = DEALING;
            //     currentDisplayState = DEAL_CARDS;
            //     updateDisplay();
            } else if (flags4.toolsMenuActive && currentToolsMenu == 1) // COLOR TUNER
            {
                colorTuner();
            } else if (flags4.toolsMenuActive && currentToolsMenu == 2) // UV TUNER
            {
                uvSensorTuner();
            } else if (flags4.toolsMenuActive && currentToolsMenu == 3) // RESET COLOR VALUES TO DEFAULT
            {
                resetEEPROMToDefaults();
                displayFace("DONE");
                // printStoredColors();
                delay(1500);
                updateDisplay();
                flags4.fullExit = true;
                currentDealState = RESET_DEALR;
                updateDisplay();
            }
            flags3.insideDealrTools = true;
            break;

        case DEAL_CARDS:
            currentDealState = DEALING;
            // currentGamePtr->onDealStart();
            // Or maybe this goes on the switch for current deal state? print in handler to test if calls are repetitive
            break;

        case LOOK_STRAIGHT:
            break;

        case LOOK_RIGHT:
            break;

        case LOOK_LEFT:
            break;

        case STRUGGLE:
            break;

        case FLIP:
            break;

        case ERROR:
            break;
    }
}

void goBack() // Returns to prior menu, or exits program.
{
   // if (verbose) {
   //     Serial.println(F("Going back one menu."));
   // }
    flags3.scrollingMenu = false;
    switch (currentDisplayState) {
        case DEAL_CARDS:
            currentDisplayState = SELECT_GAME;
            break;

        case SELECT_TOOL:
            flags4.toolsMenuActive = false; // Switching to select game menu. Deactivating Tools menu.
            currentDisplayState = SELECT_GAME;
            break;

        case SELECT_GAME:
            currentGame = 0;
            currentToolsMenu = 0;
            flags4.toolsMenuActive = false;
            flags3.buttonInitialization = false;
            flags2.scrollingStarted = false;
            if (scrollInstructions) // If instructions are enabled, going back when in the "select game" menu goes to instructions.
            {
                currentDisplayState = SCROLL_PICK_GAME_TEXT;
                flags2.blinkingAnimationActive = false;
            } else // Otherwise, we just play the blinking animation.
            {
                flags2.initialAnimationComplete = false;
                currentDisplayState = INTRO_ANIM;
            }
            currentDealState = IDLE;
            break;

        case SCROLL_PICK_GAME_TEXT:
            messageRepetitions = 2; // Skip scrolling text when going back
            flags3.buttonInitialization = false;
            currentDisplayState = SCROLL_PLACE_TAGS_TEXT;
            flags2.blinkingAnimationActive = false;
            flags2.scrollingStarted = false;
            flags2.scrollingComplete = false;
            startPreGameAnimation();
            break;

        case SCROLL_PLACE_TAGS_TEXT:
            // We don't go back to the quick intro blink, we just stay put.
            flags3.buttonInitialization = false;
            break;

        case INTRO_ANIM:
            // Nothing happens when we go back, but we can make sure flags3.buttonInitialization = false so the screensaver starts.
            flags3.buttonInitialization = false;
            break;

        case SCREENSAVER:
            // If we're watching the screensaver and someone hits the back button, the system should wake up.
            advanceMenu();
            break;

        case LOOK_STRAIGHT:
            break;

        case LOOK_RIGHT:
            break;

        case LOOK_LEFT:
            break;

        case STRUGGLE:
            break;

        case FLIP:
            break;

        case ERROR:
            break;
    }
}

void decreaseSetting() // In menus, decrements selected menu option.
{
    if (currentDisplayState == SELECT_GAME) {
        currentGame--;
        if (currentGame < 0) {
            currentGame = gameRegistry.getGameCount();
        }
    } else if (currentDisplayState == SELECT_TOOL) {
        currentToolsMenu--;
        if (currentToolsMenu < 0) {
            currentToolsMenu = numToolMenus;
        }
    }
}

void increaseSetting() // In menus, increments selected menu option.
{
    if (currentDisplayState == SELECT_GAME && currentGame >= 0) {
        currentGame++;
        if (currentGame > gameRegistry.getGameCount()) {
            currentGame = 0;
        }
    } else if (currentDisplayState == SELECT_TOOL && currentToolsMenu >= 0) {
        currentToolsMenu++;
        if (currentToolsMenu > numToolMenus) {
            currentToolsMenu = 0;
        }
    }
}
#pragma endregion UI Manipulation

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
MOTOR CONTROL FUNCTIONS
*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region Motor Control

// This function proceeds through steps to eject a card from DEALR.
void slideCard(uint8_t &amount) {
    unsigned long currentTime = millis(); // Update time
    static unsigned long lastStepTime = 0;

    switch (slideStep) {
        case 0: // Start feeding card with feed servo:
            if (slideStep != previousSlideStep) {
                previousSlideStep = slideStep;
                feedCard.write(150); // Advances feed motor to feed card towards mouth
                lastStepTime = currentTime;
            }
            if (flags3.cardLeftCraw == true) // If the IR sensor sees a card has passed through the mouth of DEALR...
            {
                flags3.cardLeftCraw = false; // Reset this state for the next card to deal.

                // Only advance state once we've shot enough cards
                amount--;
                if (amount <= 0) {
                    slideStep = 1;
                }
            }
            break;

        case 1: // Reverses motion to retract the next card into a deal-ready state.
            feedCard.write(30);
            lastStepTime = currentTime;
            slideStep = 2;
            break;

        case 2: // Wait for reverseFeedTime to complete.
            if (slideStep != previousSlideStep && currentTime - lastStepTime >= reverseFeedTime) {
                previousSlideStep = slideStep;
                feedCard.write(90); // Neutral position to stop the servo
                lastStepTime = currentTime;
                slideStep = 3;
            }
            break;

        case 3:                                                                      // Stop the servo and complete the slide
            if (slideStep != previousSlideStep && currentTime - lastStepTime >= 100) // Small delay to ensure the servo has stopped.
            {
                previousSlideStep = -1;
                feedCard.write(90);
                slideStep = 0;
                flags1.throwingCard = false;
                // if (!flags4.errorInProgress) {
                    flags1.cardDealt = true;
                // }
                overallTimeoutTag = currentTime;
                delay(10);
            }
            break;
    }
}

// Starts rotation CW or CCW at a specified speed.
void rotate(uint8_t rotationSpeed, bool direction) {
    analogWrite(MOTOR_2_PWM, rotationSpeed);

    // CW = "True"
    if (direction == CW) {
        flags1.rotatingCW = true;
        flags1.rotatingCCW = false;
        stopped = false;
        digitalWrite(MOTOR_2_PIN_1, HIGH);
        digitalWrite(MOTOR_2_PIN_2, LOW);
        currentDisplayState = LOOK_LEFT;
    } 
    
    // CCW = "False"
    else if (direction == CCW) {
        flags1.rotatingCW = false;
        flags1.rotatingCCW = true;
        stopped = false;
        analogWrite(MOTOR_2_PWM, rotationSpeed);
        digitalWrite(MOTOR_2_PIN_1, LOW);
        digitalWrite(MOTOR_2_PIN_2, HIGH);
        currentDisplayState = LOOK_RIGHT;
    }
    updateDisplay();
}

// Stops yaw rotation and toggles a few rotating states to false.
void rotateStop() {
    if (!stopped) {
        stopped = true;
        flags1.rotatingCW = false;
        flags1.rotatingCCW = false;
        digitalWrite(MOTOR_2_PIN_1, LOW);
        digitalWrite(MOTOR_2_PIN_2, LOW);
        delay(20); // Slight delay while motors stop.
    }
}

void flywheelOn(bool direction) // Turns flywheel on. Accepts "true" for forward, "false" for reverse.
{
    if (direction == false) {
        analogWrite(MOTOR_1_PWM, flywheelMaxSpeed);
        digitalWrite(MOTOR_1_PIN_1, HIGH);
        digitalWrite(MOTOR_1_PIN_2, LOW);
    } else {
        analogWrite(MOTOR_1_PWM, flywheelMaxSpeed);
        digitalWrite(MOTOR_1_PIN_1, LOW);
        digitalWrite(MOTOR_1_PIN_2, HIGH);
    }
}

void flywheelOff() // Turns flywheel off.
{
    digitalWrite(MOTOR_1_PIN_1, LOW);
    digitalWrite(MOTOR_1_PIN_2, LOW);
    delay(20);
}

void switchRotationDirection() // Changes direction of yaw rotation.
{
    flags4.rotatingBackwards = !flags4.rotatingBackwards;
}
#pragma endregion Motor Control

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
DEALR TOOLS FUNCTIONS AND THEIR HELPERS
DEALR has a few "tools" that are used for debugging and tuning. This section includes functions for saving new threshold values to EEPROM for
the specific color values of the specific tags in each box (colorTuner), the specific luminance values of the UV light reflected off any given deck
of cards (uvSensorTuner), as well as a reset to factory defaults function, a "shuffle" function, and "separate marked from unmarked cards" function,
and a "deal a single card" function.
*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region DEALR Tools

void colorTuner() // Controls the "color tuning" operation that locks down RGB values for color tags, which can experience variation for a bunch of reasons, like room lighting and UV exposure fading.
{
    if (!flags2.scrollingStarted) {
        messageRepetitions = 1;
        flags2.scrollingStarted = true;
        flags2.scrollingComplete = false;
    }

    int messageCounter = 0;
    const char* messages[] = {
        "REMOVE TAGS",
        "PUT UNDR SENSOR WHEN PROMPT",
        "PRESS G TO CONFIRM"
    };
    const int numMessages = sizeof(messages) / sizeof(messages[0]);

    while (!flags2.scrollingComplete) {
        if (messageRepetitions >= 1) {
            messageRepetitions = 0;
            startScrollText(messages[messageCounter], textStartHoldTime, textSpeedInterval, textEndHoldTime);
            messageCounter = (messageCounter + 1) % numMessages; // Cycle through messages
        }
        updateScrollText();

        if (digitalRead(BUTTON_PIN_1) == LOW || digitalRead(BUTTON_PIN_2) == LOW || digitalRead(BUTTON_PIN_3) == LOW) // Pressing a button cancels the text animation.
        {
            messageCounter = 0;
            flags2.scrollingComplete = true;
            flags2.scrollingStarted = false;
            while (digitalRead(BUTTON_PIN_1) == LOW || digitalRead(BUTTON_PIN_2) == LOW || digitalRead(BUTTON_PIN_3) == LOW) {
                // Wait for "confirm" button to be released before proceeding.
            };
            break;
        } else if (digitalRead(BUTTON_PIN_4) == LOW) {
            messageCounter = 0;
            flags4.toolsExit = true;
            currentDealState = RESET_DEALR;
            updateDisplay();
            break;
        }
    }

    if (flags2.scrollingComplete) {
        logBlackBaseline();
        recordColors(1); // Start from index 1 (Red) instead of black.
        loadColorsFromEEPROM();
        displayFace("DONE");
        flags2.scrollingStarted = false; // reset in case we want to tune again
        flags2.scrollingComplete = false;
        flags3.insideDealrTools = false;
        delay(1500);

        currentDealState = IDLE;
        flags4.toolsMenuActive = false; // Switching to select game menu. Deactivating Tools menu.
        currentDisplayState = SELECT_GAME;
        updateDisplay();
    }
}

void recordColors(int startIndex) // This function scans, records, and saves color values to EEPROM for colorTuner().
{
    unsigned long lastDebounceTime = 0;
    const unsigned long debounceDelay = 50; // 50ms debounce delay
    flags4.toolsMenuActive = false;

    for (int i = startIndex; i < TOTAL_COLORS; i++) {
        unsigned long currentTime = millis();
        displayFace(colorNames[i]);

        bool buttonPressed = false;
        bool buttonReleased = true;

        while (!buttonPressed) {
            if (digitalRead(BUTTON_PIN_4) == LOW) { // Allows the "back" button to cancel this operation
                flags4.toolsExit = true;
                currentDealState = RESET_DEALR;
                updateDisplay();
                return;
            }
            if (digitalRead(BUTTON_PIN_1) == LOW && buttonReleased) {
                currentTime = millis();
                if ((currentTime - lastDebounceTime) > debounceDelay) {
                    while (digitalRead(BUTTON_PIN_1) == LOW) {
                        buttonReleased = false;
                        buttonPressed = true;
                        lastDebounceTime = currentTime;
                        displayFace("SAVD");
                        delay(1000);
                    }
                }
            } else if (digitalRead(BUTTON_PIN_1) == HIGH) {
                buttonPressed = false;
                buttonReleased = true;
                lastDebounceTime = currentTime;
            }
        }

        uint32_t totalR = 0, totalG = 0, totalB = 0, totalC = 0;
        for (int j = 0; j < numSamples; j++) {
            uint16_t r, g, b, c;
            sensor.getRawData(&r, &g, &b, &c);
            totalR += r;
            totalG += g;
            totalB += b;
            totalC += c;
            delay(10); // Small delay between samples
        }

        uint16_t avgR = totalR / numSamples;
        uint16_t avgG = totalG / numSamples;
        uint16_t avgB = totalB / numSamples;
        uint16_t avgC = totalC / numSamples;

        if (avgC > 255) // Total brightness can be much higher than the max value for a byte (255). To prevent rolling over, clip it at 255.
        {
            avgC = 255;
        }

        float totalColor = avgR + avgG + avgB;
        float percentageRed = avgR / totalColor;
        float percentageGreen = avgG / totalColor;
        float percentageBlue = avgB / totalColor;

        uint8_t proportionRed = round(percentageRed * 255);
        uint8_t proportionGreen = round(percentageGreen * 255);
        uint8_t proportionBlue = round(percentageBlue * 255);
        uint8_t totalLuminance = avgC;

        RGBColor newColor = { proportionRed, proportionGreen, proportionBlue, totalLuminance };
        colors[i] = newColor;
        writeColorToEEPROM(i, newColor);
    }
}

void uvSensorTuner() // Controls the "uv tuning" operation that locks down the threshold visible light value for a card to be determined to be "marked.""
{
    if (!flags2.scrollingStarted) {
        messageRepetitions = 1;
        flags2.scrollingStarted = true;
        flags2.scrollingComplete = false;
    }

    int messageCounter = 0;
    const char* messages[] = {
        "PUT 10 UNMARKED CARDS IN DEALR", // We only scan 5 cards, but if only 5 cards are in the hopper, we risk getting UV bleed around the edge of the last card. So we ask the user to stack a few extra.
        "TAP G TO START "
    };
    const int numMessages = sizeof(messages) / sizeof(messages[0]);

    while (!flags2.scrollingComplete) {
        if (messageRepetitions >= 1) {
            messageRepetitions = 0;
            startScrollText(messages[messageCounter], textStartHoldTime, textSpeedInterval, textEndHoldTime);
            messageCounter = (messageCounter + 1) % numMessages; // Cycle through messages
        }
        updateScrollText();

        if (digitalRead(BUTTON_PIN_1) == LOW || digitalRead(BUTTON_PIN_2) == LOW || digitalRead(BUTTON_PIN_3) == LOW) {
            flags2.scrollingComplete = true;
            flags2.scrollingStarted = false;
            while (digitalRead(BUTTON_PIN_1) == LOW || digitalRead(BUTTON_PIN_2) == LOW || digitalRead(BUTTON_PIN_3) == LOW) {
                // Wait for "confirm" button to be released before proceeding.
            };
            break;
        } else if (digitalRead(BUTTON_PIN_4) == LOW) {
            flags4.toolsExit = true;
            currentDealState = RESET_DEALR;
            updateDisplay();
            break;
        }
    }

    if (flags2.scrollingComplete) {
        recordUVThreshold();
        loadStoredUVValueFromEEPROM(storedUVThreshold);
        displayFace("DONE");
        flags2.scrollingStarted = false;
        flags2.scrollingComplete = false;
        flags3.insideDealrTools = false;
        delay(1500);

        flags4.toolsExit = true;
        currentDealState = RESET_DEALR;
        updateDisplay();
    }
}

void recordUVThreshold() // Helper function for uvSensorTuner().
{
    int numUVReadings = 5;
    uint16_t uvReadings[numUVReadings] = { 0 };
    uint16_t maxUV = 0; // Variable for holding total UV values.
    uint8_t thresholdBuffer = 5;

    for (int i = 0; i < numUVReadings; i++) {
        dealSingleCard();
        delay(100);
        uvReadings[i] = analogRead(UV_READER);
        if (uvReadings[i] > maxUV) {
            maxUV = uvReadings[i];
        }
        delay(200);
        flags1.cardDealt = false;
    }

    uint16_t inflatedMaxUV = maxUV + thresholdBuffer;

    EEPROM.put(UV_THRESHOLD_ADDR, inflatedMaxUV); // Save UV average to EEPROM (use an address that does not conflict with color values)

    // Convert the inflatedMaxUV to a 4-character string
    char uvValueStr[5]; // Buffer to hold the 4-character string (4 chars + null terminator)
    sprintf(uvValueStr, "%04u", inflatedMaxUV);

    // Display the 4-character string on the display
    displayFace(uvValueStr);
    delay(1500);

    displayFace("SAVD");
    delay(1200);
}

void resetEEPROMToDefaults() // Function for resetting EEPROM values to defaults
{
    for (int i = 0; i < TOTAL_COLORS; i++) // Write default values to EEPROM
    
    {
        RGBColor tempColor;
        memcpy_P(&tempColor, &defaultColors[i], sizeof(RGBColor));
        writeColorToEEPROM(i, tempColor);
        //writeColorToEEPROM(i, defaultColors[i]);
    }

    EEPROM.put(UV_THRESHOLD_ADDR, defaultUVThreshold);

    EEPROM.write(EEPROM_VERSION_ADDR, EEPROM_VERSION); // Write the version byte to indicate EEPROM has been initialized
}
#pragma endregion DEALR Tools

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
ERROR AND TIMEOUT HANDLING FUNCTIONS
*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region Error and Timeout

// Handles timeouts while dealing cards. If something is taking too long, something's probably going wrong.
void handleThrowingTimeout(unsigned long currentTime) {
    static unsigned long retractDuration = 1000;
    static bool retractStarted = false;
    static bool retractCompleted = false;

    if (currentTime - throwStart >= throwExpiration && !retractStarted && !retractCompleted) // For handling dealing timeout.
    {
        retractStarted = true;
        retractStartTime = currentTime;
        feedCard.write(30);
        delay(20); // Smoothing delay
        flywheelOn(false);
        delay(20); // Smoothing delay
    }

    if (retractStarted && currentTime - retractStartTime >= retractDuration) {
        retractStarted = false;
        feedCard.write(90);
        delay(20); // Smoothing delay
        flywheelOff();
        delay(20); // Smoothing delay
        retractCompleted = true;
    }

    if (retractCompleted) {
        retractCompleted = false;
        if (currentToolsMenu == 1) {
            // flags4.errorInProgress = true;
            // shufflingCards = false;
            // flags4.toolsMenuActive = false;
            // flags4.toolsExit = true;
            // currentDisplayState = SELECT_TOOL;
            // currentDealState = RESET_DEALR;

            resetThrowCardState();


        } else if (currentToolsMenu == 2) {
            flags4.errorInProgress = true;
           // separatingCards = false;
            flags4.toolsMenuActive = true;
            flags4.toolsExit = true;
            currentDisplayState = SELECT_TOOL;
            // currentDealState = RESET_DEALR;
        } else {
            // flags4.errorInProgress = true;
            // currentDisplayState = ERROR;
            // currentDealState = RESET_DEALR
            resetThrowCardState();
        }
        updateDisplay();
    }
}

// Reset the state of the card dealer after dispensing / failing to dispence a card
void resetThrowCardState() {
    previousSlideStep = -1;
    feedCard.write(90);
    slideStep = 0;
    flags1.throwingCard = false;
    flags1.cardDealt = true;
    overallTimeoutTag = millis();
    delay(10);
}

void handleFineAdjustTimeout() // Handles timeout for fine adjustment moves.
{
    unsigned long currentTime = millis();

    if (currentTime - adjustStart > errorTimeout && currentDealState != AWAITING_PLAYER_DECISION) {
        errorStartTime = currentTime;
        currentDisplayState = ERROR;
        currentDealState = IDLE;
        updateDisplay();
    }
}

void resetFlags() // Resets all state machine flags when called.
{
    adjustStart = millis();
    flags3.advanceOnePlayer = false;
    flags2.baselineExceeded = false;
    flags2.blinkingAnimationActive = false;
    flags1.dealInitialized = false;
    flags1.cardDealt = false;
    flags3.cardLeftCraw = false;
    flags1.correctingCCW = false;
    flags1.correctingCW = false;
    colorLeftOfDealer = -1;
    flags4.currentlyPlayerLeftOfDealer = false;
    currentGame = 0;
    currentToolsMenu = 0;
    flags4.errorInProgress = false;
    flags2.fineAdjustCheckStarted = false;
    flags3.gameOver = false;
    flags3.insideDealrTools = false;
    flags2.initialAnimationComplete = false;
    flags1.numCardsLocked = false;
    flags2.newDealState = false;
    flags3.notFirstRoundOfDeal = false;
    overallTimeoutTag = millis();
    flags3.postDeal = false;
    flags2.initialAnimationInProgress = false;
    flags4.postDealRemainderHandled = false;
    flags5.postDealStartOnRed = false;
    previousSlideStep = -1;
    previousCardInCraw = 1;
    flags5.playerLeftOfDealerIdentified = false;
    flags4.rotatingBackwards = false;
    resetColorsSeen();
    flags1.rotatingCW = false;
    flags1.rotatingCCW = false;
    remainingRoundsToDeal = 0;
    stopped = true;
    flags3.scrollingMenu = false;
    flags2.scrollingStarted = false;
    flags2.scrollingComplete = false;
    slideStep = 0;
    flags1.throwingCard = false;
    totalCardsToDeal = 0;
    for (uint8_t i = 0; i < NUM_PLAYER_COLORS; i++) {
        colorStatus[i] = -1;
    }
}

void resetColorsSeen() // Function used in the "reset" tool to reset colors seen
{
    memset(colorsSeen, -1, sizeof(colorsSeen));
    colorsSeenIndexValue = 0;
}

void checkTimeouts() // Checks to see whether device has been idle for a long time, and goes into "sleep" mode.
{
    unsigned long currentTime = millis();

    if (flags3.buttonInitialization && currentDealState == IDLE && !flags3.insideDealrTools && currentTime - overallTimeoutTag > timeUntilScreensaverStart) // List of some circumstances where we want to disable the overall timeout function.
    {
        //if (verbose) {
       //     Serial.println(F("System timed out. Returning to screensaver."));
       // }
        stopScrollText();
        flags3.buttonInitialization = false;
        flags2.blinkingAnimationActive = true;
        currentDisplayState = SCREENSAVER;
        currentDealState = IDLE;
        updateDisplay();
    }
}
#pragma endregion Error and Timeout

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
FUNCTIONS FOR READING AND WRITING TO EEPROM
*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region EEPROM

void initializeEEPROM() // Function for checking whether EEPROM values were set by the user or are factory defaults.
{
    if (EEPROM.read(EEPROM_VERSION_ADDR) != EEPROM_VERSION) {
        for (int i = 0; i < TOTAL_COLORS; i++) // Write default values to EEPROM.

        {
            RGBColor tempColor;
            memcpy_P(&tempColor, &defaultColors[i], sizeof(RGBColor));
            writeColorToEEPROM(i, tempColor);
            //writeColorToEEPROM(i, defaultColors[i]);
        }
        EEPROM.put(UV_THRESHOLD_ADDR, defaultUVThreshold);
        EEPROM.write(EEPROM_VERSION_ADDR, EEPROM_VERSION); // Write the version byte to indicate EEPROM has been initialized.
    }
}

void writeColorToEEPROM(int index, RGBColor color) // Used for writing RGB values to EEPROM.
{
    int addr = index * sizeof(RGBColor) + 1;
    EEPROM.put(addr, color);
}

void loadColorsFromEEPROM() // Whatever colors have been saved to EEPROM get loaded at startup using this function.
{
    for (int i = 0; i < TOTAL_COLORS; i++) {
        colors[i] = readColorFromEEPROM(i);
    }
}

void loadStoredUVValueFromEEPROM(uint16_t& uvThreshold) // Loads stored UV threshold values from EEPROM on boot.
{
    EEPROM.get(UV_THRESHOLD_ADDR, uvThreshold);
}

RGBColor readColorFromEEPROM(int index) // Helper function used in loadColorsFromEEPROM.
{
    RGBColor color;
    int addr = index * sizeof(RGBColor) + 1;
    EEPROM.get(addr, color);
    return color;
}

RGBColor getBlackColorFromEEPROM() {
    return readColorFromEEPROM(0); // If we have black stored at index 0, this will retrieve it
}
#pragma endregion EEPROM
#pragma endregion FUNCTIONS
