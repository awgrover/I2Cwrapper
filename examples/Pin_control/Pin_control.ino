/*
   PinI2C Pin control demo
   (c) juh 2022

   Reads a digital and an analog input pin and mirrors their values on a
   digital and a PWM-capable output pin.
   Needs PinI2C.h module enabled in the target's firmware_modules.h.
   Note that the switch VALUE is "backwards": open=1,closed=0,
    but we reverse that for the LED's value so it follows the switch (closed=on)

   Wiring:
     Hardware             Pins        Notes

     Controller:
     I2C to Target        SDA/SCL/GND UNO=18/19/GND
     4.7k resistors       SDA & SCL   to logic-level voltage +
      These pull-ups are pretty important, I2C may not work without them.

     Target:
     I2C from Controller  SDA/SCL/GND UNO=18/19/GND
     switch n/o           12,GND      digital, open turns on LED
     LED, mirrors switch  13,GND      digital, w/usual 220ohm resistor, uno = LED_BUILTIN
     potentiometer 1-10K  Vcc,14,GND  analog-in, wiper on 14 (uno=A0)
     LED, mirrors pot     6,GND       PWM, w/usual 220ohm resistor
     Vusb from Controller Vusb        OR connect USB power to Target SEPARATELY
      CAUTION: be very careful if you connect power between Arduinos.
      If there is a USB power pin exposed, that should be safe.
      Note, on the Uno and Mega 2560 that pin is call "5V".
      Otherwise, just supply a separate USB power cable to the Target.

   How To:
    This sketch is written with Uno pins in mind. 
    You may have to change the pin numbers for other Arduinos.
    
    Pick 2 Arduinos. One will be the Controller, and one the Target.
    Pick Arduinos that have the same `Vcc`, ie. 3v or 5v `logic level`.
      (or you'll have to use a logic-level shifter) 
    And, the simplest switch: single-throw (or momentary), normally-open.

    Setup
    1. If you know the right pins on the Target, and you aren't using the default pins, 
      edit this sketch and adjust the pin numbers for your Arduino.
      Otherwise, you could do "Prove Target" (below) and use the first bit of output to help you.

    Prove Target
    1. Wire up the Target as above, but do not connect the I2C wires
    2. Do not connect Vcc/Vusb to the Controller
    3. Uncomment `#define I2CWRAPPER_LOOPBACK`
    4. Upload this sketch to the Target
       Watch for any "static assertion failed" erorrs, 
       this sketch can detect some cases where aPinIn and aPinOut are not valid.
    5. Open the console
       Note the first bit, it tries to give you a clue about analog-pins and LED_BUILTIN.
    Be patient with the following tests, the inputs are only tested every 1/2 second, so slow response
    6. Test the switch
       The `dPinOut` LED (builtin on Uno) should follow the switch open/closed.
       And the console will echo it.
    7. Test the potentiometer
       The `aPinOut` LED (e.g. PWM pin 6) should follow the potentiometer in brightness.
       And the console will echo it.
    8. Debug wiring if the LEDs or console in the above doesn't "follow" the hardware.
       And/or figure out the correct pin INTEGERS and adjust this sketch.

    Setup I2C: Controller & Target as target
    1. Open examples:i2cwrapper:firmware
    2. Edit firmware_modules.h, uncomment the line: #include "PinI2C_firmware.h"
    3  Save
    4. Upload to the Target
    5. check console? set debug?
    6. Disconnect the Target from USB
    7. Wire the Controller to Target with I2C
       Don't forget GND to GND
    8. Wire Vcc together.
       CAUTION: only if voltage is the same (3v vs 5v!)
       CAUTION: only if no USB is connected to the Target
       CAUTION: only if you know what Vcc's to connect.
       (or, no Vcc connection and using USB power on Target, still need GND to GND)
    9. Comment `#define I2CWRAPPER_LOOPBACK`
    10. Upload this sketch to the Controller
    11. Open the console
    12. Test the hardware on the Target just like "Prove Target" above!
        Same output to console!
    13. There should be helpful messages at each stage,
        and that should help you figure out if something hangs or goes wrong.
*/

//#define I2CWRAPPER_LOOPBACK

#include <Wire.h>
#include <PinI2C.h>

uint8_t i2cAddress = 0x08;

I2Cwrapper wrapper(i2cAddress); // each target device is represented by a wrapper...
#ifdef I2CWRAPPER_LOOPBACK
PinI2C_LoopBack pins(&wrapper); // ...that the pin interface needs to communicate with the target
#else
PinI2C pins(&wrapper); // ...that the pin interface needs to communicate with the target
#endif

// Note: These are pins on the Target, use integers, don't use names like "A0".
// You may have to deduce the integer value on the target with: Serial.println(A0);
const uint8_t dPinIn  = 12; // any pin; a simple switch (SPST normally-open) connects to GND.
const uint8_t dPinOut = 13; // any pin; connect LED with resistor or just use 13 = LED_BUILTIN on Uno/Nano
const uint8_t aPinIn  = 14; // needs analog-in pin; 14 is A0 on Uno, connect potentiometer against GND and +V
const uint8_t aPinOut = 6;  // needs PWM pin; 6 is PWM-capable on Uno/Nano; connect LED with resistor, or multimeter

void setup()
{
  Wire.begin();
  Serial.begin(115200);
  // all chips with "native" usb require waiting till `Serial` is true
  unsigned int begin_time=millis();
  while(! Serial && millis()-begin_time < 1000) { delay(10); } // but at most 1 sec if not plugged in to usb

  // Wire.setClock(10000); // uncomment for ESP8266 targets, to be on the safe side

#ifndef I2CWRAPPER_LOOPBACK
  if (!wrapper.ping()) {
    halt("Target not found! Check connections and restart.");
  } else {
    Serial.println("Target found as expected. Proceeding.\n");
  }
#else
  Serial.print(F( "I2CWRAPPER_LOOPBACK " __FILE__ " " __DATE__ " " __TIME__ " gcc " __VERSION__ " ide "));
  Serial.println(ARDUINO);
  
  // A little help on the pins
  static_assert( dPinIn < NUM_DIGITAL_PINS, "dPinIn is not a digital pin (too large)");
  static_assert( dPinOut < NUM_DIGITAL_PINS, "dPinOut is not a digital pin (too large)");
#ifndef ARDUINO_ARCH_SAMD
  // sadly not usable w/samd arduino code
  static_assert(digitalPinHasPWM(aPinOut), "aPinOut is not PWM capable on this Arduino");
#endif
  // doesn't cover all cases (e.g. int 13 on Uno):
  static_assert( analogInputToDigitalPin(aPinIn - analogInputToDigitalPin(0)) >= 0, "aPinIn is not analogRead() capable an this Arduino");
#ifdef LED_BUILTIN
  Serial.print("LED_BUILTIN="); Serial.println(LED_BUILTIN);
#else
  Serial.print("LED_BUILTIN not defined on this Arduino");
#endif
  Serial.print("A0="); Serial.println(A0);
  Serial.print("aPinIn "); Serial.print(aPinIn); Serial.print("=A"); Serial.println(aPinIn - analogInputToDigitalPin(0));

  Serial.println("First I2C, if it hangs here, connect I2C or at least put a pull-up on SDA & SCL...");
  if (!wrapper.ping()) {
    Serial.println("Loopback mode, no target found AS EXPECTED. Proceeding");
  } else {
    Serial.println("Loopback mode, Target UNEXPECTEDLY found. Proceeding.\n");
  }
#endif
  Serial.println("Rebooting Target, 1/2 second...");
  wrapper.reset(); // reset the target device
  delay(500); // and give it time to reboot

  Serial.println("Configuring Target pins...");
  pins.pinMode(dPinIn, INPUT_PULLUP); // INPUT_PULLUP is more universal for a simple switch, but "backwards" values
  pins.pinMode(dPinOut, OUTPUT);
  pins.pinMode(aPinIn, INPUT);
  pins.pinMode(aPinOut, OUTPUT);

  Serial.println("Setup() finished, running...");
}

void loop()
{
  pins.digitalWrite(dPinOut, ! pins.digitalRead(dPinIn)); // remember, switch is backwards
  pins.analogWrite(aPinOut, pins.analogRead(aPinIn) / 4);
  Serial.print("Digital input pin = "); Serial.print(pins.digitalRead(dPinIn)); // actual switch
  Serial.print(" | Analog input pin = "); Serial.println(pins.analogRead(aPinIn));
  delay(500);
}

void halt(const char* m) {
  Serial.println(m);
  Serial.println("\n\nHalting.\n");
  while (true) {
    yield(); // prevent ESPs from watchdog reset
  }
}
