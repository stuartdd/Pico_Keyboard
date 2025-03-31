/**************************************************************************
 * 1306_simple_pico.ino
 * 
 * SSD1306 checkout
 * 
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define SCREEN_LINE 16
#define SCREEN_CHAR 10
#define QUAD_W 4
#define QUAD_H 6

#define SCREEN_ADDRESS 0x3C  ///< See datasheet for Address; 0x3C

#define LIVE_LED 22   // Actual Arduino pin number
#define IN_PIN_UP 8   // Actual Arduino pin number
#define BIT_PIN_UP 1  // Bit position in buttonBits. Must be 1,2,4,8,16,,,

#define IN_PIN_DOWN 6   // Actual Arduino pin number
#define BIT_PIN_DOWN 2  // Bit position in buttonBits. Must be 1,2,4,8,16,,,

#define IN_PIN_SEL 7   // Actual Arduino pin number
#define BIT_PIN_SEL 4  // Bit position in buttonBits. Must be 1,2,4,8,16,,,

#define IN_PIN_ALT 14  // Actual Arduino pin number
#define BIT_PIN_ALT 8  // Bit position in buttonBits. Must be 1,2,4,8,16,,,

// Menu height is 1 + 3 for large display, 7 for small as yellow is always 2 lines
#define MENU_HEIGHT_LARGE 4
#define MENU_HEIGHT_SMALL 7

#define MODE_SETUP 0
#define MODE_MENU 1
#define MODE_HID 2
#define MODE_USB 3

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

int menuHeight = 0;  // Default menu height
int menuLine = 0;
bool menuMax = true;
uint8_t screenRotation = ROTATE_LHS;
int displayMode = MODE_SETUP;
int buttonBits = 0;

bool updateScreen = true;  // Screen needs to be redrawn

struct keyMap {
  int rot;
  int mode;
  int buttonBit;
  int action;
};

keyMap keyMapping[] = {
  { ROTATE_LHS, MODE_HID, BIT_PIN_UP, ACTION_UP },
  { ROTATE_LHS, MODE_HID, BIT_PIN_DOWN, ACTION_DOWN },
  { ROTATE_LHS, MODE_HID, BIT_PIN_SEL, ACTION_SEND },
  { ROTATE_LHS, MODE_HID, BIT_PIN_ALT, ACTION_CONFIG },
  { ROTATE_LHS, MODE_MENU, BIT_PIN_UP, ACTION_LINES },
  { ROTATE_LHS, MODE_MENU, BIT_PIN_DOWN, ACTION_ROTATE },
  { ROTATE_LHS, MODE_MENU, BIT_PIN_SEL, ACTION_RESET },
  { ROTATE_LHS, MODE_MENU, BIT_PIN_ALT, ACTION_CONFIG },
  { ROTATE_RHS, MODE_HID, BIT_PIN_DOWN, ACTION_UP },
  { ROTATE_RHS, MODE_HID, BIT_PIN_UP, ACTION_DOWN },
  { ROTATE_RHS, MODE_HID, BIT_PIN_ALT, ACTION_SEND },
  { ROTATE_RHS, MODE_HID, BIT_PIN_SEL, ACTION_CONFIG },
  { ROTATE_RHS, MODE_MENU, BIT_PIN_DOWN, ACTION_LINES },
  { ROTATE_RHS, MODE_MENU, BIT_PIN_UP, ACTION_ROTATE },
  { ROTATE_RHS, MODE_MENU, BIT_PIN_ALT, ACTION_RESET },
  { ROTATE_RHS, MODE_MENU, BIT_PIN_SEL, ACTION_CONFIG }
};

int keyMappingLen = sizeof(keyMapping) / sizeof(keyMap);

int findAction() {
  for (int i = 0; i < keyMappingLen; i++) {
    if ((keyMapping[i].rot == screenRotation) && (keyMapping[i].mode == displayMode) && ((keyMapping[i].buttonBit & buttonBits) != 0)) {
      return keyMapping[i].action;
    }
  }
  return ACTION_NONE;
}

struct option {
  String disp;
  String keys;
};

String configOptions[] = {
  { " Reset" },
  { " Size" },
  { " Load" },
  { " Rotate" }
};

int configOptionsLen = sizeof(configOptions) / sizeof(String);

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

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

int tos = 0;
int oldTos = 0;

void box(int lin, int quad) {
  display.drawRect(0, lin * SCREEN_LINE, SCREEN_CHAR, SCREEN_LINE, WHITE);
  switch (quad) {
    case 0:
      display.fillRect(2, lin * SCREEN_LINE + 2, QUAD_W, QUAD_H, WHITE);
      break;
    case 1:
      display.fillRect(QUAD_W, lin * SCREEN_LINE + 2, QUAD_W, QUAD_H, WHITE);
      break;
    case 2:
      display.fillRect(2, lin * SCREEN_LINE + (QUAD_H + 2), QUAD_W, QUAD_H, WHITE);
      break;
    case 3:
      display.fillRect(QUAD_W, lin * SCREEN_LINE + (QUAD_H + 2), QUAD_W, QUAD_H, WHITE);
      break;
  }
}

void resetScreen(bool showButtons) {
  display.clearDisplay();
  display.setRotation(screenRotation);
  display.setCursor(0, 0);
  display.setTextSize(2);
  if (showButtons) {
    box(0, 0);
    box(1, 1);
    box(2, 2);
    box(3, 3);
  }
}

void updateScreenMenu() {
  updateScreen = false;
  resetScreen(true);
  for (int i = 0; i < configOptionsLen; i++) {
    display.println(configOptions[i]);
  }
  display.display();
}

void updateScreenHid() {
  updateScreen = false;
  resetScreen(false);
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

void selButtonPressed() {
  resetScreen(false);
  display.println("Sending...");
  display.println(options[tos].disp);
  display.display();
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
  resetScreen(false);
  display.println("Reset...");
  display.display();
  delay(500);
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
  resetScreen(false);
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

bool scanButtons() {
  int bb = 0;
  if (digitalRead(IN_PIN_UP) == LOW) {
    bb = bb | BIT_PIN_UP;
  }
  if (digitalRead(IN_PIN_DOWN) == LOW) {
    bb = bb | BIT_PIN_DOWN;
  }
  if (digitalRead(IN_PIN_SEL) == LOW) {
    bb = bb | BIT_PIN_SEL;
  }
  if (digitalRead(IN_PIN_ALT) == LOW) {
    bb = bb | BIT_PIN_ALT;
  }
  buttonBits = bb;
  return (bb != 0);
}

void printDiag(String s) {
  if (Serial) {
    Serial.println(s);
  }
}

void setup() {
  pinMode(LIVE_LED, OUTPUT);
  pinMode(IN_PIN_UP, INPUT_PULLUP);
  pinMode(IN_PIN_DOWN, INPUT_PULLUP);
  pinMode(IN_PIN_SEL, INPUT_PULLUP);
  pinMode(IN_PIN_ALT, INPUT_PULLUP);
  if (scanButtons()) {
    //      action = findAction();

    Serial.begin(9600);
    while (!Serial) {
      ;  // wait for serial port to connect. Needed for native USB port only
    }
    delay(500);
    printDiag("Config serial out active.");
  }
  digitalWrite(LIVE_LED, HIGH);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    errorCode(4);
  }
  // Clear the buffer
  resetScreen(false);
  display.setTextColor(SSD1306_WHITE);  // Draw white text
  display.println("Setup...");
  display.display();
  delay(400);
  setMode(MODE_HID);
  updateScreen = true;
}


void loop() {
  if (scanButtons()) {
    digitalWrite(LIVE_LED, LOW);
    switch (findAction()) {
      case ACTION_UP:
        upButtonPressed();
        break;
      case ACTION_DOWN:
        downButtonPressed();
        break;
      case ACTION_SEND:
        selButtonPressed();
        break;
      case ACTION_CONFIG:
        setMode(MODE_MENU);
        break;
      case ACTION_LINES:
        setLines();
        break;
      case ACTION_ROTATE:
        rotateDisplay();
        break;
      case ACTION_RESET:
        reset();
        break;
    }
    digitalWrite(LIVE_LED, HIGH);
  }

  if (updateScreen) {
    switch (displayMode) {
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