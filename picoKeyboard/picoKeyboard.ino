/**************************************************************************
 * 
 * SSD1306 Display
 * Send Keys
 * RP2040 (pico)

 * Apache License Version 2.0, January 2004
 * Stuart Davies
 * https://github.com/stuartdd/Pico_Keyboard
 *
 */
#include <Keyboard.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FatFS.h>
#include <FatFSUSB.h>


#define SCREEN_ADDRESS 0x3C  ///< See datasheet for Address; 0x3C
#define SCREEN_WIDTH 128     // OLED display width, in pixels
#define SCREEN_HEIGHT 64     // OLED display height, in pixels
#define SCREEN_LINE 16
#define SCREEN_CHAR 10
#define QUAD_W 4
#define QUAD_H 6

#define LIVE_LED 22  // Actual Arduino pin number

#define BIT_ALL 255
#define BIT_NONE 0

#define IN_PIN_A 7  // Actual Arduino pin number
#define BIT_1 1     // Bit position in buttonBits. Must be 1,2,4,8,16,,,
#define QUAD_0 0    //

#define IN_PIN_B 8  // Actual Arduino pin number
#define BIT_2 2     // Bit position in buttonBits. Must be 1,2,4,8,16,,,
#define QUAD_1 1    //

#define IN_PIN_C 14  // Actual Arduino pin number
#define BIT_4 4      // Bit position in buttonBits. Must be 1,2,4,8,16,,,
#define QUAD_2 2     //

#define IN_PIN_D 6  // Actual Arduino pin number
#define BIT_8 8     // Bit position in buttonBits. Must be 1,2,4,8,16,,,
#define QUAD_3 3    //

// Menu height is 1 + 3 for large display, 7 for small as yellow is always 2 lines
#define MENU_HEIGHT_LARGE 4
#define MENU_HEIGHT_SMALL 7

#define MODE_SETUP 0
#define MODE_MENU 1
#define MODE_HID 2
#define MODE_PGM 3

// Screen rotation for LHS and RHS plug in.
#define ROTATE_LHS 0
#define ROTATE_RHS 2

// Unique Actions for mode, rotation and buttonBits
#define ACTION_NONE 0
#define ACTION_UP 1
#define ACTION_DOWN 2
#define ACTION_SEND 3
#define ACTION_CONFIG 4
#define ACTION_LINES 5
#define ACTION_ROTATE 6
#define ACTION_RESET 7
#define ACTION_PGM 0

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

int menuHeight = 0;  // Default menu height
int menuLine = 0;
bool menuMax = true;
uint8_t screenRotation = ROTATE_LHS;
int displayMode = MODE_SETUP;
int buttonBits = 0;
int tos = 0;
int oldTos = 0;
volatile bool updateScreen = true;  // Screen needs to be redrawn

volatile bool driveConnected = false;

void printDiag(String s) {
  if (Serial) {
    Serial.println(s);
  }
}
void unplug(uint32_t i) {
  (void)i;
  driveConnected = false;
  Serial.println("unplug");
}

// Called by FatFSUSB when the drive is mounted by the PC.  Have to stop FatFS, since the drive data can change, note it, and continue.
void plug(uint32_t i) {
  (void)i;
  driveConnected = true;
  Serial.println("plug");
}

// Called by FatFSUSB to determine if it is safe to let the PC mount the USB drive.  If we're accessing the FS in any way, have any Files open, etc. then it's not safe to let the PC mount the drive.
bool mountable(uint32_t i) {
  (void)i;
  return true;
}

struct option {
  String disp;
  String keys;
};

// Number of options. Not display specific
option options[] = {
  { "Opt 1", "A" },
  { "Opt 2", "B" },
  { "Opt 3", "C" },
  { "Opt 4", "D" },
  { "Opt 5", "E" },
  { "Opt 6", "F" },
  { "Opt 7", "G" },
  { "Opt 8", "H" },
  { "Opt 9", "I" }
};

int optionCount = sizeof(options) / sizeof(option);


bool scanButtons() {
  int bb = 0;
  if (digitalRead(IN_PIN_A) == LOW) {
    if (screenRotation == ROTATE_LHS) {
      bb = bb | BIT_1;
    } else {
      bb = bb | BIT_4;
    }
  }
  if (digitalRead(IN_PIN_B) == LOW) {
    if (screenRotation == ROTATE_LHS) {
      bb = bb | BIT_2;
    } else {
      bb = bb | BIT_8;
    }
  }
  if (digitalRead(IN_PIN_C) == LOW) {
    if (screenRotation == ROTATE_LHS) {
      bb = bb | BIT_4;
    } else {
      bb = bb | BIT_1;
    }
  }
  if (digitalRead(IN_PIN_D) == LOW) {
    if (screenRotation == ROTATE_LHS) {
      bb = bb | BIT_8;
    } else {
      bb = bb | BIT_2;
    }
  }
  buttonBits = bb;
  return (bb != 0);
}

void box(int lin, int quad) {
  display.drawRect(0, lin * SCREEN_LINE, SCREEN_CHAR, SCREEN_LINE, WHITE);
  if (screenRotation == ROTATE_RHS) {  // Rotate the Quadrant
    switch (quad) {
      case QUAD_0:
        quad = QUAD_1;
        break;
      case QUAD_1:
        quad = QUAD_0;
        break;
      case QUAD_2:
        quad = QUAD_3;
        break;
      case QUAD_3:
        quad = QUAD_2;
        break;
    }
  }
  switch (quad) {
    case QUAD_0:
      display.fillRect(2, lin * SCREEN_LINE + 2, QUAD_W, QUAD_H, WHITE);
      break;
    case QUAD_1:
      display.fillRect(QUAD_W, lin * SCREEN_LINE + 2, QUAD_W, QUAD_H, WHITE);
      break;
    case QUAD_2:
      display.fillRect(2, lin * SCREEN_LINE + (QUAD_H + 2), QUAD_W, QUAD_H, WHITE);
      break;
    case QUAD_3:
      display.fillRect(QUAD_W, lin * SCREEN_LINE + (QUAD_H + 2), QUAD_W, QUAD_H, WHITE);
      break;
  }
}

void resetScreen(int showButtons) {
  display.clearDisplay();
  display.setRotation(screenRotation);
  display.setCursor(0, 0);
  display.setTextSize(2);
  if ((showButtons & BIT_1) == BIT_1) {
    box(0, QUAD_0);
  }
  if ((showButtons & BIT_2) == BIT_2) {
    box(1, QUAD_1);
  }
  if ((showButtons & BIT_4) == BIT_4) {
    box(2, QUAD_2);
  }
  if ((showButtons & BIT_8) == BIT_8) {
    box(3, QUAD_3);
  }
}

void updateScreenPgm() {
  updateScreen = false;
  resetScreen(BIT_2 | BIT_4 | BIT_8);
  display.println("Program:");
  display.println(" Ident");
  display.println(" Upload");
  display.println(" Reset");
  display.display();
}

void updateScreenMenu() {
  updateScreen = false;
  resetScreen(BIT_ALL);
  display.println(" Reset");
  display.println(" Size");
  display.println(" Rotate");
  display.println(" Back");
  display.display();
}

void updateScreenHid() {
  updateScreen = false;
  resetScreen(BIT_NONE);
  menuLine = tos;
  display.println(options[menuLine].disp);
  if (menuMax) {
    display.setTextSize(1);
    menuHeight = MENU_HEIGHT_SMALL;
  } else {
    menuHeight = MENU_HEIGHT_LARGE;
  }
  if (menuHeight > optionCount) {
    menuHeight = optionCount;
  }
  for (int i = 1; i < menuHeight; i++) {
    menuLine++;
    if (menuLine >= optionCount) {
      menuLine = 0;
    }
    display.println(options[menuLine].disp);
  }
  display.display();
}

void sendKeys(String ch) {
  int len = sizeof(ch) / sizeof(char);
  for (int i = 0; i < len; i++) {
    Keyboard.press(ch.charAt(i));
    delay(10);
    Keyboard.releaseAll();
  }
}

void sendButtonPressed() {
  resetScreen(BIT_NONE);
  display.println("Sending...");
  display.println(options[tos].disp);
  display.display();
  sendKeys(options[tos].keys);
  delay(1000);
  updateScreen = true;
}

void setMode(int newMode) {
  if (displayMode == newMode) {
    return;
  }
  displayMode = newMode;
  updateScreen = true;
}

void upButtonPressed() {
  oldTos = tos;
  tos++;
  if (tos >= optionCount) {
    tos = 0;
  }
  if (oldTos != tos) {
    updateScreen = true;
  }
}

void downButtonPressed() {
  oldTos = tos;
  tos--;
  if (tos < 0) {
    tos = optionCount - 1;
  }
  if (oldTos != tos) {
    updateScreen = true;
  }
}

void reset() {
  resetScreen(BIT_NONE);
  display.println("Reset...");
  display.display();
  delay(1000);
  rp2040.reboot();
}

void rotateDisplay() {
  if (screenRotation == ROTATE_LHS) {
    screenRotation = ROTATE_RHS;
  } else {
    screenRotation = ROTATE_LHS;
  }
  setMode(MODE_HID);
}

void setLines() {
  resetScreen(BIT_NONE);
  menuMax = !menuMax;
  display.println("ZOOM");
  if (menuMax) {
    display.println("...8 Lines");
  } else {
    display.println("...4 Lines");
  }
  display.display();
  delay(1000);
  // Normal 1:1 pixel scale
  setMode(MODE_HID);
}

void errorCode(int c) {
  for (;;) {
    for (int i = 0; i < c; i++) {
      delay(200);
      digitalWrite(LIVE_LED, LOW);
      delay(200);
      digitalWrite(LIVE_LED, HIGH);
    }
    delay(1000);
  }
}

bool programButtons() {
  if (digitalRead(IN_PIN_C) == LOW) {
    screenRotation = ROTATE_RHS;
  }
  return (digitalRead(IN_PIN_A) == LOW) || (digitalRead(IN_PIN_C) == LOW);
}

void setup() {
  pinMode(IN_PIN_A, INPUT_PULLUP);
  pinMode(IN_PIN_B, INPUT_PULLUP);
  pinMode(IN_PIN_D, INPUT_PULLUP);
  pinMode(IN_PIN_C, INPUT_PULLUP);
  pinMode(LIVE_LED, OUTPUT);


  digitalWrite(LIVE_LED, HIGH);
  if (programButtons()) {
    digitalWrite(LIVE_LED, LOW);
    Serial.begin(9600);
    while (!Serial) {
      delay(1);
    }
    delay(5000);
    if (!FatFS.begin()) {
      Serial.println("FatFS initialization failed!");
      while (1) {
        delay(1);
      }
    }
    Serial.println("FatFS initialization done.");
    FatFSUSB.onUnplug(unplug);
    FatFSUSB.onPlug(plug);
    FatFSUSB.driveReady(mountable);
    // Start FatFS USB drive mode
    FatFSUSB.begin();
    Serial.println("FatFSUSB started.");

    while (programButtons()) {
      delay(500);
    }
    setMode(MODE_PGM);
  } else {
    setMode(MODE_HID);
  }

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    errorCode(4);
  }
  // Clear the buffer
  resetScreen(BIT_NONE);
  display.setTextColor(SSD1306_WHITE);  // Draw white text
  display.println("Setup...");
  display.display();
  digitalWrite(LIVE_LED, HIGH);
  delay(500);
  updateScreen = true;
  buttonBits = 0;
}

void loop() {
  if (scanButtons()) {
    digitalWrite(LIVE_LED, LOW);
    switch (displayMode) {
      case MODE_HID:
        if ((buttonBits & BIT_1) == BIT_1) {
          sendButtonPressed();
        } else {
          if ((buttonBits & BIT_2) == BIT_2) {
            upButtonPressed();
          } else {
            if ((buttonBits & BIT_4) == BIT_4) {
              setMode(MODE_MENU);
            } else {
              if ((buttonBits & BIT_8) == BIT_8) {
                downButtonPressed();
              }
            }
          }
        }
        break;
      case MODE_MENU:
        if ((buttonBits & BIT_1) == BIT_1) {
          reset();
        } else {
          if ((buttonBits & BIT_2) == BIT_2) {
            setLines();
          } else {
            if ((buttonBits & BIT_4) == BIT_4) {
              rotateDisplay();
            } else {
              if ((buttonBits & BIT_8) == BIT_8) {
                setMode(MODE_HID);
              }
            }
          }
        }
        break;
      case MODE_PGM:
        if ((buttonBits & BIT_2) == BIT_2) {
          printDiag("IDENT");
        } else {
          if ((buttonBits & BIT_4) == BIT_4) {
          } else {
            if ((buttonBits & BIT_8) == BIT_8) {
              reset();
            }
          }
        }
        break;
    }
    digitalWrite(LIVE_LED, HIGH);
  }

  if (updateScreen) {
    switch (displayMode) {
      case MODE_PGM:
        updateScreenPgm();
        break;
      case MODE_HID:
        updateScreenHid();
        break;
      case MODE_MENU:
        updateScreenMenu();
        break;
    }
  }

  while (scanButtons()) {
    delay(100);
  }

  delay(100);
}