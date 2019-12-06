/**
REVISION HISTORY

Version 1.0
 * Created by Henrik Ekblad <henrik.ekblad@mysensors.org>
 * Copyright (C) 2013-2015 Sensnology AB
 * Full contributor list: https://github.com/mysensors/Arduino/graphs/contributors
 *
 * Documentation: http://www.mysensors.org
 * Support Forum: http://forum.mysensors.org

Version 1.1 - Modified by Marc Stähli (different display for approaching, code cleanup)

DESCRIPTION
   Parking sensor using a neopixel led ring and distance sensor (HC-SR04).
   Configure the digital pins used for distance sensor and neopixels below.
   NOTE! Remeber to feed leds and distance sensor serparatly from your Arduino.
   It will probably not survive feeding more than a couple of LEDs. You
   can also adjust intesity below to reduce the power requirements.

MIT License

Copyright (c) 2019 3KU_Delta, Marc Stähli

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include <Adafruit_NeoPixel.h>
#include <NewPing.h>

#define NEO_PIN      6  // NeoPixels input pin

#define TRIGGER_PIN  4  // Arduino pin tied to trigger pin on the ultrasonic sensor.
#define ECHO_PIN     5  // Arduino pin tied to echo pin on the ultrasonic sensor.

#define NUMPIXELS      12 // Number of nexpixels in ring/strip
#define MAX_INTESITY   25  // Intesity of leds (in percentage). Remeber more intesity requires more power.

// The maximum rated measuring range for the HC-SR04 is about 400-500cm.
#define MAX_DISTANCE 80 // Max distance we want to start indicating green (in cm)
#define PANIC_DISTANCE 10 // Mix distance we red warning indication should be active (in cm)
#define PARKED_DISTANCE 20 // Distance when "parked signal" should be sent to controller (in cm)

#define PARK_OFF_TIMEOUT 10*1000 // Number of milliseconds until turning off light when parked.

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, NEO_PIN, NEO_GRB + NEO_KHZ800);

NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE); // NewPing setup of pins and maximum distance.

unsigned long sendInterval = 5000;  // Send park status at maximum every 5 second.
unsigned long lastSend;

unsigned long blinkInterval = 100;  // blink interval (milliseconds)
unsigned long lastBlinkPeriod;
bool blinkColor = true;

// To make a fading motion on the led ring/tape we only move one pixel/distDebounce time
unsigned long distDebounce = 15;
unsigned long lastDebouncePeriod;

int skipZero = 0;
int numLightPixels = 0;
int measuredDist;
int displayDist;
int newLightPixels;
bool moving = false;
unsigned long standingtime;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting park distance sensor");
  pixels.begin();                                  // Initializing NeoPixel library.
  Serial.println("Neopixels initialized");
}

void loop() {
  unsigned long now = millis();
  measuredDist = (sonar.ping_median() / US_ROUNDTRIP_CM);
  displayDist = min(measuredDist, MAX_DISTANCE);

  if (displayDist == 0 && skipZero < 10) {         // Try to filter zero readings
    skipZero++;
    return;
  }

  //******************************************************************************************************
  
  if (now - lastDebouncePeriod > distDebounce) {   // Check if it is time to alter the leds
    lastDebouncePeriod = now;
    if (displayDist == 0) {                        // No reading from sensor, assume no object found 
      if (numLightPixels > 0) {
        numLightPixels--;
      } else {
        numLightPixels = 0;
      }
    } else {
      skipZero = 0;
      newLightPixels = NUMPIXELS - (NUMPIXELS * (displayDist - PANIC_DISTANCE) / (MAX_DISTANCE - PANIC_DISTANCE));
      if (newLightPixels > numLightPixels) {          // level raising
        numLightPixels++;
        moving = true;
        Serial.print("NumLights raising: ");
        Serial.println(numLightPixels);
      } else if (newLightPixels < numLightPixels) {   // level lowering
        numLightPixels--;
        moving = true;
        Serial.print("NumLights falling: ");
        Serial.println(numLightPixels);
      } else if (newLightPixels == numLightPixels) {  // level steady
        if (moving) {
          moving = false;
          standingtime = now;
          Serial.print("NumLights steady: ");
          Serial.println(numLightPixels);
        }
      }
    }
  }

  //******************************************************************************************************

  if (!moving && now - standingtime > PARK_OFF_TIMEOUT) {  // no movement? switch off the lights
    for (int i = 0; i < NUMPIXELS; i++) {
      pixels.setPixelColor(i, pixels.Color(0, 0, 0));
    }
    Serial.println("NumLights off");
  }
  else if (numLightPixels >= NUMPIXELS) {                  // too close? intense red blinking
    if (now - lastBlinkPeriod > blinkInterval) {
      blinkColor = !blinkColor;
      lastBlinkPeriod = now;
    }
    for (int i = 0; i < numLightPixels; i++) {
      pixels.setPixelColor(i, pixels.Color(blinkColor ? 255 * MAX_INTESITY / 100 : 0, 0, 0));
    }
  }
  else {
    for (int i = 0; i < numLightPixels; i++) {             // normal procedure for updating leds
      int r = 255 * i / NUMPIXELS;
      int g = 255 - r;
      // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
      pixels.setPixelColor(i, pixels.Color(r * MAX_INTESITY / 100, g * MAX_INTESITY / 100, 0));
      pixels.setPixelColor(NUMPIXELS - i, pixels.Color(r * MAX_INTESITY / 100, g * MAX_INTESITY / 100, 0));
    }
    if (numLightPixels <= NUMPIXELS / 2) {                 // Turn off the remaining lights
      for (int i = numLightPixels; i <= NUMPIXELS / 2; i++) {
        pixels.setPixelColor(i, pixels.Color(0, 0, 0));
        pixels.setPixelColor(NUMPIXELS - i, pixels.Color(0, 0, 0));
      }
    }
  }
  pixels.show(); // This sends the updated pixel color to the hardware.
}
