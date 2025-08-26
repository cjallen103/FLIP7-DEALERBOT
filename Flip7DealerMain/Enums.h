// Enums that we need defined centrally for multiple files to access

#ifndef SHARED_ENUMS_H
#define SHARED_ENUMS_H

#include "Definitions.h"

// DEAL STATE: Tracks DEALR's operational states
enum dealState {
    IDLE,                     // Enter this state when we are in menus or finished a game.
    INITIALIZING,             // Enter this state when we want to initialize our rotational direction to the red tag.
    DEALING,                  // Enter this state when we want to deal a single card.
    ADVANCING,                // Enter this state when we want to advance from one tag to the next.
    AWAITING_PLAYER_DECISION, // Enter this state when we're pausing for a player input.
    RESET_DEALR,              // Enter this state when we want to reset dealr completely, including all of its state machine flags.
};

// DISPLAY STATE: Tracks what is displayed on the 14-segment screen.
enum displayState {
    DISPLAY_UNSET,          // A type to signify no state saved. Used for `lastDisplayState` in the Game class
    INTRO_ANIM,             // Very first blinking animation that occurs on boot.
    SCROLL_PLACE_TAGS_TEXT, // First instructions that occur after blinking animation.
    SCROLL_PICK_GAME_TEXT,  // Animation that scrolls "PICK GAME USING BLUE/YELLOW" instructions before game selection menu.
    SELECT_GAME,            // Displays the select game menu.
    SELECT_TOOL,            // Displays the tools menu.
    DEAL_CARDS,             // Controls what is displayed when dealing a card.
    ERROR,                  // Displays "EROR" when an error happens.
    STRUGGLE,               // Struggling face.
    LOOK_LEFT,              // Looking left face.
    LOOK_RIGHT,             // Looking right face.
    LOOK_STRAIGHT,          // Open eyes face.
    FLIP,                   // Display state for the word "FLIP".
    SCREENSAVER,            // Screensaver animation that occurs after timeoutF
    CUSTOM_FACE             // Custom displays the `customFace` string, used by the Game class 
};

// Buttons
enum Buttons : int {
    GREEN = BUTTON_PIN_1,
    BLUE = BUTTON_PIN_2,
    YELLOW = BUTTON_PIN_3,
    RED = BUTTON_PIN_4
};

// Enums to help intellisense for game modules
struct GamInitResult {
    static constexpr bool StartDealing = true;
    // These three are the same but are easier to work with and allow further refarctoring down the road
    static constexpr bool SelectPlayer = false;
    static constexpr bool SelectCards = false;
    static constexpr bool SelectPlayerAndCards = false;
};

//booleans
struct Flags1 {
    uint8_t rotatingCW : 1;     // Indicates clockwise rotation.
    uint8_t rotatingCCW  : 1;       // Indicates counter-clockwise rotation.
    uint8_t correctingCCW : 1;      // Indicates when a fine-adjust correction is being made CCW.
    uint8_t correctingCW : 1;       // Indicates when a fine-adjust correction is being made CW.
    uint8_t dealInitialized : 1;        // Indicates we have initialized to the red tag and are ready to deal.
    uint8_t throwingCard : 1;       // Indicates we're currently throwing a card with flywheel.
    uint8_t cardDealt : 1;          // Indicates whether or not a card has been dealt.
    uint8_t numCardsLocked : 1;         // Indicates confirmation of the number of cards in a selected game.
};
extern Flags1 flags1;

struct Flags2 {
    uint8_t blinkingAnimationActive : 1;        // Indicates if the blinking animation is active.
    uint8_t newDealState: 1;                // Indicates if a change deal state has just taken place.
    uint8_t baselineExceeded : 1;       // Indicates when a spike in color data has been seen.
    uint8_t fineAdjustCheckStarted  : 1;       // Indicates when a fine-adjust on a colored tag has started.
    uint8_t initialAnimationInProgress : 1;   // Indicates when the start animation is in progress.
    uint8_t initialAnimationComplete : 1;     // Indicates whether the initial pre-game animation has been completed.
    uint8_t scrollingStarted : 1;             // Indicates the beginning of a text-scrolling operation.
    uint8_t scrollingComplete : 1;            // Indicates the end of a text-scrolling operation.
};
extern Flags2 flags2;

struct Flags3 {
    uint8_t cardLeftCraw : 1;                 // Indicates when a card has exited the mouth of DEALR.
    uint8_t notFirstRoundOfDeal : 1;          // Indicates whether or not it's currently a round of deal *after* the first.
    uint8_t buttonInitialization : 1;         // Indicates whether or not a button has been pressed yet.
    uint8_t advanceOnePlayer : 1;             // Indicates whether we're currently advancing one player at a time (like in the post-deal portion of Go Fish).
    uint8_t gameOver : 1;                     // Indicates that a game is over and we should fully reset.
    uint8_t postDeal : 1;                     // Indicates a main deal is over and post-deal has begun.
    uint8_t scrollingMenu : 1;                // Indicates whether or not we're currently scrolling menu text.
    uint8_t insideDealrTools : 1;             // Indicates whether or not we're inside one of the DEALR "tools," like Deal a Single Card or Shuffle.
};
extern Flags3 flags3;

struct Flags4 {
    uint8_t toolsMenuActive : 1;              // Indicates whether we're in the "games" or "tools" menus. "False" represents the "games" menu, and "true" the tools menu.
    uint8_t rotatingBackwards : 1;            // Indicates whether or not we're dealing in reverse (useful in rigged games).
    uint8_t postDealRemainderHandled : 1;     // Indicates whether or not we've successfully dealt the post-deal remaining cards, like in Rummy.
    uint8_t fullExit : 1;                     // Indicates, during a reset, that we should return to the pre-game animation.
    uint8_t gamesExit : 1;                   // Indicates, during a reset, that we should return to the games menu.
    uint8_t toolsExit : 1;                    // Indicates, during a reset, that we should return to the tools menu.
    uint8_t errorInProgress : 1;             // Indicates whether or not an error is detected to be in progress.
    uint8_t currentlyPlayerLeftOfDealer : 1;  // Indicates when we're currently looking at the player left of dealer.
};
extern Flags4 flags4;

struct Flags5 {
    uint8_t playerLeftOfDealerIdentified : 1; // Indicates whether or not we've found the player left of dealer.
    uint8_t postDealStartOnRed : 1;           // Indicates if post-deal starts on red or on player left of dealer. Left of dealer is most common, but starting on red is useful in some rigged games.
    uint8_t handlingFlipCard : 1;      // Indicates whether or not we're currently handling the "flip card" in some games that flip a card after main deal.
    uint8_t adjustInProgress : 1;      // Indicates whether or not we're currently fine adjusting to confirm the color of the tag we're looking at.
};
extern Flags5 flags5;

#endif // SHARED_ENUMS_H