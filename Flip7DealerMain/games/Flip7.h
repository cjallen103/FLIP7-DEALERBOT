#ifndef FLIP7_H
#define FLIP7_H

#include "../Game.h"

#define MAX_PLAYERS NUM_PLAYER_COLORS  // max players is number of colors defined in ColorNames.h

// bitmask definitions for playerStatus
#define IS_PLAYING (1 << 0)   // bit 0
#define IS_ACTIVE (1 << 1) // bit 1
#define IS_BUST (1 << 2) // bit 2
#define IS_DEALT (1 << 3) // bit 3
//Helper macros for setting bitmask status
#define isPlayerPlaying(i) (playerStatus[i] & IS_PLAYING)  // return true if player is playing

#define isPlayerActive(i) (playerStatus[i] & IS_ACTIVE)  // return true if player is active in round
#define setIsPlayerActive(i) (playerStatus[i] |= IS_ACTIVE) // set player as active
#define setPlayersActiveIfPlaying(MAX_PLAYERS) for (uint8_t i=0; i<MAX_PLAYERS; i++) { if (isPlayerPlaying(i)) setIsPlayerActive(i); } // set all players who are playing as active
#define setIsNotActive(i) (playerStatus[i] &= ~IS_ACTIVE) // set player as not active

#define isPlayerBust(i) (playerStatus[i] & IS_BUST)  // return true if player is busted
#define setIsBust(i) (playerStatus[i] |= IS_BUST) // set player as busted
#define setIsNotBust(i) (playerStatus[i] &= ~IS_BUST) // set player as not busted
#define setAllPlayersNotBust(MAX_PLAYERS) for (uint8_t i=0; i<MAX_PLAYERS; i++) {setIsNotBust(i);}  //set all players to not busted

#define isPlayerDealt(i) (playerStatus[i] & IS_DEALT) // return true if player has been dealt
#define setIsPlayerDealt(i) (playerStatus[i] |= IS_DEALT)  // set player status to dealt
#define setIsNotDealt(i) (playerStatus[i] &= ~IS_DEALT) // set player to not dealt
#define setAllPlayersNotDealt(MAX_PLAYERS) for (uint8_t i=0; i<MAX_PLAYERS; i++) { if (isPlayerPlaying(i)) setIsNotDealt(i); } // set all players to not dealt

class Flip7 : public Game {
  public:

    enum GameState {   //  game states
        STARTUP,
        DEALSPECIAL,
        ACTION,
        PICK,
        PICKSPECIAL,
        PICKPLAYER,
        ENTERSCORE,
        REPORTSCORE,
        SHOWSCORES,
        GAMEOVER
    };

    enum SpecialState {   // special states
        FREEZE,
        FLIP3,
        NONE
    };

    enum CycleDirection {
        UP,
        DOWN
    };
      
    const char* getName() const override {      
        // needed for pass the game name to the main code
        return "FLIP7";
    }

    virtual const char** getDisplayMessages(uint8_t &count) {       
        // scrolling messages for each state
        switch (gameState) {
            case STARTUP:
                static const char* startupMessages[] = { 
                    "G = START ",
                    "Y/B= TOTAL SCORE "
                };
                count = sizeof(startupMessages) / sizeof(startupMessages[0]);
                return startupMessages;
                break;

            case DEALSPECIAL:
                static const char* dealspecialMessages[] = { 
                    "G = PROCEED ",
                    "R = SPECIAL "
                };
                count = sizeof(dealspecialMessages) / sizeof(dealspecialMessages[0]);
                return dealspecialMessages;
                break;

            case ACTION:
                static const char* actionMessages[] = { 
                    "G = STAND ",
                    "R = HIT "
                };
                count = sizeof(actionMessages) / sizeof(actionMessages[0]);
                return actionMessages;
                break;

            case PICK:
                static const char* pickMessages[] = { 
                    "G = CONTINUE ",
                    "R = BUST ",
                    "Y = SPECIAL ",
                    "B = SEVEN "
                };
                count = sizeof(pickMessages) / sizeof(pickMessages[0]);
                return pickMessages;
                break;

            case PICKSPECIAL:
                static const char* pickspecialMessages[] = { 
                    "G = CONFIRM ",
                    "Y = FREEZE ",
                    "B = FLIP3 "
                };
                count = sizeof(pickspecialMessages) / sizeof(pickspecialMessages[0]);
                return pickspecialMessages;
                break;

            case PICKPLAYER:
                static const char* pickplayerMessages[] = { 
                    "G = CONFIRM ",
                    "Y/B= CHOOSE PLAYER "                        
                };
                count = sizeof(pickplayerMessages) / sizeof(pickplayerMessages[0]);
                return pickplayerMessages;
                break;

            case ENTERSCORE:
                static const char* enterscoreMessages[] = { 
                    "G = GO TO SCORING "
                };
                count = sizeof(enterscoreMessages) / sizeof(enterscoreMessages[0]);
                return enterscoreMessages;
                break;

            case REPORTSCORE:
                static const char* reportscoreMessages[] = { 
                    "G = START NEW ROUND ",
                    "Y/B = SHOW SCORES ",
                    "R = ADJ SCORES "                        
                };
                count = sizeof(reportscoreMessages) / sizeof(reportscoreMessages[0]);
                return reportscoreMessages;
                break;

            case SHOWSCORES:
                static const char* showscoreMessages[] = { 
                    "G = PLAYER OR SCORE ",
                    "Y/B= CHANGE PLAYER "                
                };
                count = sizeof(showscoreMessages) / sizeof(showscoreMessages[0]);
                return showscoreMessages;
                break;

            case GAMEOVER:
                static const char* gameoverMessages[] = { 
                    "G = NEW GAME ",
                    "R = MAIN MENU ",
                    "Y/B= SHOW SCORES "                        
                };
                count = sizeof(gameoverMessages) / sizeof(gameoverMessages[0]);
                return gameoverMessages;
                break;
        }
    }

    bool initialize() override {                    
        // function runs 1st time game starts, sets a bunch of variables/arrays to 0
        setDealAmount(0);
        ScoretoWin = minScore;
        stackPointer = -1;
        numPlayers = 0;
        memset(playerScores, 0, sizeof(playerScores));
        memset(currentRoundScores, 0, sizeof(currentRoundScores));
        memset(playerColors, 0, sizeof(playerColors));
        memset(playerStatus, 0, sizeof(playerStatus));
        memset(&gameFlags, 0, sizeof(gameFlags));
        return true;
    }


    void handleButtonPress(int button) override {
        // function handles button press for each game state
        switch (gameState) {
            case STARTUP:       // set the amount to play, register players, and begin game
                if (button == Buttons::RED) {
                    // RESET machine
                    gameFlags.isDisplayingSelection = false;
                    flags4.gamesExit = true;
                    currentDealState = RESET_DEALR;
                    
                } else if (button == Buttons::YELLOW) {
                    //Decrease amounnt to play to
                    if (ScoretoWin == minScore){
                        ScoretoWin = maxScore;
                    } else {
                        ScoretoWin -= 10;
                    }           
                    snprintf(displayBuffer, sizeof(displayBuffer), "%d", ScoretoWin);
                    displayFace(displayBuffer);
                    gameFlags.isDisplayingSelection = true;

                } else if (button == Buttons::BLUE) {
                    // Increase amount to play to
                    if (ScoretoWin == maxScore){
                        ScoretoWin = minScore;
                    } else {
                        ScoretoWin += 10;
                    }           
                    snprintf(displayBuffer, sizeof(displayBuffer), "%d", ScoretoWin);
                    displayFace(displayBuffer);
                    gameFlags.isDisplayingSelection = true;

                } else if (button == Buttons::GREEN) {
                    // Accept score and begin game
                    gameState = DEALSPECIAL;        // once game begins, enter DEALSPECIAL state
                    gameFlags.isDealing = true; 
                    snprintf(displayBuffer, sizeof(displayBuffer), "%d", ScoretoWin);
                    displayFace(displayBuffer);             //show amount to play to on screen briefly
                    gameFlags.isDisplayingSelection = true;
                    delay(500);
                    RegisterPlayers(); // register each player
                    setPlayersActiveIfPlaying(MAX_PLAYERS); // set all players who are playing as active
                    dealOne(); //deal to starting player
                    gameFlags.isDisplayingSelection = false;
                }
                break;

            case DEALSPECIAL:       // deal one card to everyone and resolve any special cards on initial deal
                if (button == Buttons::RED) {
                    // Special card dealt,  go to PICKSPECIAL state to resolve
                    specialState = NONE;
                    prevState = DEALSPECIAL;
                    gameState = PICKSPECIAL;

                } else if (button == Buttons::YELLOW) {

                } else if (button == Buttons::BLUE) {

                } else if (button == Buttons::GREEN) {
                    // Proceed,     no special card, move to next player or if all dealt, ACTION state
                    proceedDealing();
                }
                break;

            case ACTION:            //  player chooses to hit (draw card) or to stand
                if (button == Buttons::RED) {
                    // draw one immediately and move to PICK state to resolve card
                    dispenseCards(1);
                    gameState = PICK;

                } else if (button == Buttons::YELLOW) {

                } else if (button == Buttons::BLUE) {

                } else if (button == Buttons::GREEN) {
                    // stand,       make player inactive and advance to next active player or end round (ENTERSCORE state) if no active players
                    setIsNotActive(currentPlayerIndex);
                    if (areActivePlayers()){
                        advanceToNextActivePlayer();
                    } else {
                        spin("END ROUND SCORING", spin_normal);
                        gameFlags.isDisplayingSelection = false;
                        gameState = ENTERSCORE; 
                    }
                }
                break;

            case PICK:              // player resolves card with bust, special, seven, or proceed
                if (button == Buttons::RED) {
                    //Bust,     make player inactive and advance to next active player or end round (ENTERSCORE state)
                    setIsNotActive(currentPlayerIndex);
                    setIsBust(currentPlayerIndex);
                    if (areActivePlayers()){
                        if (stackPointer >=1) {      // if the game is more than one level inside of a FLIP3, go back to previous player
                            uint8_t returnPlayer = returnPlayerStack[stackPointer];
                            stackPointer--;
                            moveToPlayer(returnPlayer);
                            gameState = PICK;
                        } else {                    // if game is one level or less inside a FLIP3
                            if (stackPointer == 0){     // if game is one level inside a FLIP3, return to previous player
                                uint8_t returnPlayer = returnPlayerStack[stackPointer];
                                stackPointer--;
                                moveToPlayer(returnPlayer);
                            }
                            if (gameFlags.isDealing == true) {      // if game is in the dealing state, proceed dealing to remaining players
                                proceedDealing();
                            } else {                                // if not dealing, advance to next active player
                                advanceToNextActivePlayer();
                                gameState = ACTION;
                            }
                        }                   
                    } else {                    // if no active players remaining, end round
                        spin("END ROUND SCORING", spin_normal);
                        gameFlags.isDisplayingSelection = false;
                        gameFlags.isDealing = false;
                        gameState = ENTERSCORE; 
                    }
                    
                } else if (button == Buttons::YELLOW) {
                    // SPECIAL          Move to PICKSPECIAL state
                    gameFlags.isDisplayingSelection = false;
                    specialState = NONE;
                    prevState = PICK;
                    gameState = PICKSPECIAL;

                } else if (button == Buttons::BLUE) {
                    // SEVEN            Automatically ends round as this player has 7 cards
                    displayFace(WILD);
                    gameFlags.isDisplayingSelection = true;
                    delay(500);
                    gameFlags.isDisplayingSelection = false;
                    spin("777 END ROUND SCORING", spin_normal);
                    gameFlags.isDisplayingSelection = false;
                    gameFlags.isDealing = false;
                    gameState = ENTERSCORE;

                } else if (button == Buttons::GREEN) {
                    //Proceed,          move to next player
                    if (stackPointer >=1) {         // if the game is more than one level inside of a FLIP3, go back to previous player
                        uint8_t returnPlayer = returnPlayerStack[stackPointer];
                        stackPointer--;
                        moveToPlayer(returnPlayer);
                        gameState = PICK;
                    } else {                        // if game is one level or less inside a FLIP3
                        if (stackPointer == 0){             // if game is one level inside a FLIP3, return to previous player
                            uint8_t returnPlayer = returnPlayerStack[stackPointer];
                            stackPointer--;
                            moveToPlayer(returnPlayer);
                        }
                        if (gameFlags.isDealing == true) {       // if game is in the dealing state, proceed dealing to remaining players
                            proceedDealing();
                        } else {
                            advanceToNextActivePlayer();        // if not dealing, advance to next active player
                            gameState = ACTION;
                        }
                    }               
                }
                break;

            case PICKSPECIAL:               //  player picks which type of special card they have
                if (button == Buttons::RED) {
                    //go back           return to previous state
                    specialState = NONE;
                    gameFlags.isDisplayingSelection = false;
                    gameState = prevState;
                    
                } else if (button == Buttons::YELLOW) {
                    //Freeze                    choose freeze
                    specialState = FREEZE;
                    displayFace("FRZE");            //when the button is pressed, the screen changes
                    gameFlags.isDisplayingSelection = true;

                } else if (button == Buttons::BLUE) {
                    //Flip3                     choose flip3
                    specialState = FLIP3;
                    displayFace("FLP3");            //when the button is pressed, the screen changes
                    gameFlags.isDisplayingSelection = true;

                } else if (button == Buttons::GREEN) {
                    //Enter                 proceed with choice
                    if (specialState != NONE) {         // only go to next state if a special state has been selected
                        gameFlags.isDisplayingSelection = false;
                        displayedPlayerIndex = currentPlayerIndex;
                        gameState = PICKPLAYER;
                    }
                }
                break;

            case PICKPLAYER:                //  choose player to recieve impace of special card
                if (button == Buttons::RED) {
                    //go back               return to pickspecial
                    specialState = NONE;
                    gameFlags.isDisplayingSelection = false;
                    gameState = PICKSPECIAL;
                    
                } else if (button == Buttons::YELLOW) {
                    //cycle active players down
                    uint8_t nextIndex = 0;
                    if (cycleActivePlayer(displayedPlayerIndex, DOWN, nextIndex)){      //if there is an active player, set displayedPlayerIndex to the prev one
                        displayedPlayerIndex = nextIndex;
                        const char* colorName = getColorName(playerColors[displayedPlayerIndex]);       //get player color name
                        displayFace(colorName);                                                 //display player color
                        gameFlags.isDisplayingSelection = true;
                    }

                } else if (button == Buttons::BLUE) {
                    //cycle active players up
                    uint8_t nextIndex = 0;
                    if (cycleActivePlayer(displayedPlayerIndex, UP, nextIndex)){        //if there is an active player, set displayedPlayerIndex to the next one
                        displayedPlayerIndex = nextIndex;
                        const char* colorName = getColorName(playerColors[displayedPlayerIndex]);       //get player color name
                        displayFace(colorName);                                             //display player color
                        gameFlags.isDisplayingSelection = true;
                    }

                } else if (button == Buttons::GREEN) {
                    //Enter                     proceed with selected player
                    if (gameFlags.isDisplayingSelection) {     // only proceed if a player has been selected
                        gameFlags.isDisplayingSelection = false;
                        if (specialState == FREEZE){            //action to take if freeze selected
                            specialState = NONE;
                            //make selected player inactive
                            setIsNotActive(displayedPlayerIndex);       //make selected player inactive
                            if (areActivePlayers()) {               // if there remains active players
                                if (stackPointer == -1) {           // if not inside of of flip3
                                    if (gameFlags.isDealing) {      // if in dealing phase, proceed dealing
                                        proceedDealing();
                                    } else {                            // if not in dealing phase move to next active player
                                        //move to next active player
                                        advanceToNextActivePlayer();
                                        gameState = ACTION;
                                    }
                                } else {                           //if inside of a flip3, return to prev player pick screen (in case they have more spec cards from a flip3)
                                    gameState = PICK;
                                }
                            } else {                                // if no active players remain, end the round
                                spin("END ROUND SCORING", spin_normal);
                                gameFlags.isDisplayingSelection = false;
                                gameFlags.isDealing = false;
                                gameState = ENTERSCORE;
                            }
                        }
                        if (specialState == FLIP3){             //action to take if flip3 selected
                            specialState = NONE;
                            if (stackPointer < MAX_FLIP3_DEPTH - 1) {        // increment stack if flip3 max depth hasn't been reached
                                stackPointer++;
                                returnPlayerStack[stackPointer] = currentPlayerIndex;  
                            }                    
                            moveToPlayer(displayedPlayerIndex);         //move to selected player
                            dispenseCards(3);                           //deal 3 cards
                            gameState = PICK;                           //pick screen
                        }
                    }
                }
                break;

            case ENTERSCORE:                        // enter each player's scores
                if (button == Buttons::RED) {
                    // flip negative and positive increment if in adjust mode
                    if (gameFlags.isAdjScore) {
                        gameFlags.adjSign = !gameFlags.adjSign;
                    }

                } else if (button == Buttons::YELLOW) {
                    //add 10 to score
                    if (gameFlags.isDisplayingSelection){           // if screen is displaying color and not in initial scroll screen
                        incrementScoreByTen(currentPlayerIndex);    // add 10 to score, increments up until max round score and cycles back to 0
                        displayPlayerScore(currentPlayerIndex);
                    }

                } else if (button == Buttons::BLUE) {
                    //add 1 to score
                    if (gameFlags.isDisplayingSelection){           // if screen is displaying color and not in initial scroll screen
                        cycleOnesDigit(currentPlayerIndex);         // add 1 to score, increments through 0-9
                        displayPlayerScore(currentPlayerIndex);
                    }

                } else if (button == Buttons::GREEN) {
                    //confirm and move to next player
                    if (gameFlags.isDisplayingSelection == false) {     // if in initial scroll screen, proceed to entering scores
                        if (!moveToNextunBustedPlayer((startPlayerIndex - 1 + numPlayers)%numPlayers)) {  // move to 1st unbusted player
                            gameState = REPORTSCORE;            // If no unbusted players found, everyone busted and no scores to enter. Move to reportscore
                            gameFlags.isAdjScore = false;
                            break;
                        }
                        gameFlags.isDisplayingSelection = true;
                        displayPlayerScore(currentPlayerIndex);
                    } else {                                // if unbusted players exist
                        setIsBust(currentPlayerIndex);          //set currentplayer to busted (this tracks that we've now entered their score)
                        if (moveToNextunBustedPlayer()) {           //move to next unbusted player
                            displayPlayerScore(currentPlayerIndex);
                        } else {                                        //after everyone entered scores, tally scores and check for winner
                            gameFlags.isDisplayingSelection = false;
                            gameFlags.isAdjScore = false;
                            gameState = REPORTSCORE;
                            tallyScores();                          //add current round scores to total scores
                            uint8_t winner = checkForWinner();      // check for winner
                            if (winner != 255){                     // if winner end game, otherwise move to REPORTSCORE
                                char winnerMessage[9];
                                const char* colorName = getColorName(playerColors[winner]);
                                snprintf(winnerMessage, sizeof(winnerMessage), "WIN %s", colorName);
                                spin(winnerMessage, spin_win);          //display winner's color and spin
                                moveToPlayer(winner);
                                gameState = GAMEOVER;
                            } 
                        }
                    }
                }
                break;

            case REPORTSCORE:                   // options to adjust scores, show scores, or move to next round
                if (button == Buttons::RED) {
                    //adjust scores  - currently only let's you add to score
                    setAllPlayersNotBust(MAX_PLAYERS)   //set all players to not busted - this is so we can reuse enterscore state cycle through all players 
                    gameFlags.isDisplayingSelection = false;
                    gameFlags.isAdjScore = true;
                    gameFlags.adjSign = 0;
                    gameState = ENTERSCORE;

                } else if (button == Buttons::YELLOW || button == Buttons::BLUE) {
                    //send to showscores
                    gameFlags.isDisplayingSelection = false;
                    prevState = REPORTSCORE;
                    gameState = SHOWSCORES;

                } else if (button == Buttons::GREEN) {
                    //start new round
                    setAllPlayersNotBust(MAX_PLAYERS)               //reset bust
                    setPlayersActiveIfPlaying(MAX_PLAYERS)          //reset active
                    setAllPlayersNotDealt(MAX_PLAYERS)              //reset dealt
                    stackPointer = -1;
                    startPlayerIndex = (startPlayerIndex +1) % numPlayers;       //increment starting player by one
                    moveToPlayer(startPlayerIndex);
                    gameFlags.isDealing = true;
                    dealOne();
                    gameState = DEALSPECIAL;                        //return to DEALSPECIAL to start new round
                }
                break;

            case SHOWSCORES:                    //cycle through players and show their scores
                if (button == Buttons::RED) {
                    //go back
                    gameState = prevState;
                    gameFlags.isDisplayingSelection = false;
                    
                } else if (button == Buttons::YELLOW) {
                    //increment player down
                    if (!gameFlags.isDisplayingSelection){          //if no player shown yet, show 1st player color
                        displayedPlayerIndex = 0;
                        gameFlags.isShowingScore = false;
                        gameFlags.isDisplayingSelection = true;
                        displayFace(getColorName(playerColors[displayedPlayerIndex]));
                    } else {                                        // increment player number down and show their color
                        displayedPlayerIndex = (displayedPlayerIndex - 1 + numPlayers) % numPlayers;
                        gameFlags.isShowingScore = false;
                        displayFace(getColorName(playerColors[displayedPlayerIndex]));
                    }
                } else if (button == Buttons::BLUE) {
                    //increment player up
                    if (!gameFlags.isDisplayingSelection){          //if no player shown yet, show 1st player color
                        displayedPlayerIndex = 0;
                        gameFlags.isShowingScore = false;
                        gameFlags.isDisplayingSelection = true;
                        displayFace(getColorName(playerColors[displayedPlayerIndex]));
                    } else {                                        // increment player number down and show their color
                        displayedPlayerIndex = (displayedPlayerIndex + 1) % numPlayers;
                        gameFlags.isShowingScore = false;
                        displayFace(getColorName(playerColors[displayedPlayerIndex]));
                    }
                } else if (button == Buttons::GREEN) {
                    //switch between color and score
                    if (gameFlags.isDisplayingSelection) {          // if player showing switch between showing their color and score
                        gameFlags.isShowingScore = !gameFlags.isShowingScore;
                        if (gameFlags.isShowingScore) {
                            snprintf(displayBuffer, sizeof(displayBuffer), "%4d", playerScores[displayedPlayerIndex]);
                            displayFace(displayBuffer);                 //display score
                        } else {
                            displayFace(getColorName(playerColors[displayedPlayerIndex]));      //display color
                        }
                    }
                }
                break;

            case GAMEOVER:          // options when game ends
                if (button == Buttons::RED) {
                    // main menu
                    gameFlags.isDisplayingSelection = false;
                    flags4.gamesExit = true;
                    currentDealState = RESET_DEALR;
                    
                } else if (button == Buttons::YELLOW) {
                    //show scores
                    gameFlags.isDisplayingSelection = false;
                    prevState = GAMEOVER;
                    gameState = SHOWSCORES;

                } else if (button == Buttons::BLUE) {
                    //show scores
                    gameFlags.isDisplayingSelection = false;
                    prevState = GAMEOVER;
                    gameState = SHOWSCORES;

                } else if (button == Buttons::GREEN) {
                    //new game
                    gameFlags.isDisplayingSelection = false;
                    initialize();
                    gameState = STARTUP;
                }
                break;
        }
    }

    void handleAwaitDecisionDisplay() override {
        // First, check if the face is locked and if 1.5 seconds have passed
        if (gameFlags.isDisplayingSelection){
            return;
        }
        // Now, call the original function from the Game base class.
        // It will automatically handle the scrolling text, but only if the face is unlocked.
        Game::handleAwaitDecisionDisplay();
    }


  private:
    uint8_t numPlayers = 0;  // number of players in the game
    const uint16_t minScore = 200; // Min score allowed
    uint16_t ScoretoWin = minScore; // Default score to play to - adjust this in startup screen
    const uint16_t maxScore = 990; // Max score allowed
    const int16_t maxRoundScore = 171;  //maximum that can be achieved in 1 round
    char displayBuffer[5];
    struct {
        uint8_t isDisplayingSelection : 1;      //true if overriding scrolling message
        uint8_t isShowingScore : 1;             //true when showing player score vs their color
        uint8_t isSpinning : 1;                 //true when machine is spinning
        uint8_t isDealing : 1;                  //true when machine is performing initial deal of one card to everyone
        uint8_t isAdjScore : 1;                 //true when in adjust score mode
        uint8_t adjSign : 1;                    //false is positive and true is negative
    } gameFlags;

    GameState gameState = STARTUP; // game starts in startup state
    SpecialState specialState = NONE;  //game starts with no selection
    GameState prevState = REPORTSCORE;  //used for moving backwards after showing scores

    // player tracking arrays
    int16_t playerScores[MAX_PLAYERS];  // holds each player's score
    int16_t currentRoundScores[MAX_PLAYERS];  //holds current round scores
    uint8_t playerColors[MAX_PLAYERS];  // holds each player's color
    uint8_t playerStatus[MAX_PLAYERS];  // holds each player's status (isplaying, isactive, isbust, isdealt))

    uint8_t currentPlayerIndex = 0;         //index of who the machine is pointing to
    uint8_t startPlayerIndex = 0;           //index of player who started the round
    uint8_t displayedPlayerIndex = 0;       //if display is showing a color, this is the index of that player
    static const uint8_t MAX_FLIP3_DEPTH = 4;       //maximum amount of flip3 that can occur in a row
    uint8_t returnPlayerStack[MAX_FLIP3_DEPTH];  // stack for returning to playerindex after flip3
    int8_t stackPointer = -1;                       // tracks indexes in returnPlayerStack,  -1 for empty

    //spin durations
    const uint16_t spin_normal = 4000;      //ms
    const uint16_t spin_win = 8000;         //ms

    void advanceToNextActivePlayer(){
        //find the next active player and move there
        if (numPlayers == 0) return;

        uint8_t nextPlayer = currentPlayerIndex;
        //loop until we find next active player
        do {
            nextPlayer = (nextPlayer + 1) % numPlayers;
        } while (!isPlayerActive(nextPlayer) && nextPlayer != currentPlayerIndex);   //if no active except current player, it will return to currentplayer

        moveToPlayer(nextPlayer);
    }

    void moveToFirstActivePlayer(){
        //  starting with the startPlayer, move to the 1st active player it finds
        uint8_t nextPlayer = startPlayerIndex;
        if(areActivePlayers()){
            while (!isPlayerActive(nextPlayer)){
                nextPlayer = (nextPlayer +1) % numPlayers;
            }
        moveToPlayer(nextPlayer);
        }
    }

    void advanceNextActiveUndealtPlayer() {
        // move to the next active player who hasn't been dealt yet
        uint8_t nextPlayer = currentPlayerIndex;
        do {
            nextPlayer = (nextPlayer + 1) % numPlayers;
        } while (!isPlayerActive(nextPlayer) || isPlayerDealt(nextPlayer));

        moveToPlayer(nextPlayer);
    }

    void advanceOnePosition() {
        // moves machine forward one tag position
        moveOffActiveColor(CW);   //get started by moving into black
        rotate(mediumSpeed, CW);    //rotate at medium speed to ensure reading of colors
        while (activeColor == 0) {       //keep rotating until the active color is not black
            if (gameFlags.isSpinning) {
                updateScrollText();         //if in spinning mode, keep updating the scrolling text
            }
            colorScan();                //continuosly check the color until not black
        }
        //activecolor > 4 are tags I printed.  These are wider than the standard tags
        //It needs to keep rotating a little longer to get to the center of the tag to avoid an edge reading
        if (activeColor > 4){
            delay(75);                  
        }
        delay(10);  //all tags rotate a little longer to get to center of tag to avoid edge readings
        rotateStop();
        for (uint8_t i = 0; i<15; i++) {
            colorScan();                    //after stopping, perform color scan multiple times to get a stable color
        }
    }

    void RegisterPlayers() {
        // this function finds all players and registers them in the player arrays
        delay(20);
        for (uint8_t i = 0; i<15; i++) {
            colorScan();            //at the start, perform color scan to get a stable color for first tag
        }
        uint8_t startingColor = activeColor;
        startPlayerIndex = 0;
        do {
            if (numPlayers < MAX_PLAYERS) {
                playerColors[numPlayers] = activeColor;     // Store the color of the player
                playerScores[numPlayers] = 0;               // Initialize player score to 0
                playerStatus[numPlayers] = IS_PLAYING;      // Set player as playing
                numPlayers++;
            }
            gameFlags.isDisplayingSelection = true;
            displayFace(getColorName(playerColors[numPlayers - 1]));    //display players color for confirmation
            delay(400);
            advanceOnePosition(); // Move to the next position
        } while (activeColor != startingColor);         //keep advancing until start color is seen again
        currentPlayerIndex = 0;
        gameFlags.isDisplayingSelection = false;
    }    

    bool areActivePlayers() const {
        // returns true if there are any active players remaining
        for (uint8_t i = 0; i<numPlayers; i++) {
            if (isPlayerActive(i)){
                return true;
            }
        }
        return false;
    }

    bool areActiveandUndealt() const {
        // returns true if there are any active and undealt players remaining
        for (uint8_t i = 0; i<numPlayers; i++){
            if (isPlayerActive(i) && !isPlayerDealt(i)){
                return true;
            }
        }
        return false;
    }

    bool cycleActivePlayer(uint8_t currentIndex, CycleDirection direction, uint8_t& nextPlayerIndex) const {
        //based on input variable either cycles up or down through the players
        //sets the nextPlayerIndex to be the next active player (up or down).
        //returns true if an active player found
        if (numPlayers == 0) {
            return false;
        }
        for (uint8_t i = 1; i <= numPlayers; i++) {
            uint8_t searchIndex;

            if (direction == UP) {
                searchIndex = (currentIndex +i) % numPlayers;
            } else { //down
                searchIndex = (currentIndex -i + numPlayers) % numPlayers;
            }

            if (isPlayerActive(searchIndex)) {
                nextPlayerIndex = searchIndex;
                return true;
            }
        }
        return false;
    }

    const char* getColorName(uint8_t colorValue) const {
        //returns color name for displaying
        // A static buffer is used to prevent issues with pointer scope.
        static char nameBuffer[5];

        // First, check if the requested colorValue is a valid index for our array.
        // This prevents the program from crashing if it receives a bad value.
        if (colorValue >= 1 && colorValue < TOTAL_COLORS) {
            // Since the array is in standard SRAM, we can access it directly.
            return colorNames[colorValue];
        }

        // If the colorValue is invalid, return a clear error message.
        // This will help debug issues if an unexpected value is passed.
        snprintf(nameBuffer, sizeof(nameBuffer), "E %d", colorValue);
        return nameBuffer;
    }

    bool moveToPlayer(uint8_t targetPlayerIndex) {
        // moves machine to the targeted player index
        if (targetPlayerIndex >= numPlayers) {
            return false;
        }
        uint8_t targetColor = playerColors[targetPlayerIndex];

        if (activeColor == targetColor){            //check to see if already at desired player
            currentPlayerIndex = targetPlayerIndex;
            return true;
        }

        while (activeColor != targetColor) {        //keep advancing one position until target player found
            advanceOnePosition();
        }
        currentPlayerIndex = targetPlayerIndex; // Update the current player index
        return true;
    }

    void spin(const char* message, uint16_t spinDuration) {
        // spin machine the desired time while scrolling message
        gameFlags.isSpinning = true;
        startScrollText(message, textStartHoldTime, textSpeedInterval, textEndHoldTime);  //Start the scrolling text using variables from Config.h

        if (spinDuration == 0){
            stopScrollText();
            gameFlags.isSpinning = false;
            return;
        }

        rotate(highSpeed, CW);              //spin at high speed
        unsigned long startTime = millis();

        while (millis() - startTime < spinDuration) {
            updateScrollText();
            delay(1);
        }
        rotateStop();
        delay(50);

        stopScrollText();
        gameFlags.isSpinning = false;
    }

    bool moveToNextunBustedPlayer(uint8_t startIndex = 255){
        // find next unbusted player and move there
        // startIndex input allows you to start search from a specific playerIndex.  default to currentPlayerIndex
        //returns true if there an unbusted player to move to

        if (startIndex == 255) {
            startIndex = currentPlayerIndex;
        }
        if (numPlayers == 0){
            return false;
        }
        for (uint8_t i=1; i<=numPlayers; i++){
            uint8_t searchIndex = (startIndex + i) % numPlayers;
            if (isPlayerPlaying(searchIndex) && !isPlayerBust(searchIndex)){
                moveToPlayer(searchIndex);
                return true;
            }
        }
        return false;
    }

    void displayPlayerScore(uint8_t playerIndex) {
        //displays the score playerIndex
        int16_t score = currentRoundScores[playerIndex];
        snprintf(displayBuffer, sizeof(displayBuffer), "%4d", score);
        displayFace(displayBuffer);
    }

    void incrementScoreByTen(uint8_t playerIndex) {
        //Increment the currentroundScore of playerIndex by 10.  If greater than the max, go back to 0
        if (gameFlags.isAdjScore && gameFlags.adjSign) {  // if in adjust mode and sign is negative subtract 10
            currentRoundScores[playerIndex] -= 10;
        } else {                                        // otherwise add 10
            currentRoundScores[playerIndex] += 10;
        }
        //make sure maximum score change isn't more or less than max
        if (currentRoundScores[playerIndex] > maxRoundScore || currentRoundScores[playerIndex] < -maxRoundScore) {
            currentRoundScores[playerIndex] = 0;
        }
    }

    void cycleOnesDigit(uint8_t playerIndex) {
        //Increments the currentRoundScore of playerIndex by 1.  If more than 9 entered, cycles back to 0 for the ones digit
        int16_t score = currentRoundScores[playerIndex];

        // Isolate the tens and ones digits
        int16_t tens = (score / 10) * 10; // Example: if score is 54, tens = 50
        int16_t ones = score % 10;        // Example: if score is 54, ones = 4

        // Increment the ones digit, wrapping from 9 back to 0
        if (gameFlags.isAdjScore && gameFlags.adjSign) {        // if in adjust mode and sign is negative subtract 1
            if (score > 0){
                if (ones > 0){
                    ones--;
                } else {
                    ones = 9;
                }
            } else {
                if (ones > -9) {
                    ones--;
                } else {
                    ones = 0;
                }
            }
        } else {                                                // otherwise add 1
            if (score >= 0) {
                ones = (ones + 1) % 10;    
            } else {
                if (ones < 0) {
                    ones++;
                } else {
                    ones = -9;
                }
            }
        }

        // Calculate the potential new score
         currentRoundScores[playerIndex] = tens + ones;

        // Ensure the new score does not exceed the maximum round score
        if ( currentRoundScores[playerIndex] > maxRoundScore  || currentRoundScores[playerIndex] < -maxRoundScore) {
            // If it does, just use the tens value (e.g., if max is 171 and score
            // was 169, the next score should be 160, not an invalid 170).
            currentRoundScores[playerIndex] = tens;
        } 
    }

    void tallyScores() {
        // add currentRoundScores to the total playerScores.  then reset currentRoundScore
        for (uint8_t i=0; i<numPlayers; i++) {
            playerScores[i] += currentRoundScores[i];
            if (playerScores[i] < 0){
                playerScores[i] = 0;
            }
        }
        memset(currentRoundScores, 0, sizeof(currentRoundScores));
    }

    uint8_t checkForWinner() const {
        //check to see if anyone has more than the ScoreToWin.  returns player with highest score or 255 if no winner
        uint8_t winnerIndex = 255;
        uint16_t highestWinningScore = 0;

        for (uint8_t i=0; i < numPlayers; i++) {
            if (playerScores[i] >= ScoretoWin && playerScores[i] > highestWinningScore) {
                highestWinningScore = playerScores[i];
                winnerIndex = i;
            }
        }
        return winnerIndex;
    }

    void dealOne() {
        // deal one card to current player and set status to isdealt
        dispenseCards(1);
        delay(500);
        setIsPlayerDealt(currentPlayerIndex);
    }

    void proceedDealing() {
        //checks to see if there are active and undealt players, if so advances to next one and deals a card
        // if no active and undealt, move to ACTION state
        if (areActiveandUndealt()) {  
            advanceNextActiveUndealtPlayer();
            dealOne();
            gameState = DEALSPECIAL;
        } else {
            moveToFirstActivePlayer();
            gameState = ACTION;
            gameFlags.isDealing = false;
        }
    }

};

#endif // FLIP7_H
