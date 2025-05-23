/**************************************************************************
 * 
 * SSD1306 Display
 * Send Keys
 * RP2040 (pico)
 *
 * Apache License Version 2.0, January 2004
 * Stuart Davies
 * https://github.com/stuartdd/Pico_Keyboard
 * 
 *
 * Add to Preferences: Additional Board Manager URL:
 * https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
 * https://github.com/earlephilhower/arduino-pico/releases/download/4.0.2/package_rp2040_index.json
 *
 *
 * Add Library Adafruit_SSD1306 and the additional libraries it says it requires.
 *
 * Select Board: Raspberry Pi Pico.
 * Set: Tools -> Flash Size -> to at least 1 meg to enable USB storage
 *
 */
#include <Keyboard.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/Picopixel.h>
#include <FatFS.h>
#include <FatFSUSB.h>

#include "config.h"
#include "KeyboardLayout_en_UK.h"


#define MENU_FILE "menu.txt"  // Namer of the menu file

#define SCREEN_ADDRESS 0x3C  ///< See datasheet for Address; 0x3C
#define SCREEN_WIDTH 128     // OLED display width, in pixels
#define SCREEN_HEIGHT 64     // OLED display height, in pixels
#define SCREEN_LINE 16       // Physical number of lines for button icon height
#define SCREEN_CHAR 10       // Physical number of lines for button icon width
#define QUAD_H 6             // Physical number of lines for button dot height
#define QUAD_W 3             // Physical number of lines for button dot width
#define LOG_LINES 4          // Number of non subject (heading) lines in the log display
#define LINE_1_SMALL 24      // Using small font. This is Y offset to 1st Line
#define LINE_1_LARGE 20      // Using large font. This is Y offset to 1st Line
#define LINE_Y_SMALL 12      // Using small font. This is Y offset to 1st Line
#define LINE_Y_LARGE 22      // Using large font. This is Y offset to 1st Line


#define CHAR_MAXIMUM 127  // Ignore chars above 126
#define CHAR_CR 10        // END OF LINE
#define CHAR_SPACE 32     // SPACE
#define CHAR_DELIM 124    // |

#define LIVE_LED 22  // Actual Arduino pin number

#define BIT_ALL 255  // Show ALL button icons
#define BIT_NONE 0   // Show NO button icons

#define IN_PIN_A 7   // Actual Arduino pin number
#define BIT_PIN_A 1  // Bit position in button icon. Must be 1,2,4,8,16,,,
#define QUAD_0 0     // Button Icon Top Left

#define IN_PIN_B 8   // Actual Arduino pin number
#define BIT_PIN_B 2  // Bit position in button icon. Must be 1,2,4,8,16,,,
#define QUAD_1 1     // Button Icon Top right

#define IN_PIN_C 14  // Actual Arduino pin number
#define BIT_PIN_C 4  // Bit position in button icon. Must be 1,2,4,8,16,,,
#define QUAD_2 2     // Button Icon Bottom left

#define IN_PIN_D 6   // Actual Arduino pin number
#define BIT_PIN_D 8  // Bit position in button icon. Must be 1,2,4,8,16,,,
#define QUAD_3 3     // Button Icon Bottom right

// Menu height is 1 + 3 for large display, 7 for small as yellow is always 2 lines
#define MENU_HEIGHT_LARGE 3
#define MENU_HEIGHT_SMALL 5
#define MENU_LINES_MAX 15
#define MENU_COLUMNS 10

#define MODE_SETUP 0
#define MODE_MENU 1
#define MODE_HID 2
#define MODE_PGM 3
#define MODE_DIAG 4

// Screen rotation for LHS and RHS plug in.
#define ROTATE_LHS 0
#define ROTATE_RHS 2


int lineOneOffset = LINE_1_SMALL;
int lineYOffset = LINE_Y_SMALL;

int menuHeight = 0;                 // Default menu height
int menuLine = 0;                   // Used while drawing the menu
int menuCount = 0;                  // Number of prompts found in the menu file.
int displayMode = MODE_SETUP;       // Current operating mode
int buttonBits = 0;                 // Bits to represent buttons LHS or RHS
int tos = 0;                        // The menu item at the top of the screen                   // The previous item at the top of screen
volatile bool updateScreen = true;  // Screen needs to be redrawn

String logSubjectStr = "";   // Log heading
String logLines[LOG_LINES];  // Lines under heading (Circular buffer)
int logLinePos = 0;          // Next position to wrire a log line
int diagCounter = 0;
int pushSendDelay = 0;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

void pushSend(int delay) {
  pushSendDelay = delay;
}

void initDisplayLarge() {
  setFontSizeSmall(false);
  display.setRotation(getConfigUint(CONFIG_ROTATE));
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextColor(WHITE);
}

void setFontSizeSmall(bool small) {
  bool rotate = getConfigBool(CONFIG_ROTATE);
  display.setTextSize(2);
  if (small) {
    lineOneOffset = LINE_1_SMALL;
    lineYOffset = LINE_Y_SMALL;
    menuHeight = MENU_HEIGHT_SMALL;
    display.setFont(&Picopixel);
  } else {
    lineOneOffset = LINE_1_LARGE;
    lineYOffset = LINE_Y_LARGE;
    menuHeight = MENU_HEIGHT_LARGE;
    display.setFont();
  }
}


// Data read from the menu file...
struct menuData {
  char prompt[MENU_COLUMNS + 1];  // The prompt (displayed) first N chars of the line. The +1 for 0 string terminator
  int offset;                     // The offset in the file to the key data to be sent for the prompt.
  int len;                        // The length of the prompt
} menuLines[MENU_LINES_MAX];      // The menu data

// Log a line and scroll the display while retaining the subject (header)
// Delay after line is logged. So human can see it!
void logLine(String s, int d) {
  logLines[logLinePos] = s;
  initDisplayLarge();
  display.print(logSubjectStr);
  setFontSizeSmall(true);
  int lineY = lineOneOffset;
  int lp = logLinePos;
  display.setCursor(0, lineY);
  for (int i = 0; i < LOG_LINES; i++) {
    lp++;
    if (lp >= LOG_LINES) {
      lp = 0;
    }
    display.print(logLines[lp]);
    lineY = lineY + lineYOffset;
    display.setCursor(0, lineY);
  }
  display.display();
  logLinePos++;
  if (logLinePos >= LOG_LINES) {
    logLinePos = 0;
  }
  delay(d);
}

// Log a line and wait for a key press.
void logError(String s, int d) {
  logLine(s, d + 1);
  if (d < 2) {
    waitForButton("");
    if (d == 1) {
      logLine("Reboot:", 1000);
      rp2040.reboot();
    }
  }
}

// Log the subject (header) large. Clear the screen
void logSubject(String s) {
  logSubjectStr = s;
  logLinePos = 0;
  for (int i = 0; i < LOG_LINES; i++) {
    logLines[i] = "";
  }
  initDisplayLarge();
  display.println(logSubjectStr);
  display.display();
}

int readKeyFile(File f) {
  for (int x = 0; x < MENU_LINES_MAX; x++) {
    for (int y = 0; y < (MENU_COLUMNS + 1); y++) {
      menuLines[x].prompt[y] = 0;
      menuLines[x].offset = 0;
      menuLines[x].len = 0;
    }
  }
  int row = 0;
  int coll = 0;
  int char1 = 0;
  bool delimFound = false;
  int count = 0;
  int offset = 0;

  while (f.available()) {
    char1 = f.read();
    offset++;  // Remember the offset for this char
    if (char1 == CHAR_CR) {
      if (menuLines[row].len > 0) {
        if (menuLines[row].offset == 0) {
          logLine("Menu: " + String(menuLines[row].prompt), 10);
          logError(String("Line: ") + row + " has No keys", 1);
        }
        count++;
      }
      row++;
      if (row >= MENU_LINES_MAX) {
        return count;
      }
      coll = 0;
      delimFound = false;
    } else {
      if ((char1 == CHAR_DELIM) && (!delimFound)) {
        delimFound = true;
        menuLines[row].offset = offset;
      } else {
        if ((char1 >= CHAR_SPACE) && (char1 < CHAR_MAXIMUM) && (coll < MENU_COLUMNS) && !delimFound) {
          menuLines[row].prompt[coll] = char1;
          menuLines[row].len++;
          coll++;
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
    logLine("--> " + String(menuLines[tos].prompt), 10);
    if (f.seek(offset)) {
      while (f.available()) {
        int char1 = f.read();
        if (char1 != CHAR_CR) {
          count = count + sendKeyInt(char1);
        } else {
          break;
        }
      }
    } else {
      logError(String("Seek fail:") + offset, 1);
    }
    f.close();
    logLine(String("Sent: ") + count + " Keys.", 700);
  } else {
    logError("FS openFile FAIL", 1);
  }
  // waitForButton();
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

  for (int i = 0; i < CONFIG_LEN; i++) {
    configFlags[i] = '0';
  }

  Dir dir = FatFS.openDir("/");
  while (true) {
    if (!dir.next()) {
      break;
    }
    if (dir.isFile()) {
      if (dir.fileName() == CONFIG_FILE) {
        if (!readConfigData()) {
          logError("FAIL-OPEN R " + String(CONFIG_FILE), 1);
        }
        logLine("READ Config OK", 10);
      }
      if (dir.fileName() == MENU_FILE) {
        logLine("READ " + dir.fileName(), 10);
        File f = dir.openFile("r");
        if (f) {
          menuCount = readKeyFile(f);
          if (menuCount == 0) {
            logError("No items found", 1);
          };
          logLine(String("ITEMS ") + menuCount, 10);
        } else {
          logError("FS openFile FAIL", 1);
        }
      }
    }
  }
  if (!configLoaded) {
    logLine("NO File " + String(CONFIG_FILE), 1);
  }
  if (menuCount == -1) {
    logError("NO File:" + String(MENU_FILE), 1);
  }
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

void waitForButton(String m) {
  if (m == "") {
    logLine("Press B or D..", 1);
  } else {
    logLine(m, 1);
  }
  while ((digitalRead(IN_PIN_B) == HIGH) && (digitalRead(IN_PIN_D) == HIGH)) {
    delay(10);
  }
  stopIfButton();
}

void stopIfButton() {
  while ((digitalRead(IN_PIN_A) == LOW) || (digitalRead(IN_PIN_B) == LOW) || (digitalRead(IN_PIN_C) == LOW) || (digitalRead(IN_PIN_D) == LOW)) {
    delay(10);
  }
}

bool programButtons() {
  return (digitalRead(IN_PIN_A) == LOW) || (digitalRead(IN_PIN_C) == LOW);
}

bool altButtons() {
  return (digitalRead(IN_PIN_B) == LOW) || (digitalRead(IN_PIN_D) == LOW);
}

bool scanButtons() {
  if (pushSendDelay > 1) {
    pushSendDelay--;
    return false;
  }
  if (pushSendDelay == 1) {
    pushSendDelay = 0;
    buttonBits = BIT_PIN_A;
    return true;
  }
  int bb = 0;
  if (digitalRead(IN_PIN_A) == LOW) {
    if (getConfigUint(CONFIG_ROTATE) == ROTATE_LHS) {
      bb = bb | BIT_PIN_A;
    } else {
      bb = bb | BIT_PIN_C;
    }
  }
  if (digitalRead(IN_PIN_B) == LOW) {
    if (getConfigUint(CONFIG_ROTATE) == ROTATE_LHS) {
      bb = bb | BIT_PIN_B;
    } else {
      bb = bb | BIT_PIN_D;
    }
  }
  if (digitalRead(IN_PIN_C) == LOW) {
    if (getConfigUint(CONFIG_ROTATE) == ROTATE_LHS) {
      bb = bb | BIT_PIN_C;
    } else {
      bb = bb | BIT_PIN_A;
    }
  }
  if (digitalRead(IN_PIN_D) == LOW) {
    if (getConfigUint(CONFIG_ROTATE) == ROTATE_LHS) {
      bb = bb | BIT_PIN_D;
    } else {
      bb = bb | BIT_PIN_B;
    }
  }
  buttonBits = bb;
  return (bb != 0);
}

void box(int lin, int quad) {
  bool rotate = getConfigUint(CONFIG_ROTATE) == ROTATE_RHS;
  int adjust = 0;
  if (rotate) {
    adjust = 1;
  }
  display.drawRect(0, lin * SCREEN_LINE + adjust, SCREEN_CHAR, SCREEN_LINE - 1, WHITE);
  if (rotate) {  // Rotate the Quadrant
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
      display.fillRect(QUAD_W + 2, lin * SCREEN_LINE + 2, QUAD_W, QUAD_H, WHITE);
      break;
    case QUAD_2:
      display.fillRect(2, lin * SCREEN_LINE + (QUAD_H + 2), QUAD_W, QUAD_H, WHITE);
      break;
    case QUAD_3:
      display.fillRect(QUAD_W + 2, lin * SCREEN_LINE + (QUAD_H + 2), QUAD_W, QUAD_H, WHITE);
      break;
  }
}

void initSerial() {
  int c = 500;
  Serial.begin(9600);
  while (!Serial) {
    delay(10);
    c--;
    if (c <= 0) {
      logError("Serial Timeout", 1);
      break;
    }
  }
  if (c > 0) {
    logLine("Serial 9600", 10);
  }
  delay(1000);
}

void initScreenButtons(int showButtons) {
  initDisplayLarge();
  if ((showButtons & BIT_PIN_A) == BIT_PIN_A) {
    box(0, QUAD_0);
  }
  if ((showButtons & BIT_PIN_B) == BIT_PIN_B) {
    box(1, QUAD_1);
  }
  if ((showButtons & BIT_PIN_C) == BIT_PIN_C) {
    box(2, QUAD_2);
  }
  if ((showButtons & BIT_PIN_D) == BIT_PIN_D) {
    box(3, QUAD_3);
  }
}

void updateScreenPgm() {
  updateScreen = false;
  initScreenButtons(BIT_PIN_B);
  display.println("Program:");
  display.println(" Reset");
  display.display();
}

void updateScreenMenu() {
  updateScreen = false;
  initScreenButtons(BIT_ALL);
  display.println(" Reset");
  display.println(" Size");
  display.println(" Rotate");
  display.println(" Down/Up");
  display.display();
}

void updateScreenHid() {
  updateScreen = false;
  initDisplayLarge();
  menuLine = tos;
  display.print(menuLines[menuLine].prompt);
  if (getConfigBool(CONFIG_MENU_MAX)) {
    setFontSizeSmall(true);
  }
  if (menuHeight > menuCount) {
    menuHeight = menuCount;
  }
  int lineY = lineOneOffset;
  display.setCursor(0, lineY);
  for (int i = 1; i < menuHeight; i++) {
    menuLine++;
    if (menuLine >= menuCount) {
      menuLine = 0;
    }
    display.print(menuLines[menuLine].prompt);
    lineY = lineY + lineYOffset;
    display.setCursor(0, lineY);
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

void setSelected(int sel, bool wrap) {
  if (sel < 0) {
    if (wrap) {
      sel = (menuCount - 1);
    } else {
      sel = 0;
    }
  }
  if (sel >= menuCount) {
    if (wrap) {
      sel = 0;
    } else {
      sel = (menuCount - 1);
    }
  }
  if (sel != tos) {
    tos = sel;
    updateScreen = true;
  }
}

void upButtonPressed(bool swap) {
  if (swap) {
    downButtonPressed(false);
  } else {
    setSelected(tos + 1, true);
  }
}

void downButtonPressed(bool swap) {
  if (swap) {
    upButtonPressed(false);
  } else {
    setSelected(tos - 1, true);
  }
}

void reset() {
  logSubject("Resetting:");
  delay(1000);
  rp2040.reboot();
}

void swapDownUp() {
  flipConfigBool(CONFIG_DOWN_UP);
  logSubject("SWAP:\nDown & Up");
  delay(1000);
  setMode(MODE_HID);
}

void rotateDisplay() {
  if (getConfigUint(CONFIG_ROTATE) == ROTATE_LHS) {
    setConfigUint(CONFIG_ROTATE, ROTATE_RHS);
  } else {
    setConfigUint(CONFIG_ROTATE, ROTATE_LHS);
  }
  setMode(MODE_HID);
}

void setLines() {
  flipConfigBool(CONFIG_MENU_MAX);
  logSubject("ZOOM:");
  if (getConfigBool(CONFIG_MENU_MAX)) {
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

int pcx = 0;
int pcxi = 1;
int pcxmax = 100;
int pcxmin = 0;
int pcy = 0;
int pcyi = 1;
int pcymax = 62;
int pcymin = 7;
bool passCodeKey(String s, int bits, bool wait, unsigned long lo, unsigned long hi) {
  buttonBits = 0;
  pcx = pcxmin;
  pcy = pcymin;
  while (!scanButtons()) {
    display.clearDisplay();
    display.setCursor(pcx, pcy);
    display.setTextSize(2);
    display.print(s);
    display.display();
    pcx = pcx + pcxi;
    if (pcx > pcxmax) {
      pcxi = -1;
    } else {
      if (pcx < pcxmin) {
        pcxi = 1;
      }
    }

    pcy = pcy + pcyi;
    if (pcy > pcymax) {
      pcyi = -1;
    } else {
      if (pcy < pcymin) {
        pcyi = 1;
      }
    }
    delay(10);
  }
  unsigned long m1 = millis();
  int bb = buttonBits;
  if (wait) {
    while (scanButtons()) {
      delay(10);
    }
  }
  unsigned long m2 = millis() - m1;
  display.setCursor(0, 10);
  display.print(String("") + m2);
  display.display();
  delay(100);
  if ((bb == bits) && (m2 > lo) && (m2 < hi)) {
    return true;
  }
  return false;
}

String pasPrompt(String in, bool ok) {
  if (getConfigBool(CONFIG_DIAG)) {
    if (ok) {
      return in + "+";
    }
    return in + "-";
  }
  return in;
}

void passCode() {
  if (!getConfigBool(CONFIG_PC)) {
    return;
  }
  bool a = false;
  bool b = false;
  bool c = false;
  bool d = false;
  do {
    a = passCodeKey(pasPrompt("?", true), BIT_PIN_A, true, 100, 400);
    b = passCodeKey(pasPrompt("1", a), BIT_PIN_B, true, 1000, 3000);
    c = passCodeKey(pasPrompt("2", b), BIT_PIN_C, true, 1000, 3000);
    d = passCodeKey(pasPrompt("3", c), BIT_PIN_D, true, 100, 400);
    passCodeKey(pasPrompt("4", d), 0, false, 100, 500);
    if (a && b && c && d) {
      return;
    }
    display.clearDisplay();
    display.display();
    digitalWrite(LIVE_LED, HIGH);
    delay(6000);
    rp2040.reboot();
  } while (true);
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
  initDisplayLarge();

  logSubject("Setup:");
  digitalWrite(LIVE_LED, HIGH);
  if (programButtons()) {
    stopIfButton();
    loadConfigData();
    passCode();
    digitalWrite(LIVE_LED, LOW);
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
    Keyboard.begin(KeyboardLayout_en_UK);
    loadConfigData();
    passCode();
    setMode(MODE_HID);
  }
  stopIfButton();
  display.setRotation(getConfigUint(CONFIG_ROTATE));
  digitalWrite(LIVE_LED, HIGH);
  updateScreen = true;
  buttonBits = 0;
  // waitForButton();
}

void loop() {
  if (displayMode == MODE_DIAG) {
    if (diagCounter < 128) {
      Serial.print(diagCounter);
      Serial.print(": ");
      Serial.println(String(itoa(int(KeyboardLayout_en_UK[diagCounter]), numberString, 16)));
      diagCounter++;
    }
  } else {
    if (scanButtons()) {
      digitalWrite(LIVE_LED, LOW);
      switch (displayMode) {
        case MODE_HID:
          if ((buttonBits & BIT_PIN_A) == BIT_PIN_A) {
            sendButtonPressed();
          } else {
            if ((buttonBits & BIT_PIN_B) == BIT_PIN_B) {
              upButtonPressed(getConfigBool(CONFIG_DOWN_UP));
            } else {
              if ((buttonBits & BIT_PIN_C) == BIT_PIN_C) {
                setMode(MODE_MENU);
              } else {
                if ((buttonBits & BIT_PIN_D) == BIT_PIN_D) {
                  downButtonPressed(getConfigBool(CONFIG_DOWN_UP));
                }
              }
            }
          }
          break;
        case MODE_MENU:
          if ((buttonBits & BIT_PIN_A) == BIT_PIN_A) {
            reset();
          } else {
            if ((buttonBits & BIT_PIN_B) == BIT_PIN_B) {
              setLines();
            } else {
              if ((buttonBits & BIT_PIN_C) == BIT_PIN_C) {
                rotateDisplay();
              } else {
                if ((buttonBits & BIT_PIN_D) == BIT_PIN_D) {
                  swapDownUp();
                }
              }
            }
          }
          break;
        case MODE_PGM:
          if ((buttonBits & BIT_PIN_B) == BIT_PIN_B) {
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
  }

  while (scanButtons()) {
    delay(100);
  }

  delay(100);
}