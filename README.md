# 🤖 FLIP7-DEALERBOT

A modified version of the Crunchlabs Dealerbot firmware designed to play the card game **FLIP7**. This code allows you to play with up to 8 players using the Hack Pack DEALR BOT.

---

## Key Features & Changes

This project is based on the modularized UNO code from WKoA. To adapt it for FLIP7 and optimize it for an Arduino Nano, I've made the following changes:

* **🃏 Added Game Logic:** The `games` folder now contains the complete logic for FLIP7.
* **🧠 Memory Optimized:** To fit on an Arduino Nano, all other games and unused functions from the original `.ino` file have been removed.
* **⚙️ Code Refactored:** Some of the main dealer code has been refactored to use less memory.
* **🧩 CAD Files Included:** Includes a 3D printable insert to adapt the dealer for the narrower FLIP7 cards.

---

## 🛠️ Installation & Setup

Follow these steps to get the game running on your Dealerbot.

1.  **Download the Code**
    * Click the green `Code` button on this page and select `Download ZIP`, or clone the repository.

2.  **Setup Arduino IDE**
    * You must configure your Arduino IDE to work with the Hack Pack hardware. Follow the setup instructions provided on the Crunchlabs Discord.
    * > **Note:** The following Discord link is only accessible to members of the Crunchlabs Discord server. [Link to Arduino Setup Instructions](https://discord.com/channels/1229106258749948056/1367718391196029030/1367718391196029030).

3.  **Upload to Dealerbot**
    * Open the `Flip7DealerMain.ino` file in the Arduino IDE.
    * Connect your Dealerbot and upload the sketch.

---

## 🖨️ Hardware Modifications

### Card Tray Adapter
FLIP7 cards are narrower than standard playing cards. To ensure they deal correctly:
* 3D print the `.stl` file located in the `/CAD` folder.
* Place the printed insert into the dealer's card tray.
* **Only place about half of the deck in the dealer at a time.** The weight of a full deck can cause dealing issues.

### Expanding to 8 Players
To play with more than four players, you will need to create new colored player tags.
* An `.stl` file for new tags was created by `belugafan` on the Crunchlabs Discord. [Link to Tag .stl File](https://discord.com/channels/1229106258749948056/1366237526259531858).
* Modify the `ColorNames.h` file as instructed in the file's comments to define your new colors. The 8 colors currently in the file are a good starting point.

> ### ⚠️ **VERY IMPORTANT** ⚠️
> After changing the colors in `ColorNames.h`, you **must** retune the dealer's color sensor through the built-in tuning menu for the new tags to be recognized correctly.

---

## 🙏 Acknowledgements

A huge thank you to **WKoA** for the original modularized Card Dealer code which served as the foundation for this project. You can find the original repository [here](https://github.com/Reginald-Gillespie/CardDealerModularized/releases/tag/v1.2.1).
