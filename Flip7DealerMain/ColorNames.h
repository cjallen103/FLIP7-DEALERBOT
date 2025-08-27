#ifndef COLOR_NAMES_H
#define COLOR_NAMES_H

#include <avr/pgmspace.h>
#include "Definitions.h"

//==================================================================
// USER CONFIGURATION
//==================================================================
// 1. Set the number of PLAYER colors you want to use
#define NUM_PLAYER_COLORS 8

// 2. Ensure the two lists below have (NUM_PLAYER_COLORS + 1) entries.
//    The first entry is always "BLAK" for the non-player surface.
//
// 3. Each name in colorNames MUST be exactly 4 characters long. Use spaces if
//    the name is shorter, e.g., "RED ".
//
// 4. The defaultColors list are what the dealr uses until you have tuned the colors.  
//    Once the colors are tuned, the values in this list are ignored
//    
// Example:  if you only have the original 4 colors:
//            - change NUM_PLAYER_COLORS to 4
//            - remove the WHIT through DBLU rows in the colorNames list
//            - remove the same rows in the defaultColors list
//
// Note: The colors I have below work. If you pick two colors that are too similar, the program will get confused. For example,
//  when I first made this, I printed a white tag and a grey tag. The RGB values for these are very similar and the program thought the tags were the same color.
//==================================================================

#define TOTAL_COLORS (NUM_PLAYER_COLORS + 1)    // Do not edit this line

const char* const colorNames[TOTAL_COLORS] = {       // Adjust this list to include the colors you have
    "BLAK", // Index 0: Black (always required)
    "RED ", // Index 1: Player 1
    "YELO", // Index 2: Player 2
    "LBLU", // Index 3: Player 3
    "GREE", // Index 4: Player 4
    "WHIT", // Index 5: Player 5
    "PINK", // Index 6: Player 6
    "PURP", // Index 7: Player 7
    "DBLU"  // Index 8: Player 8
};

// Array for the default raw color sensor values.
// This list MUST have the same number of entries as colorNames.
const RGBColor defaultColors[TOTAL_COLORS] PROGMEM = {
    { 58, 149, 48, 118 },  // Black (Index 0)
    { 121, 105, 29, 255 }, // Red
    { 73, 157, 25, 255 },  // Yellow
    { 35, 132, 89, 255 },  // light Blue
    { 41, 173, 41, 255 },  // Green
    { 54, 138, 63, 255 },  // white
    { 53, 136, 66, 255 },  // pink
    { 58, 108, 89, 255 },  // purple
    { 37, 108, 110, 255 }  // dark blue
};

#endif // COLOR_NAMES_H
