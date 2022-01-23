/*!
  @file AccelStepperI2C.h
  @brief Arduino library for I2C-control of stepper motors connected to
  another Arduino which runs the associated AccelStepperI2C firmware
  @section author Author
  Copyright (c) 2022 juh
  @section license License
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation, version 2.
  @todo Error handling and sanity checks are rudimentary. Currently the
  system is not stable against transmission errors, partly due to the limitations
  of Wire.requestFrom() and Wire.available(). A better protocol would be
  needed, which would mean more overhead. Need testing to see how important
  this is in practics.
  @todo Implement interrupt mechanism, enabling the slave to inform the master
  about finished tasks or other events
  @todo Make the I2C address programmable and persistend in EEPROM.
  @todo Versioning, I2C command to request version (important, as library and firmware
  always need to match)
  @todo make time the slave has to answer I2C requests (I2CrequestDelay)
  configurable, as it will depend on µC and bus frequency etc.
  @todo implement diagnostic functions, e.g. measurements how long messages take 
  to be processed
  @todo <del>Implement end stops/reference stops.</del>
  @par Revision History
  @version 0.1 initial release
*/

#ifndef AccelStepperI2C_h
#define AccelStepperI2C_h

#include <Arduino.h>
#include <AccelStepper.h>
#include <SimpleBuffer.h>


// upper limit of send and receive buffer(s)
const uint8_t maxBuf = 16; // includes 1 crc8 byte

// ms to wait between I2C communication, this needs to be improved, a fixed constant is much too arbitrary
const uint16_t I2CrequestDelay = 30;

// used as error code for calls with long/float result that got no correct reply from slave ###broken, remove
/// @todo fix (i.e. remove) error reporting
const long resError = -2147483648;

// I2C commands and, if non void, returned bytes, for AccelStepper functions, starting at 10
// note: not all of those are necessarily implemented yet
const uint8_t moveToCmd             = 10;
const uint8_t moveCmd               = 11;
const uint8_t runCmd                = 12; const uint8_t runResult                 = 1; // 1 boolean
const uint8_t runSpeedCmd           = 13; const uint8_t runSpeedResult            = 1; // 1 boolean
const uint8_t setMaxSpeedCmd        = 14;
const uint8_t maxSpeedCmd           = 15; const uint8_t maxSpeedResult            = 4; // 1 float
const uint8_t setAccelerationCmd    = 16;
const uint8_t setSpeedCmd           = 17;
const uint8_t speedCmd              = 18; const uint8_t speedResult               = 4; // 1 float
const uint8_t distanceToGoCmd       = 19; const uint8_t distanceToGoResult        = 4; // 1 long
const uint8_t targetPositionCmd     = 20; const uint8_t targetPositionResult      = 4; // 1 long
const uint8_t currentPositionCmd    = 21; const uint8_t currentPositionResult     = 4; // 1 long
const uint8_t setCurrentPositionCmd = 22;
const uint8_t runToPositionCmd      = 23; // blocking
const uint8_t runSpeedToPositionCmd = 24;  const uint8_t runSpeedToPositionResult = 1; // 1 boolean
const uint8_t runToNewPositionCmd   = 25; // blocking
const uint8_t stopCmd               = 26;
const uint8_t disableOutputsCmd     = 27;
const uint8_t enableOutputsCmd      = 28;
const uint8_t setMinPulseWidthCmd   = 29;
const uint8_t setEnablePinCmd       = 30;
const uint8_t setPinsInverted1Cmd   = 31;
const uint8_t setPinsInverted2Cmd   = 32;
const uint8_t isRunningCmd          = 33; const uint8_t isRunningResult           = 1; // 1 boolean


// new commands for AccelStepperI2C start here

// new commands and for specific steppers, starting at 100
const uint8_t setStateCmd           = 100;
const uint8_t setInterruptCmd       = 101; // not implemented yet
const uint8_t setEndstopPinCmd      = 102;
const uint8_t enableEndstopsCmd     = 103;
const uint8_t endstopsCmd           = 104; const uint8_t endstopsResult           = 1; // 1 uint8_t

// new general commands that don't address a specific stepper (= have a 1 byte header), starting at 200
const uint8_t generalCommandsStart  = 200; // to check for general commands
const uint8_t addStepperCmd         = 200; const uint8_t addStepperResult         = 1; // 1 uint8_t
const uint8_t resetCmd              = 201;



/// @brief stepper state machine states
const uint8_t state_stopped = 0; ///< state machine is inactive, stepper can still be controlled directly
const uint8_t state_run = 1; ///< corresponds to AccelStepper::run(), will fall back to state_stopped if target reached
const uint8_t state_runSpeed = 2; ///< corresponds to AccelStepper::runSpeed(), will remain active until stopped by user (@todo: or endstop)
const uint8_t state_runSpeedToPosition = 3; ///< corresponds to AccelStepper::state_runSpeedToPosition(), will fall back to state_stopped if target position reached

//const uint8_t error_ok = 0;
//const uint8_t error_resultNotMatching = 1;

/*!
 * @brief Tells the slave to reset. Call this *before* adding any steppers *if*
 * master and slave resets are not synchronized by hardware. Else the slave
 * survives the master's reset, will think the master wants another stepper,
 * not a first one, and will run out of steppers, sooner or later.
 * @par All steppers are stopped by AccelStepper::stop() and disabled by
 * AccelStepper::disableOutputs(), after that a hardware reset is triggered.
 * @todo reset should be entirely done in software.
*/
void resetAccelStepperSlave(uint8_t address);


class AccelStepperI2C {
 public:
  /*!
   * @brief Constructor. Like the original, but needs an additional i2c address
   * parameter.
   * @param i2c_address Address of the controller the stepper is connected to. The library should support multiple controllers with different addresses, this is still untested, though.
   * @param interface Only AccelStepper::DRIVER is tested at the moment, but AccelStepper::FULL2WIRE, AccelStepper::FULL3WIRE, AccelStepper::FULL4WIRE, AccelStepper::HALF3WIRE, and AccelStepper::HALF4WIRE should work as well, AccelStepper::FUNCTION of course not
   * @result For the constructor, instead of checking sentOK and resultOK for
   * success, you can just check myNum to see if the slave successfully added
   * the stepper. If not, it's -1.
  */
  AccelStepperI2C(uint8_t i2c_address = 0x8,
                  uint8_t interface = AccelStepper::FULL4WIRE,
                  uint8_t pin1 = 2,
                  uint8_t pin2 = 3,
                  uint8_t pin3 = 4,
                  uint8_t pin4 = 5,
                  bool enable = true);
//    AccelStepper(void (*forward)(), void (*backward)());
  void    moveTo(long absolute);
  void    move(long relative);
  
  // don't use this, use state machine instead
  // If you use it, be sure to also check sentOK and resultOK.
  
  /*!
   * @brief Don't use this, use state machine instead with runState().
   * @result If you use it for whatever reason, check sentOK and resultOK to be sure that things are alright and the return value can be trusted.
   */
  boolean run();
  /*!
   * @brief Don't use this, use state machine instead with runSpeedState().
   * @result If you use it for whatever reason, check sentOK and resultOK to be sure that things are alright and the return value can be trusted.
   */
  boolean runSpeed();
  void    setMaxSpeed(float speed);
  float   maxSpeed();
  void    setAcceleration(float acceleration);
  void    setSpeed(float speed);
  float   speed();
  long    distanceToGo();
  long    targetPosition();
  long    currentPosition();
  void    setCurrentPosition(long position);
  // void    runToPosition();
  /*!
   * @brief Don't use this, use state machine instead with runSpeedToPositionState().
   * @result If you use it for whatever reason, check sentOK and resultOK to be sure that things are alright and the return value can be trusted.
   */
  boolean runSpeedToPosition();
  // void    runToNewPosition(long position);
  void    stop();
  void    disableOutputs();
  void    enableOutputs();
  void    setMinPulseWidth(unsigned int minWidth);
  void    setEnablePin(uint8_t enablePin = 0xff);
  void    setPinsInverted(bool directionInvert = false, bool stepInvert = false, bool enableInvert = false);
  void    setPinsInverted(bool pin1Invert, bool pin2Invert, bool pin3Invert, bool pin4Invert, bool enableInvert);
  bool    isRunning();

  //void setInterruptPin(int8_t interruptPin, bool activeLow = true);

  /*!
   * @brief Define a new endstop pin. Each stepper can have up to two, so don't
   * call this more than twice per stepper.
   * @param pin Pin the endstop switch is connected to.
   * @param activeLow True if activated switch will pull pin LOW, false if
   * HIGH.
   * @param internalPullup Use internal pullup for this pin.
   * @sa Use AccelStepperI2C::enableEndstops() to tell the state machine to actually
   * use the endstop(s). Use AccelStepperI2C::endstops() to read their currentl
   * state
   */
  void setEndstopPin(int8_t pin, bool activeLow, bool internalPullup);

  /*!
   * @brief Enable to tell the state machine to check the endstops after each
   * call to run(), runSpeed() or runSpeedToPosition(). If two switches are used,
   * it does not differentiate them. On a hit, it will simply revert to
   * state_stopped, but do nothing else.
   * @param enable true (default) to enable, false to disable.
   */

  void enableEndstops(bool enable = true);
  /*!
   * @brief Read current state of endstops
   * @returns One bit for each endstop, LSB is always the last addded endstop.
   *   Bits take activeLow setting in account, i.e. 0 for open, 1 for closed
   *   switch.
   */
  uint8_t endstops(); // returns endstop(s) states in bits 0 and 1

  /*!
   * @brief Set the state machine's state manually.
   * @param newState one of state_stopped, state_run, state_runSpeed, or state_runSpeedToPosition.
   */
  void setState(uint8_t newState);

  /*!
  * @brief Will stop any of the above states, i.e. stop polling. It does nothing else, so the master is solely in command of target, speed, and other settings.
  */
  void stopState();

  /*!
  * @brief Will poll run(), i.e. run to the target with acceleration and stop
     the state machine upon reaching it.
  */
  void runState();

  /*!
  * @brief Will poll runSpeed(), i.e. run at constant speed until told
  * otherwise (see AccelStepperI2C::stopState().
  */
  void runSpeedState();

  /*!
   * @brief Will poll runSpeedToPosition(), i.e. run at constant speed until target has been reached.
   */
  void runSpeedToPositionState();


  bool resultOK = true; // ###tells user if function call with return values returned successfully
  bool sentOK = true;   // ### tells user if command was not transferred to slave (i.e. sendCommand() failed)
  int8_t myNum = -1;

 private:
  uint8_t addStepper(uint8_t interface = AccelStepper::FULL4WIRE, uint8_t pin1 = 2, uint8_t pin2 = 3, uint8_t pin3 = 4, uint8_t pin4 = 5, bool enable = true);
  void prepareCommand(uint8_t cmd);
  bool sendCommand();
  bool readResult(uint8_t numBytes);

  uint8_t address;
  SimpleBuffer buf;

};


#endif
