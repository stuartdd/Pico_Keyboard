#include "Keyboard.h"

/*
https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
https://github.com/earlephilhower/arduino-pico/releases/download/4.0.2/package_rp2040_index.json
*/

/*
Dec  Char                           Dec  Char     Dec  Char     Dec  Char
---------                           ---------     ---------     ----------
  0  NUL (null)                      32  SPACE     64  @         96  `
  1  SOH (start of heading)          33  !         65  A         97  a
  2  STX (start of text)             34  "         66  B         98  b
  3  ETX (end of text)               35  #         67  C         99  c
  4  EOT (end of transmission)       36  $         68  D        100  d
  5  ENQ (enquiry)                   37  %         69  E        101  e
  6  ACK (acknowledge)               38  &         70  F        102  f
  7  BEL (bell)                      39  '         71  G        103  g
  8  BS  (backspace)                 40  (         72  H        104  h
  9  TAB (horizontal tab)            41  )         73  I        105  i
 10  LF  (NL line feed, new line)    42  *         74  J        106  j
 11  VT  (vertical tab)              43  +         75  K        107  k
 12  FF  (NP form feed, new page)    44  ,         76  L        108  l
 13  CR  (carriage return)           45  -         77  M        109  m
 14  SO  (shift out)                 46  .         78  N        110  n
 15  SI  (shift in)                  47  /         79  O        111  o
 16  DLE (data link escape)          48  0         80  P        112  p
 17  DC1 (device control 1)          49  1         81  Q        113  q
 18  DC2 (device control 2)          50  2         82  R        114  r
 19  DC3 (device control 3)          51  3         83  S        115  s
 20  DC4 (device control 4)          52  4         84  T        116  t
 21  NAK (negative acknowledge)      53  5         85  U        117  u
 22  SYN (synchronous idle)          54  6         86  V        118  v
 23  ETB (end of trans. block)       55  7         87  W        119  w
 24  CAN (cancel)                    56  8         88  X        120  x
 25  EM  (end of medium)             57  9         89  Y        121  y
 26  SUB (substitute)                58  :         90  Z        122  z
 27  ESC (escape)                    59  ;         91  [        123  {
 28  FS  (file separator)            60  <         92  \        124  |
 29  GS  (group separator)           61  =         93  ]        125  }
 30  RS  (record separator)          62  >         94  ^        126  ~
 31  US  (unit separator)            63  ?         95  _        127  DEL
---------------------------------------------------------------------------
  For a keyboard with an ISO physical layout, use the scan codes below:
      +---+---+---+---+---+---+---+---+---+---+---+---+---+-------+
      |01 |   |   |   |   |   |   |   |   |   |   |   |   |       |
      +---+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-----+

      +---+---+---+---+---+---+---+---+---+---+---+---+---+-------+
      |35 |1e |1f |20 |21 |22 |23 |24 |25 |26 |27 |2d |2e |BackSp |
      +---+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-----+
      | Tab |14 |1a |08 |15 |17 |1c |18 |0c |12 |13 |2f |30 | Ret |
      +-----++--++--++--++--++--++--++--++--++--++--++--++--++    |
      |CapsL |04 |16 |07 |09 |0a |0b |0d |0e |0f |33 |34 |31 |    |
      +----+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+---+----+
      |Shi.|32 |1d |1b |06 |19 |05 |11 |10 |36 |37 |38 |  Shift   |
      +----+---++--+-+-+---+---+---+---+---+--++---+---++----+----+
      |Ctrl|Win |Alt |                        |AlGr|Win |Menu|Ctrl|
      +----+----+----+------------------------+----+----+----+----+

| a # £ \ @ " ~ ¬ ` ! $ % ^ & * ( ) - = _ + [ ] { } ' ; : , . / < > ?

*/
#define SHIFT 0x80
#define ALT_GR 0xc0
#define ISO_KEY 0x64
#define ISO_REPLACEMENT 0x32
#define ESCAPE_CHAR 92
#define GAP_MS_DEFAULT 5
#define PRESS_RELEASE_MS 5

void logLine(String s, int i);

extern const uint8_t KeyboardLayout_en_UK[128] PROGMEM = {
  0x20 | SHIFT,  // NUL Remap 00 to 0x20 | SHIFT for £ (dec 163)
  0x35 | SHIFT,  // SOH Remap 00 to 0x20 | SHIFT for ¬ (dec 172)
  0x48,          // STX UP!
  0xE0,          // ETX
  0x00,          // EOT
  0x00,          // ENQ
  0x00,          // ACK
  0x00,          // BEL
  0x2a,          // BS  Backspace
  0x2b,          // TAB Tab
  0x28,          // LF  Enter
  0x00,          // VT
  0x00,          // FF
  0x00,          // CR
  0x00,          // SO
  0x00,          // SI
  0x00,          // DEL
  0x00,          // DC1
  0x00,          // DC2
  0x00,          // DC3
  0x00,          // DC4
  0x00,          // NAK
  0x00,          // SYN
  0x00,          // ETB
  0x00,          // CAN
  0x00,          // EM
  0x00,          // SUB
  0x01,          // ESC Remap 00 to 0x01 for ESC
  0x00,          // FS
  0x00,          // GS
  0x00,          // RS
  0x00,          // US

  0x2c,          // ' '
  0x1e | SHIFT,  // !
  0x1f | SHIFT,  // " (Dec 34) From 34 to 1f
  0x31,          // # Map from 0x20|SHIFT (dec 35),
  0x21 | SHIFT,  // $
  0x22 | SHIFT,  // %
  0x24 | SHIFT,  // &
  0x34,          // '
  0x26 | SHIFT,  // (
  0x27 | SHIFT,  // )
  0x25 | SHIFT,  // *
  0x2e | SHIFT,  // +
  0x36,          // ,
  0x2d,          // -
  0x37,          // .
  0x38,          // /
  0x27,          // 0
  0x1e,          // 1
  0x1f,          // 2
  0x20,          // 3
  0x21,          // 4
  0x22,          // 5
  0x23,          // 6
  0x24,          // 7
  0x25,          // 8
  0x26,          // 9
  0x33 | SHIFT,  // :
  0x33,          // ;
  0x36 | SHIFT,  // <
  0x2e,          // =
  0x37 | SHIFT,  // >
  0x38 | SHIFT,  // ?
  0x34 | SHIFT,  // @ (Dec 64) From 1f to 34
  0x04 | SHIFT,  // A
  0x05 | SHIFT,  // B
  0x06 | SHIFT,  // C
  0x07 | SHIFT,  // D
  0x08 | SHIFT,  // E
  0x09 | SHIFT,  // F
  0x0a | SHIFT,  // G
  0x0b | SHIFT,  // H
  0x0c | SHIFT,  // I
  0x0d | SHIFT,  // J
  0x0e | SHIFT,  // K
  0x0f | SHIFT,  // L
  0x10 | SHIFT,  // M
  0x11 | SHIFT,  // N
  0x12 | SHIFT,  // O
  0x13 | SHIFT,  // P
  0x14 | SHIFT,  // Q
  0x15 | SHIFT,  // R
  0x16 | SHIFT,  // S
  0x17 | SHIFT,  // T
  0x18 | SHIFT,  // U
  0x19 | SHIFT,  // V
  0x1a | SHIFT,  // W
  0x1b | SHIFT,  // X
  0x1c | SHIFT,  // Y
  0x1d | SHIFT,  // Z
  0x2f,          // [
  0x32,          // bslash (dec 92) From  0x31,
  0x30,          // ]
  0x23 | SHIFT,  // ^
  0x2d | SHIFT,  // _
  0x35,          // ` (Dec 96)
  0x04,          // a
  0x05,          // b
  0x06,          // c
  0x07,          // d
  0x08,          // e
  0x09,          // f
  0x0a,          // g
  0x0b,          // h
  0x0c,          // i
  0x0d,          // j
  0x0e,          // k
  0x0f,          // l
  0x10,          // m
  0x11,          // n
  0x12,          // o
  0x13,          // p
  0x14,          // q
  0x15,          // r
  0x16,          // s
  0x17,          // t
  0x18,          // u
  0x19,          // v
  0x1a,          // w
  0x1b,          // x
  0x1c,          // y
  0x1d,          // z
  0x2f | SHIFT,  // {
  0x32 | SHIFT,  // |
  0x30 | SHIFT,  // }
  0x31 | SHIFT,  // ~ (Dec 126) from 0x35 | SHIFT to 0x31 | SHIFT
  0x00           // DEL
};

bool escaping = false;
int gapMs = GAP_MS_DEFAULT;
char numberArray[20];  // Use for number to string conversion

void sendNumber(int i, bool hex) {
  if (i == 32) {
    return;
  }
  Keyboard.write('[');
  if (hex) {
    itoa(i, numberArray, 16);
    Keyboard.write('0');
    Keyboard.write('x');
  } else {
    itoa(i, numberArray, 10);
  }
  for (int xx = 0; xx < 4; xx++) {
    if (numberArray[xx] == 0) {
      break;
    }
    Keyboard.write(numberArray[xx]);
  }
  Keyboard.write(']');
}

// | \\ \a \n// ...
// | a # £ \ @ " ~ ¬ ` ! $ % ^ & * ( ) - = _ + [ ] { } ' ; : , . / < > ?
// | a # £ \ @ " ~ ¬ ` ! $ % ^ & * ( ) - = _ + [ ] { } ' ; : , . / < > ?"
//
//
int pressKeyInt(int key) {
  Keyboard.press(key);
  delay(PRESS_RELEASE_MS);
  Keyboard.release(key);
  delay(gapMs - PRESS_RELEASE_MS);
  return 1;
}

int sendKeyInt(int key) {
  int count = 0;
  if (escaping) {
    escaping = false;
    if ((key >= '0') && (key <= '9')) {
      gapMs = ((key - '0') * GAP_MS_DEFAULT) + PRESS_RELEASE_MS;
      logLine(String(itoa(gapMs, numberArray, 10)), 10);
      return count;
    }
    switch (key) {
      case 'n':
        key = 10;
        break;
      case 't':
        key = 9;
        break;
      case 'b':
        key = 8;
        break;
      case 'e':
        return pressKeyInt(KEY_ESC);
      case 'u':
        return pressKeyInt(KEY_UP_ARROW);
      case 'd':
        return pressKeyInt(KEY_DOWN_ARROW);
      case 'l':
        return pressKeyInt(KEY_LEFT_ARROW);
      case 'r':
        return pressKeyInt(KEY_RIGHT_ARROW);
      case ESCAPE_CHAR:
        break;
      default:
        count = count + pressKeyInt(ESCAPE_CHAR);
    }
  } else {
    if (key == ESCAPE_CHAR) {
      escaping = true;
      return count;
    }
  }
  switch (key) {
    case 194:
      return count;
    case 163:  // Map £
      count = count + pressKeyInt(0);
      return count;
    case 172:  // MAp ¬
      count = count + pressKeyInt(1);
      return count;
    default:
      count = count + pressKeyInt(key);
  }
  return count;
}
