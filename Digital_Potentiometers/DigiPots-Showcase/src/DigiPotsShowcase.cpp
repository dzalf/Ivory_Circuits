#include <Arduino.h>
#include <Bounce2.h>
#include <Encoder.h>
#include <U8g2lib.h>

#include "MCP40xx.h"
#include "TPL0102.h"

/*
*   Definitions
*/
#define TPL0102_ADDRESS 0x50
#define FIRST_ROW 25
#define SECOND_ROW 38
#define THIRD_ROW 50
#define VALUE_COLUMN 47
#define DRAW 1
#define CLEAR 0
#define DEBUG true

// Measured values
#define TPL0102_RES 95700.0
#define MCP4011_RES 49220.0
#define MCP4013_RES 49690.0
// end of defintions

// OLED Screen members
// SPI Pinout
const int DC_Pin = 15;
const int RST_Pin = 14;
const int CS_Pin = 10;

// TPL0102 Members
const int ledA = 20;
const int ledB = 21;
const int chanSel_Pin = 6;

byte channelTPL;

// MCP4011 Members
typedef struct mcp40xx_pins {
    const int UD;
    const int CS;
} MCP40xx_pins;

MCP40xx_pins pins4011{23, 22};
MCP40xx_pins pins4013{17, 16};

// Rotary encoder
long oldPosition = 0;

const int SCK_Pin = 9;
const int DT_Pin = 8;
const int SW_Pin = 7;

/*
* Showcase code members
*/

byte potIndex = 0;
byte chanPtr = 0;

int tapPtr[3] = {0, 0, 0};

float values[3] = {};
byte digiPotLocations[3] = {FIRST_ROW, SECOND_ROW, THIRD_ROW};

bool fromRotary = false;
bool fromIncrement = false;
bool fromDecrement = false;
bool fromRotarySw = false;

const char* digiPotLabels[3] = {"TPL0102", "MCP4011", "MCP4013"};
const char* channelLabels[2] = {"A", "B"};

unsigned long pollRotTime;
unsigned long pollRotSWTime;
unsigned long pollDisplayTime;
unsigned long pollChannelSelectTime;

const unsigned long rotaryDelay = 50;
const unsigned long rotarySWDelay = 10;
const unsigned long displayDelay = 40;
const unsigned long channelButtonDelay = 10;

// Function protoypes

void buttonsSetup(void);             // Sets up the push buttons
void setupDigiPots(void);            // Sets up the digipots
void beginDigiPots(void);            // Issue a begin command to the digipots
void zeroWipers(void);               // Set the wipers/taps to the minimum
void displaySerialBanner(void);      // Initial welcome serial message
void displayResistanceValues(void);  // Print on the serial port the approximate (computed) resistance value
void setupInitialScreen(void);       // Sets all the static labels from the OLED screen
void selectiveIncrement(void);       // Depending on the current variables conditions set the tap conter and digipot values
void selectiveDecrement(void);
void highlighterFrame(uint8_t, byte);  // Handle the selection of the currently selected digipot
void tapValueHandler(void);            // Manage the values of the taps within their nominal limits
void displayChannel(void);

// Objects instantiation
Encoder enc(DT_Pin, SCK_Pin);
Bounce encSw = Bounce();
Bounce chanSw = Bounce();

U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI disp(U8G2_R0,
                                             CS_Pin, /* cs=*/
                                             DC_Pin, /* dc=*/
                                             RST_Pin /* reset=*/
);
// Works!

// Encapsulate the three digital potentiometers
// in one structure

struct digital_pots {
    TPL0102 tpl = TPL0102(ledA, ledB, DEBUG);
    MCP40xx MCP4011 = MCP40xx(pins4011.CS, pins4011.UD, DEBUG);
    MCP40xx MCP4013 = MCP40xx(pins4013.CS, pins4013.UD, DEBUG);

} digiPots;

void setup() {
    //
    Serial.begin(6000000);  // Max Teensy 3.2 baudrate

    delay(1500);

    Serial.println(F("Initialization  messages: \n\n"));
    // Initialize the push buttons: encoder and channel select
    buttonsSetup();
    // Setup the connections and values from the three digipots
    setupDigiPots();
    // Issue a begin command to all digipots
    beginDigiPots();
    // start this demo at minimum values
    zeroWipers();

    delay(1500);
    // Once everything is set, send a cute initial screen on the serial port
    displaySerialBanner();

    setupInitialScreen();

    pollRotTime = 0;
    pollRotSWTime = 0;
    pollDisplayTime = 0;
    pollChannelSelectTime = 0;
}

void loop() {
    unsigned long now = millis();

    if (now - pollRotTime >= rotaryDelay) {
        pollRotTime = now;

        // Encoder reading routine
        long newPosition = enc.read();

        if (newPosition != oldPosition) {
            fromRotary = true;
            Serial.println("\n");
            // clear the printed portion of the screen
            disp.setDrawColor(0);
            disp.drawBox(105, FIRST_ROW, 12, 8);
            disp.drawBox(105, SECOND_ROW, 20, 8);
            disp.drawBox(112, THIRD_ROW, 10, 8);

            displayChannel();

            if (newPosition >= oldPosition) {
                fromIncrement = true;

            } else if (newPosition < oldPosition) {
                fromDecrement = true;
            }

            tapValueHandler();

            displayResistanceValues();

            Serial.println("\n");

            oldPosition = newPosition;
        }
    }

    if (now - pollChannelSelectTime >= channelButtonDelay) {
        pollChannelSelectTime = now;

        if (potIndex == 0) {  // Only monitor channel change if TPL0102 is selected
            chanSw.update();

            if (chanSw.fell()) {

                Serial.println("\n");
                disp.setDrawColor(0);
                disp.drawBox(111, THIRD_ROW, 15, 8);

                chanPtr++;

                if (chanPtr >= 2)
                    chanPtr = 0;

                digiPots.tpl.setChannel(chanPtr);
                digiPots.tpl.setTap(chanPtr, tapPtr[potIndex]);

                Serial.print(F("Channel Selected >> "));
                Serial.println(channelLabels[chanPtr]);

                displayChannel();

                Serial.println("\n");
            }
        }
    }
    if (now - pollRotSWTime >= rotarySWDelay) {
        pollRotSWTime = now;

        encSw.update();

        if (encSw.fell()) {
            fromRotarySw = true;

            disp.setDrawColor(0);
            disp.drawBox(108, SECOND_ROW, 16, 8);

            highlighterFrame(CLEAR, digiPotLocations[potIndex]);  // Erase the frame before next increment

            potIndex++;

            if (potIndex > 2) {
                potIndex = 0;
            } else if (potIndex >= 0) {
                disp.setDrawColor(0);
                disp.drawFrame(2, digiPotLocations[potIndex], 96, 11);
            }

            tapValueHandler();

            displayResistanceValues();

            highlighterFrame(DRAW, digiPotLocations[potIndex]);

            // Bug: when selecting the next pot, I lose part of the text
            disp.setDrawColor(1);
            disp.setFont(u8g2_font_5x8_mr);
            disp.drawStr(87, SECOND_ROW, "Tap:");

            disp.setFont(u8g2_font_5x8_mr);
            disp.drawStr(87, THIRD_ROW, "Chan:");

            Serial.print(F("Selected >> "));
            Serial.println(digiPotLabels[potIndex]);
        }
    }

    if (now - pollDisplayTime >= displayDelay) {
        pollDisplayTime = now;

        disp.sendBuffer();
    }
}

/*
******************************************
*/

void displaySerialBanner() {
    Serial.println(F("*****************************************"));
    Serial.println(F("      Digital Potentiometers Showcase    "));
    Serial.println(F("         TPL0102, MCP4011 & MCP4013      "));
    Serial.println(F("*****************************************"));
}

void setupDigiPots() {
    digiPots.MCP4011.setup();
    digiPots.MCP4013.setup();
}

void beginDigiPots() {
    digiPots.tpl.begin(TPL0102_ADDRESS,TPL0102_RES, FAST);
    digiPots.MCP4011.begin(MCP4011_RES);
    digiPots.MCP4013.begin(MCP4013_RES);
}

void zeroWipers() {
    // Zeroing both channels from the TPL0102 pot
    for (byte i = 0; i < 2; i++) {
        digiPots.tpl.zeroWiper(i);
        delay(100);
    }
    digiPots.tpl.setChannel(chanPtr);
    // Zero wipers from MCP40xx group
    digiPots.MCP4011.zeroWiper();
    digiPots.MCP4013.zeroWiper();

    values[0] = digiPots.tpl.readValue(chanPtr);
    values[1] = digiPots.MCP4011.readValue();
    values[2] = digiPots.MCP4013.readValue();
}

void displayChannel() {
    disp.setDrawColor(1);
    disp.setFont(u8g2_font_4x6_mr);
    disp.setCursor(115, THIRD_ROW + 1);
    disp.print(channelLabels[chanPtr]);
}

void setupInitialScreen() {
    disp.begin();
    disp.clearBuffer();
    disp.drawRFrame(0, 0, 128, 64, 2);

    //disp.drawRFrame(2,20,74,42,3);
    //disp.drawRFrame(76,20,50,42,3);

    disp.setFont(u8g2_font_smart_patrol_nbp_tf);  // choose a suitable font
    disp.setFontPosTop();
    disp.setDrawColor(1);
    disp.drawStr(9, 6, "IVORY CIRCUITS");  // write something to the internal memory
    disp.drawRFrame(5, 4, 118, 16, 3);

    disp.setFont(u8g2_font_5x8_mr);
    disp.drawStr(4, FIRST_ROW, "TPL0102: ");

    disp.setCursor(VALUE_COLUMN, FIRST_ROW + 1);
    disp.setFont(u8g2_font_4x6_mr);
    disp.print(values[0], 1);

    disp.setFont(u8g2_font_5x8_mr);
    disp.drawStr(87, FIRST_ROW, "Rot:");

    disp.setFont(u8g2_font_4x6_mr);
    disp.drawStr(110, FIRST_ROW, "---");

    disp.setFont(u8g2_font_5x8_mr);
    disp.drawStr(5, SECOND_ROW, "MCP4011: ");

    disp.setCursor(VALUE_COLUMN, SECOND_ROW + 1);
    disp.setFont(u8g2_font_4x6_mr);
    disp.print(values[1], 1);

    disp.setFont(u8g2_font_5x8_mr);
    disp.drawStr(87, SECOND_ROW, "Tap:");

    disp.setCursor(110, SECOND_ROW + 1);
    disp.setFont(u8g2_font_4x6_mr);
    disp.print(tapPtr[potIndex]);

    disp.setFont(u8g2_font_5x8_mr);
    disp.drawStr(5, THIRD_ROW, "MCP4013: ");

    disp.setCursor(VALUE_COLUMN, THIRD_ROW + 1);
    disp.setFont(u8g2_font_4x6_mr);
    disp.print(values[2], 1);

    disp.setFont(u8g2_font_5x8_mr);
    disp.drawStr(87, THIRD_ROW, "Chan:");

    disp.setCursor(115, THIRD_ROW + 1);
    disp.setFont(u8g2_font_4x6_mr);
    disp.print(channelLabels[chanPtr]);

    disp.setDrawColor(1);

    highlighterFrame(DRAW, digiPotLocations[potIndex]);

    disp.sendBuffer();
}

void highlighterFrame(uint8_t index, byte pos) {
    //
    switch (index) {
    case CLEAR:

        disp.setDrawColor(0);
        disp.drawHLine(5, pos + 8, 38);

        break;

    case DRAW:

        disp.setDrawColor(1);
        disp.drawHLine(5, pos + 8, 38);

        break;
    }
}

void tapValueHandler() {
    //
    if ((fromIncrement == true) || (fromRotarySw == true)) {
        if (potIndex == 0) {
            if (tapPtr[potIndex] < (int)TPL0102_TAP_NUMBER) {
                if (fromRotary == true) {  // If the change was issued by the encoder
                    tapPtr[potIndex] += 1;
                    digiPots.tpl.setTap(chanPtr, tapPtr[potIndex]);
                    Serial.println("Tap Increase");

                    Serial.print(F("Retrieved tap value >> "));
                    Serial.println(digiPots.tpl.taps(chanPtr));

                    Serial.print(F("Tap set time (us) >> "));
                    Serial.println(digiPots.tpl.setMicros());

                } else {  // If the change was issued by a change of the selected digipot
                    tapPtr[potIndex] = tapPtr[potIndex];
                }
            } else if (tapPtr[potIndex] >= (int)TPL0102_TAP_NUMBER) {
                tapPtr[potIndex] = (int)TPL0102_TAP_NUMBER;
                digiPots.tpl.setTap(chanPtr, tapPtr[potIndex]);

                Serial.print(F("Tap set time (us) >> "));
                Serial.println(digiPots.tpl.setMicros());
            }

            values[0] = digiPots.tpl.readValue(chanPtr);

        } else if (potIndex == 1) {
            if (tapPtr[potIndex] < (int)MCP40xx_TAP_NUMBER) {
                if (fromRotary == true) {  // If the change was issued by the encoder
                    tapPtr[potIndex] += 1;
                    digiPots.MCP4011.setTap(tapPtr[potIndex]);
                    Serial.println(F("Tap Increase"));

                    Serial.print(F("Retrieved tap value >> "));
                    Serial.println(digiPots.MCP4011.taps());

                    Serial.print(F("Tap set time (us) >> "));
                    Serial.println(digiPots.MCP4011.setMicros());

                } else if (tapPtr[potIndex] >= (int)MCP40xx_TAP_NUMBER) {
                    tapPtr[potIndex] = tapPtr[potIndex];
                }

            } else if (tapPtr[potIndex] >= (int)MCP40xx_TAP_NUMBER) {
                tapPtr[potIndex] = MCP40xx_TAP_NUMBER;
                digiPots.MCP4011.setTap(tapPtr[potIndex]);
                Serial.print(F("Tap set time (us) >> "));
                Serial.println(digiPots.MCP4011.setMicros());
            }

            values[1] = digiPots.MCP4011.readValue();

        } else if (potIndex == 2) {
            if (tapPtr[potIndex] < (int)MCP40xx_TAP_NUMBER) {
                if (fromRotary == true) {  // If the change was issued by the encoder
                    tapPtr[potIndex] += 1;
                    digiPots.MCP4013.setTap(tapPtr[potIndex]);
                    Serial.println(F("Tap Increase"));

                    
                    Serial.print(F("Retrieved tap value >> "));
                    Serial.println(digiPots.MCP4013.taps());

                    Serial.print(F("Tap set time (us) >> "));
                    Serial.println(digiPots.MCP4013.setMicros());


                } else {
                    tapPtr[potIndex] = tapPtr[potIndex];
                }
            } else if (tapPtr[potIndex] >= (int)MCP40xx_TAP_NUMBER) {
                tapPtr[potIndex] = MCP40xx_TAP_NUMBER;
                digiPots.MCP4013.setTap(tapPtr[potIndex]);

                Serial.print(F("Tap set time (us) >> "));
                Serial.println(digiPots.MCP4013.setMicros());
            }

            values[2] = digiPots.MCP4013.readValue();
        }

        disp.setDrawColor(1);
        disp.setFont(u8g2_font_4x6_mr);
        disp.drawStr(110, FIRST_ROW, "Inc");

        fromIncrement = false;
    }
    
    //
    if ((fromDecrement == true) || (fromRotarySw == true)) {
        if (potIndex == 0) {
            if (tapPtr[potIndex] > 0) {
                if (fromRotary == true) {
                    tapPtr[potIndex] -= 1;
                    digiPots.tpl.setTap(chanPtr, tapPtr[potIndex]);
                    Serial.println(F("Tap Decrease"));

                    Serial.print(F("Retrieved tap value >> "));
                    Serial.println(digiPots.tpl.taps(chanPtr));

                       Serial.print(F("Tap set time (us) >> "));
                    Serial.println(digiPots.tpl.setMicros());

                 

                } else {
                    tapPtr[potIndex] = tapPtr[potIndex];
                }
            }

            values[0] = digiPots.tpl.readValue(chanPtr);

        } else if (potIndex == 1) {
            if (tapPtr[potIndex] > 0) {
                if (fromRotary == true) {
                    tapPtr[potIndex] -= 1;
                    digiPots.MCP4011.setTap(tapPtr[potIndex]);

                    Serial.println(F("Tap Decrease"));

                    Serial.print(F("Retrieved tap value >> "));
                    Serial.println(digiPots.MCP4011.taps());

                    Serial.print(F("Tap set time (us) >> "));
                    Serial.println(digiPots.MCP4011.setMicros());

                } else {
                    tapPtr[potIndex] = tapPtr[potIndex];
                }
            }

            values[1] = digiPots.MCP4011.readValue();

        } else if (potIndex == 2) {
            if (tapPtr[potIndex] > 0) {
                if (fromRotary == true) {
                    tapPtr[potIndex] -= 1;

                    digiPots.MCP4013.setTap(tapPtr[potIndex]);

                    Serial.println(F("Tap Decrease"));

                    Serial.print(F("Retrieved tap value >> "));
                    Serial.println(digiPots.MCP4013.taps());
                    Serial.print(F("Tap set time (us) >> "));
                    Serial.println(digiPots.MCP4013.setMicros());

                } else {
                    tapPtr[potIndex] = tapPtr[potIndex];
                }
            }

            values[2] = digiPots.MCP4013.readValue();
        }

        disp.setDrawColor(1);
        disp.setFont(u8g2_font_4x6_mr);
        disp.drawStr(110, FIRST_ROW, "Dec");

        fromDecrement = false;
    }

    disp.setDrawColor(1);
    disp.setFont(u8g2_font_4x6_mr);
    disp.setCursor(110, SECOND_ROW + 1);
    disp.print(tapPtr[potIndex]);

    fromRotary = false;
    fromRotarySw = false;
}

void buttonsSetup() {
    pinMode(SW_Pin, INPUT);
    encSw.attach(SW_Pin);
    encSw.interval(15);

    pinMode(chanSel_Pin, INPUT);
    chanSw.attach(chanSel_Pin);
    chanSw.interval(15);
}

void displayResistanceValues() {
    //
    if (potIndex == 0) {
        disp.setDrawColor(0);
        disp.drawBox(VALUE_COLUMN, FIRST_ROW, 35, 10);
        disp.setCursor(VALUE_COLUMN, FIRST_ROW + 1);

    } else if (potIndex == 1) {
        disp.setDrawColor(0);

        disp.drawBox(VALUE_COLUMN, SECOND_ROW, 35, 10);
        disp.setCursor(VALUE_COLUMN, SECOND_ROW + 1);

    } else if (potIndex == 2) {
        disp.setDrawColor(0);
        disp.drawBox(VALUE_COLUMN, THIRD_ROW, 35, 10);
        disp.setCursor(VALUE_COLUMN, THIRD_ROW + 1);
    }

    disp.setDrawColor(1);
    disp.setFont(u8g2_font_4x6_mr);
    disp.print(values[potIndex], 1);

    Serial.print("Value ");
    Serial.print(digiPotLabels[potIndex]);

    if(potIndex == 0){
        Serial.print("[CH(");
        Serial.print(channelLabels[chanPtr]);
        Serial.print(F(")]"));
    }
    Serial.print(" >> ");
    Serial.println(values[potIndex], 1);
}

/*    DEPRECATED */
/*
void selectiveIncrement() {
    //
    if (potIndex == 0) {
        digiPots.tpl.setTap(chanPtr, tapPtr);
        values[0] = digiPots.tpl.readValue(chanPtr);

        Serial.print("Retrieved value >> ");
        Serial.println(digiPots.tpl.taps(chanPtr));

    } else if (potIndex == 1) {
        digiPots.MCP4011.setTap(tapPtr);  // .inc()
        values[1] = digiPots.MCP4011.readValue();
        Serial.print("Retrieved value >> ");
        Serial.println(digiPots.MCP4011.taps());

    } else if (potIndex == 2) {
        digiPots.MCP4013.setTap(tapPtr);
        values[2] = digiPots.MCP4013.readValue();
        Serial.print("Retrieved value >> ");
        Serial.println(digiPots.MCP4013.taps());
    }
}

void selectiveDecrement() {
    if (potIndex == 0) {
        digiPots.tpl.setTap(chanPtr, tapPtr);  // .dec()
        values[0] = digiPots.tpl.readValue(chanPtr);

    } else if (potIndex == 1) {
        digiPots.MCP4011.setTap(tapPtr);  // .dec();
        values[1] = digiPots.MCP4011.readValue();

    } else if (potIndex == 2) {
        digiPots.MCP4013.setTap(tapPtr);  // .dec()
        values[2] = digiPots.MCP4013.readValue();
    }

    disp.setDrawColor(1);
    disp.setFont(u8g2_font_4x6_mr);
    disp.drawStr(110, FIRST_ROW, "Dec");
    disp.print(tapPtr);

    Serial.print("Value ");
    Serial.print(digiPotLabels[potIndex]);
    Serial.print(" >> ");
    Serial.println(values[potIndex]);
}
*/
