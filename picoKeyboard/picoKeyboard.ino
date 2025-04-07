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
#define LOG_LINES 6
#define QUAD_W 4
#define QUAD_H 6
#define CHAR_MAXIMUM 127  // Ignore chars above 126
#define CHAR_CR 10        // END OF LINE
#define CHAR_SPACE 32     // SPACE
#define CHAR_DELIM 124    // |

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
#define MENU_LINES 10
#define MENU_COLUMNS 10

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
String logSubjectStr = "";
String logLines[LOG_LINES] = { "", "", "", "", "", "" };
int logLinePos = 0;
char numberArray[20];



char menuLines[MENU_LINES][MENU_COLUMNS + 1];
int menuCount = 0;

volatile bool driveConnected = false;

void printDiag(String s) {
  if (Serial) {
    Serial.println(s);
  }
}

void logLine(String s, int d) {
  logLines[logLinePos] = s;
  display.clearDisplay();
  display.setRotation(screenRotation);
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println(logSubjectStr);
  display.setTextSize(1);
  int lp = logLinePos;
  for (int i = 0; i < LOG_LINES; i++) {
    lp++;
    if (lp >= LOG_LINES) {
      lp = 0;
    }
    display.println(logLines[lp]);
  }
  display.display();
  logLinePos++;
  if (logLinePos >= LOG_LINES) {
    logLinePos = 0;
  }
  delay(d);
}

void logSubject(String s) {
  logSubjectStr = s;
  logLinePos = 0;
  for (int i = 0; i < LOG_LINES; i++) {
    logLines[i] = "";
  }
  display.clearDisplay();
  display.setRotation(screenRotation);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(logSubjectStr);
  display.display();
}

int readKeyFile(File f) {
  for (int x = 0; x < MENU_LINES; x++) {
    for (int y = 0; y < (MENU_COLUMNS + 1); y++) {
      menuLines[x][y] = 0;
    }
  }
  int row = 0;
  int coll = 0;
  int char1 = 0;
  int capture = 0;
  while (f.available()) {
    char1 = f.read();
    if (char1 == CHAR_CR) {
      row++;
      if (row >= MENU_LINES) {
        return MENU_LINES;
      }
      coll = 0;
      capture = 0;
    } else {
      if (char1 == CHAR_DELIM) {
        capture++;
      } 
      if ((char1 >= CHAR_SPACE) && (char1 < CHAR_MAXIMUM) && (coll < MENU_COLUMNS) && ((capture == 0) || (capture == 2))) {
        menuLines[row][coll] = char1;
        coll++;
        capture = 0;
      }
    }
  }
  return row;
}

void loadKeyData() {
  menuCount = 0;
  logSubject("Loading:");
  logLine("FS Init", 10);
  if (FatFS.begin()) {
    logLine("FS OK", 10);
  } else {
    logLine("FS FAIL", 5000);
    return;
  }
  Dir dir = FatFS.openDir("/");
  while (true) {
    if (!dir.next()) {
      break;
    }
    if (dir.isFile()) {
      logLine(dir.fileName(), 10);
      File f = dir.openFile("r");
      if (f) {
        logLine("FS openFile OK", 10);
        menuCount = readKeyFile(f);
        logLine("FS openFile done", 10);
        logLine(itoa(menuCount, numberArray, 10), 11);
      } else {
        logLine("FS openFile FAIL", 5000);
        return;
      }
    }
  }
}

void unplug(uint32_t i) {
  (void)i;
  driveConnected = false;
}

// Called by FatFSUSB when the drive is mounted by the PC.  Have to stop FatFS, since the drive data can change, note it, and continue.
void plug(uint32_t i) {
  (void)i;
  driveConnected = true;
}

// Called by FatFSUSB to determine if it is safe to let the PC mount the USB drive.  If we're accessing the FS in any way, have any Files open, etc. then it's not safe to let the PC mount the drive.
bool mountable(uint32_t i) {
  (void)i;
  return true;
}

void waitForButtonC() {
  while (digitalRead(IN_PIN_C) == HIGH) {
    delay(10);
  }
  while (digitalRead(IN_PIN_C) == LOW) {
    delay(10);
  }
}

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

void clearScreen(int showButtons) {
  display.clearDisplay();
  display.setRotation(screenRotation);
  display.setTextColor(SSD1306_WHITE);  // Draw white text
  display.setTextWrap(false);
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
  clearScreen(BIT_2 | BIT_4 | BIT_8);
  display.println("Program:");
  display.println(" Ident");
  display.println(" Upload");
  display.println(" Reset");
  display.display();
}

void updateScreenMenu() {
  updateScreen = false;
  clearScreen(BIT_ALL);
  display.println(" Reset");
  display.println(" Size");
  display.println(" Rotate");
  display.println(" Back");
  display.display();
}

void updateScreenHid() {
  updateScreen = false;
  clearScreen(BIT_NONE);
  menuLine = tos;
  display.println(menuLines[menuLine]);
  if (menuMax) {
    display.setTextSize(1);
    menuHeight = MENU_HEIGHT_SMALL;
  } else {
    menuHeight = MENU_HEIGHT_LARGE;
  }
  if (menuHeight > menuCount) {
    menuHeight = menuCount;
  }
  for (int i = 1; i < menuHeight; i++) {
    menuLine++;
    if (menuLine >= menuCount) {
      menuLine = 0;
    }
    display.println(menuLines[menuLine]);
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
  logSubject("Sending...");
  logLine(menuLines[tos], 100);
  sendKeys(menuLines[tos]);
  logLine("Done", 1000);
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
  if (tos >= menuCount) {
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
    tos = menuCount - 1;
  }
  if (oldTos != tos) {
    updateScreen = true;
  }
}

void reset() {
  logSubject("Resetting:");
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
  menuMax = !menuMax;
  logSubject("ZOOM:");
  if (menuMax) {
    logLine("...8 Lines", 1000);
  } else {
    logLine("...4 Lines", 1000);
  }
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
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    errorCode(4);
  }
  // Clear the buffer
  clearScreen(BIT_NONE);
  logSubject("Setup:");
  digitalWrite(LIVE_LED, HIGH);
  if (programButtons()) {
    digitalWrite(LIVE_LED, LOW);
    logLine("Serial?", 10);
    int c = 100;
    Serial.begin(9600);
    while (!Serial) {
      delay(10);
      c--;
      if (c <= 0) {
        logLine("Timeout", 10);
        break;
      }
    }
    if (c > 0) {
      logLine("Serial OK", 10);
    }
    delay(1000);
    logLine("FS Init", 10);
    if (!FatFS.begin()) {
      logLine("FS Fail", 10);
    } else {
      logLine("FS OK", 10);
    }
    FatFSUSB.onUnplug(unplug);
    FatFSUSB.onPlug(plug);
    FatFSUSB.driveReady(mountable);
    // Start FatFS USB drive mode
    FatFSUSB.begin();
    while (programButtons()) {
      delay(500);
    }
    logLine("Program", 1000);
    setMode(MODE_PGM);
  } else {
    loadKeyData();
    setMode(MODE_HID);
  }
  digitalWrite(LIVE_LED, HIGH);
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