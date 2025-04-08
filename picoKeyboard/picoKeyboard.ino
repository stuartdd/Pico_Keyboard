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

#define MENU_FILE "menu.txt"
#define EEPROM_FILE "eeprom.dat"
#define EEPROM_LEN 10
#define EEPROM_ROTATE 0
#define EEPROM_MENU_MAX 1
#define EEPROM_TRUE '1'
#define EEPROM_FALSE '0'

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



Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

int menuHeight = 0;  // Default menu height
int menuLine = 0;
int displayMode = MODE_SETUP;
int buttonBits = 0;
int tos = 0;
int oldTos = 0;
volatile bool updateScreen = true;  // Screen needs to be redrawn
String logSubjectStr = "";
String logLines[LOG_LINES] = { "", "", "", "", "", "" };
int logLinePos = 0;
char numberArray[20];
uint8_t configFlags[EEPROM_LEN];
bool eepromLoaded = false;


uint8_t getConfigUint(int pos) {
  return configFlags[pos] - 48;
}

bool getConfigBool(int pos) {
  return configFlags[pos] != EEPROM_FALSE;
}

void setConfigUint(int pos, uint8_t v) {
  uint8_t vv = getConfigUint(pos);
  if (v != vv) {
    configFlags[pos] = v + 48;
    writeEEpromData();
  }
}

void setConfigBool(int pos, bool v) {
  bool vv = getConfigBool(pos);
  if (v != vv) {
    if (v) {
      configFlags[pos] = EEPROM_TRUE;
    } else {
      configFlags[pos] = EEPROM_FALSE;
    }
    writeEEpromData();
  }
}

struct menuData {
  char prompt[MENU_COLUMNS + 1];
  int offset;
  int len;
} menuLines[MENU_LINES];
int menuCount = 0;

void printDiag(String s) {
  if (Serial) {
    Serial.println(s);
  }
}

void logLine(String s, int d) {
  logLines[logLinePos] = s;
  display.clearDisplay();
  display.setRotation(getConfigUint(EEPROM_ROTATE));
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

void logError(String s, int d) {
  logLine(s, d + 1);
  if (d < 2) {
    waitForButton();
    if (d == 1) {
      logLine("Reboot:", 1000);
      rp2040.reboot();
    }
  }
}

void logSubject(String s) {
  logSubjectStr = s;
  logLinePos = 0;
  for (int i = 0; i < LOG_LINES; i++) {
    logLines[i] = "";
  }
  display.clearDisplay();
  display.setRotation(getConfigUint(EEPROM_ROTATE));
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(logSubjectStr);
  display.display();
}


void writeEEpromData() {
  File f = FatFS.open(EEPROM_FILE, "w");
  if (f) {
    f.write(configFlags, EEPROM_LEN);
    f.close();
    logLine("EEPROM Created", 10);
  } else {
    logError("FAIL-OPEN W " + String(EEPROM_FILE), 1);
  }
}

void readEEpromData() {
  File f = FatFS.open(EEPROM_FILE, "r");
  if (f) {
    int pos = 0;
    while (f.available()) {
      configFlags[pos] = char(f.read());
      pos++;
      if (pos >= EEPROM_LEN) {
        break;
      }
    }
    f.close();
    eepromLoaded = true;
    logLine("EEPROM Loaded", 10);
  } else {
    logError("FAIL-OPEN R " + String(EEPROM_FILE), 1);
  }
}

int readKeyFile(File f) {
  for (int x = 0; x < MENU_LINES; x++) {
    for (int y = 0; y < (MENU_COLUMNS + 1); y++) {
      menuLines[x].prompt[y] = 0;
      menuLines[x].offset = 0;
      menuLines[x].len = 0;
    }
  }
  int row = 0;
  int coll = 0;
  int char1 = 0;
  int capture = 0;
  int count = 0;
  int offset = 0;
  while (f.available()) {
    char1 = f.read();
    offset++;
    if (char1 == CHAR_CR) {
      if (menuLines[row].len > 0) {
        if (menuLines[row].offset == 0) {
          logLine("Menu: " + String(menuLines[row].prompt), 10);
          logError("Line: " + String(itoa(row, numberArray, 10)) + " has No keys", 1);
        }
        count++;
      }
      row++;
      if (row >= MENU_LINES) {
        return count;
      }
      coll = 0;
      capture = 0;
    } else {
      if (char1 == CHAR_DELIM) {
        capture++;
        menuLines[row].offset = offset;
      }
      if ((capture == 0) || (capture == 2)) {
        if ((char1 >= CHAR_SPACE) && (char1 < CHAR_MAXIMUM) && (coll < MENU_COLUMNS)) {
          menuLines[row].prompt[coll] = char1;
          menuLines[row].len++;
          coll++;
          capture = 0;
        }
      }
    }
  }
  return count;
}

void sendKeyData(int tos) {
  int count = 0;
  logSubject("Sending:");
  int offset = menuLines[tos].offset;
  File f = FatFS.open(MENU_FILE, "r");
  if (f) {
    logLine("Sending item: " + String(menuLines[tos].prompt), 10);
    if (f.seek(offset)) {
      while (f.available()) {
        int char1 = f.read();
        if (char1 != CHAR_CR) {
          Keyboard.press(char1);
          delay(10);
          Keyboard.releaseAll();
          count++;
        } else {
          break;
        }
      }
    } else {
      logError("Seek fail:" + String(itoa(offset, numberArray, 10)), 1);
    }
    f.close();
    logLine("Sent: " + String(itoa(count, numberArray, 10)) + " Keys.", 700);
  } else {
    logError("FS openFile FAIL", 1);
  }
  stopIfButton();
}

void loadConfigData() {
  menuCount = -1;
  logSubject("Loading:");
  logLine("FS Init", 10);
  if (FatFS.begin()) {
    logLine("FS OK", 10);
  } else {
    logError("FS FAIL", 1);
  }

  for (int i = 0; i < EEPROM_LEN; i++) {
    configFlags[i] = '0';
  }

  Dir dir = FatFS.openDir("/");
  while (true) {
    if (!dir.next()) {
      break;
    }
    if (dir.isFile()) {
      if (dir.fileName() == EEPROM_FILE) {
        logLine("READ " + dir.fileName(), 10);
        readEEpromData();
      }
      if (dir.fileName() == MENU_FILE) {
        logLine("READ " + dir.fileName(), 10);
        File f = dir.openFile("r");
        if (f) {
          menuCount = readKeyFile(f);
          if (menuCount == 0) {
            logError("No items found", 1);
          };
          logLine("ITEMS " + String(itoa(menuCount, numberArray, 10)), 10);
        } else {
          logError("FS openFile FAIL", 1);
        }
      }
    }
  }
  if (!eepromLoaded) {
    logLine("NO File " + String(EEPROM_FILE), 1);
  }
  if (menuCount == -1) {
    logError("NO File:" + String(MENU_FILE), 1);
  }
  stopIfButton();
}

void unplug(uint32_t i) {
  (void)i;
}

// Called by FatFSUSB when the drive is mounted by the PC.  Have to stop FatFS, since the drive data can change, note it, and continue.
void plug(uint32_t i) {
  (void)i;
}

// Called by FatFSUSB to determine if it is safe to let the PC mount the USB drive.  If we're accessing the FS in any way, have any Files open, etc. then it's not safe to let the PC mount the drive.
bool mountable(uint32_t i) {
  (void)i;
  return true;
}

void waitForButton() {
  logLine("Press B or D..", 1);
  while ((digitalRead(IN_PIN_B) == HIGH) && (digitalRead(IN_PIN_D) == HIGH)) {
    delay(10);
  }
  stopIfButton();
}

void stopIfButton() {
  while ((digitalRead(IN_PIN_B) == LOW) || (digitalRead(IN_PIN_D) == LOW)) {
    delay(10);
  }
}

bool programButtons() {
  return (digitalRead(IN_PIN_A) == LOW) || (digitalRead(IN_PIN_C) == LOW);
}

bool scanButtons() {
  int bb = 0;
  if (digitalRead(IN_PIN_A) == LOW) {
    if (getConfigUint(EEPROM_ROTATE) == ROTATE_LHS) {
      bb = bb | BIT_1;
    } else {
      bb = bb | BIT_4;
    }
  }
  if (digitalRead(IN_PIN_B) == LOW) {
    if (getConfigUint(EEPROM_ROTATE) == ROTATE_LHS) {
      bb = bb | BIT_2;
    } else {
      bb = bb | BIT_8;
    }
  }
  if (digitalRead(IN_PIN_C) == LOW) {
    if (getConfigUint(EEPROM_ROTATE) == ROTATE_LHS) {
      bb = bb | BIT_4;
    } else {
      bb = bb | BIT_1;
    }
  }
  if (digitalRead(IN_PIN_D) == LOW) {
    if (getConfigUint(EEPROM_ROTATE) == ROTATE_LHS) {
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
  if (getConfigUint(EEPROM_ROTATE) == ROTATE_RHS) {  // Rotate the Quadrant
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
  display.setRotation(getConfigUint(EEPROM_ROTATE));
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
  clearScreen(BIT_2);
  display.println("Program:");
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
  display.println(menuLines[menuLine].prompt);
  if (getConfigBool(EEPROM_MENU_MAX)) {
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
    display.println(menuLines[menuLine].prompt);
  }
  display.display();
}

void sendButtonPressed() {
  sendKeyData(tos);
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
  if (getConfigUint(EEPROM_ROTATE) == ROTATE_LHS) {
    setConfigUint(EEPROM_ROTATE, ROTATE_RHS);
  } else {
    setConfigUint(EEPROM_ROTATE, ROTATE_LHS);
  }
  setMode(MODE_HID);
}

void setLines() {
  setConfigBool(EEPROM_MENU_MAX, !getConfigBool(EEPROM_MENU_MAX));
  logSubject("ZOOM:");
  if (getConfigBool(EEPROM_MENU_MAX)) {
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
        logError("Serial Timeout", 0);
        break;
      }
    }
    if (c > 0) {
      logLine("Serial OK", 10);
    }
    delay(1000);
    logLine("FS Init", 10);
    if (!FatFS.begin()) {
      logError("FS Fail", 0);
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
    loadConfigData();
    setMode(MODE_HID);
  }
  digitalWrite(LIVE_LED, HIGH);
  updateScreen = true;
  buttonBits = 0;
  // waitForButton();
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
        if ((buttonBits & BIT_8) == BIT_8) {
          reset();
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