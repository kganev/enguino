// Copyright 2017, Thomas Court
//
// 'Print' to the Aux display
// --------------------------
//
//  This file is part of Enguino.
//
//  Enguino is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  Enguino is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with Enguino.  If not, see <http://www.gnu.org/licenses/>.



// Digital pin assignment
#define AUX_SWITCH   11

// lookup port mapping to pins here: https://www.arduino.cc/en/Reference/PortManipulation
#define SCL_PIN 0
#define SCL_PORT PORTD
#define SDA_PIN 1
#define SDA_PORT PORTD
#define I2C_SLOWMODE 1
#define I2C_TIMEOUT 10

#if __AVR__
#include "softI2CMaster.h"
#endif

#define HT16K33_OSCILATOR_ON 0x21

#define HT16K33_BLINK_CMD         0x80
#define HT16K33_BLINK_DISPLAYON   0x01
#define HT16K33_BLINK_OFF     (HT16K33_BLINK_CMD | HT16K33_BLINK_DISPLAYON | 0)
#define HT16K33_BLINK_2HZ     (HT16K33_BLINK_CMD | HT16K33_BLINK_DISPLAYON | 2)
#define HT16K33_BLINK_1HZ     (HT16K33_BLINK_CMD | HT16K33_BLINK_DISPLAYON | 4)
#define HT16K33_BLINK_HALFHZ  (HT16K33_BLINK_CMD | HT16K33_BLINK_DISPLAYON | 6)

#define HT16K33_CMD_BRIGHTNESS 0xE0
#define HT16K33_BRIGHT_MAX    (HT16K33_CMD_BRIGHTNESS | 15)
#define HT16K33_BRIGHT_MIN    (HT16K33_CMD_BRIGHTNESS | 0)

#define I2C_ADDRESS   (0x70<<1)    // second line is 0x71

// segments are aranged as follows (values are hexadecimal)
//
// +---  1 ---+
// |          |
// 20         2
// |          |
// +--- 40 ---+
// |          |
// 10         4
// |          |
// +---  8 ---+ *80

#define LED_TEXT(a,b,c,d) LED_ ## a, LED_ ## b, LED_ ## c, LED_ ## d

#define LED_0 0x3F
#define LED_1 0x06
#define LED_2 0x5B
#define LED_3 0x4F
#define LED_4 0x66
#define LED_5 0x6D
#define LED_6 0x7D
#define LED_7 0x07
#define LED_8 0x7F
#define LED_9 0x6F

#define LED_DP 0x80 // decimal point
#define LED__ 0x40  // minus sign
#define LED_  0x0   // blank

#define LED_A 0x77
#define LED_a 0x77    // looks like A
#define LED_B 0x7c    // looks like b
#define LED_b 0x7c
#define LED_C 0x39
#define LED_c 0x58
#define LED_D 0x5e    // looks like d
#define LED_d 0x5e
#define LED_E 0x79
#define LED_F 0x71
#define LED_f 0x71    // same as F
#define LED_G 0x3b
#define LED_g 0x6f
#define LED_H 0x76
#define LED_h 0x74
#define LED_i 0x4
#define LED_J 0x1e
#define LED_j 0x1e    // same as j
#define LED_L 0x38
#define LED_n 0x54
#define LED_O 0x3F    // same as 0
#define LED_o 0x5c
#define LED_P 0x73
#define LED_r 0x50
#define LED_S 0x6d    // same as 5
#define LED_s 0x6d    // same as S,5
#define LED_T 0x78    // looks like t
#define LED_t 0x78
#define LED_U 0x3e
#define LED_u 0x1c
#define LED_V 0x3e    // same as U
#define LED_v 0x1c    // same as u
#define LED_Y 0x6e
#define LED_y 0x6e    // same as Y
#define LED_Z 0x5b    // same as 2
#define LED_z 0x5b    // same as Z, 2

volatile byte switchDown;   // use this for detecting a key held down for a period of time
volatile byte switchUp;
volatile byte switchPress;   // use this to detect a short keypress, then reset to 0

static const byte addressDigit[] = { 1, 3, 7, 9 };    // skip the colon
static const byte characterMap[] = { LED_0, LED_1, LED_2, LED_3, LED_4, LED_5, LED_6, LED_7, LED_8, LED_9 };

static byte ledBuffer[17];   // first byte is 0 for the address, 2 is first digit, 4 the second, etc.

byte alertStatus;
#define STATUS_WARNING   0x4
#define STATUS_CAUTION 0x5
#define STATUS_NORMAL  0x1

byte colon;
#define LED_COLON 2

void writeI2C(byte line, byte *buffer, byte len) {
  if (i2c_start((I2C_ADDRESS | (line<<1)) | I2C_WRITE)) {
    while (len--) {
      if (!i2c_write(*buffer++))
        break;
    }
  }
  i2c_stop();
}

// use this to set blink or brightness
void commandLED(byte line, byte command) {
  writeI2C(line, &command, 1);
}

void writeLED(byte line) {
  writeI2C(line, ledBuffer, sizeof(ledBuffer));
}

void printLEDRawDigits(byte offset, word val) {
  while (offset--) {
    ledBuffer[addressDigit[offset]] = characterMap[val%10];
    val /= 10;
    if (val == 0)
      break;
  }
}

// Number is clipped at 999 and has a decimal point at 1.
// Numbers greater than 99 are displayed whole, smaller in tenths
void printLEDRawHalfDigits(byte offset, word number) {
  if (short(number) == FAULT) {
    ledBuffer[addressDigit[offset-1]] = LED__;
    ledBuffer[addressDigit[offset-2]] = LED__;
  }
  else {
    if (number < 0)
      number = 0;
    if (number > 999)
      number = 999;
    if (number < 100) {
      printLEDRawDigits(offset, number);
      ledBuffer[addressDigit[offset-2]] |= LED_DP;   // decimal point
    }
    else
      printLEDRawDigits(offset, number/10);
  }
}

void printStatus(byte line) {
  memset(ledBuffer, 0, sizeof(ledBuffer));
  if (line == 0) {
    // 4/9 duty cycle for caution/alarm indicator  (22 ma max current, 13 ma typical)
    ledBuffer[6] = ledBuffer[14] = ((alertStatus == STATUS_CAUTION) ? STATUS_NORMAL : alertStatus);  // yellow = 2/4 green +
    ledBuffer[2] = ledBuffer[10] = alertStatus;                                                      //          2/4 green + 2/4 red
  }
}

// -------------------------------------------------------

void printLEDSetup() {
  // Aux switch grounds pin when pressed, pollAuxSwitch watches for changes in this pin
  pinMode(AUX_SWITCH, INPUT_PULLUP);

  i2c_init();

  for (byte line=0; line<2; line++) {
    commandLED(line, HT16K33_OSCILATOR_ON);
    commandLED(line, HT16K33_BLINK_OFF);
    commandLED(line, HT16K33_BRIGHT_MAX);
  }
}

// print a text message to the LED on line 0 or 1
void printLED(byte line, byte a, byte b, byte c, byte d) {
  printStatus(line);
  ledBuffer[1] =  a;
  ledBuffer[3] =  b;
  ledBuffer[5] =  colon; // turn colon off
  ledBuffer[7] =  c;
  ledBuffer[9] = d;
  writeLED(line);
}

// print a text message to the LED on line 0 or 1
void printLED(byte line, byte *txt) {
  printLED(line, txt[0], txt[1], txt[2], txt[3]);
}

// print the fuel gauge (e.g. 2.5:17)  (left tank : right tank)
void printLEDFuel(short left, short right) {
  memset(ledBuffer, 0, sizeof(ledBuffer));
  ledBuffer[1] = ledBuffer[7] = 0;
  printLEDRawHalfDigits(2, left);
  printLEDRawHalfDigits(4, right);
  ledBuffer[5] = LED_COLON;
  writeLED(1);
}

// print the 'number' to 'line' 0 or 1, place a decimal point 'decimal' digits to the left
void printLED(byte line, short number, byte decimal) {
  if (number == FAULT) {
    printLED(line, LED_TEXT(i,n,o,P));
  }
  else {
    if (number < 0)
      number = 0;
    if (number > 9999)
      number = 9999;
    printStatus(line);
    printLEDRawDigits(4, number);
    if (decimal != 0)
      ledBuffer[addressDigit[3-decimal]] |= LED_DP;
  }
  writeLED(line);
}

// call this about 8 times a second in an IRQ
inline void pollAuxSwitch() {
  if (digitalRead(AUX_SWITCH)) {
    // switch up
    if (switchUp < 255)
      switchUp++;
    if (switchUp >  2) {    // debounce test, button must be up for more than 1/4 second before considered a button up state
      if (switchDown)
        switchPress = switchDown;
      switchDown = 0;
    }
   }
  else {
    // switch is down
    ++switchDown;
    switchUp = 0;
  }
}
