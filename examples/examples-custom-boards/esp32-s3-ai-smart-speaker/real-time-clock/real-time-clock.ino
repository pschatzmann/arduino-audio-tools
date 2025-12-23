/**
 * @file real-time-clock.ino
 * @brief RTC PCF85063A example for ESP32-S3-AI-Smart-Speaker
 *
 * Dependencies:
 * - https://github.com/pschatzmann/arduino-audio-tools
 * - https://github.com/pschatzmann/arduino-audio-driver
 * - https://github.com/SolderedElectronics/Soldered-PCF85063A-RTC-Module-Arduino-Library
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "ESP32S3AISmartSpeaker.h"
#include "PCF85063A-SOLDERED.h"

PCF85063A rtc;
AudioBoardStream board(ESP32S3AISmartSpeaker);

void setup() {
  Serial.begin(115200);

  // Initialize audio board: I2C
  board.begin();

  // Initialize RTC module
  rtc.begin();

  //  setTime(hour, minute, sec);
  rtc.setTime(6, 54, 00);  // 24H mode, ex. 6:54:00
  //  setDate(weekday, day, month, yr);
  rtc.setDate(6, 16, 5, 2020);  // 0 for Sunday, ex. Saturday, 16.5.2020.
}

void loop() {
  printCurrentTime();  // Call funtion printCurrentTime()
  delay(1000);
}

void printCurrentTime() {
  // Get weekday, 0 is Sunday and decode to string
  switch (rtc.getWeekday()) {
    case 0:
      Serial.print("Sunday , ");
      break;
    case 1:
      Serial.print("Monday , ");
      break;
    case 2:
      Serial.print("Tuesday , ");
      break;
    case 3:
      Serial.print("Wednesday , ");
      break;
    case 4:
      Serial.print("Thursday , ");
      break;
    case 5:
      Serial.print("Friday , ");
      break;
    case 6:
      Serial.print("Saturday , ");
      break;
  }

  Serial.print(rtc.getDay());  // Function for getting day in month
  Serial.print(".");
  Serial.print(rtc.getMonth());  // Function for getting month
  Serial.print(".");
  Serial.print(rtc.getYear());  // Function for getting year
  Serial.print(". ");
  Serial.print(rtc.getHour());  // Function for getting hours
  Serial.print(":");
  Serial.print(rtc.getMinute());  // Function for getting minutes
  Serial.print(":");
  Serial.println(rtc.getSecond());  // Function for getting seconds
}
