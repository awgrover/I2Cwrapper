/*!
   @file simpleBuffer.h
   @brief Simple and ugly serialization buffer for any data type. Just a buffer,
   a current position pointer used for reading from *and* writing to the buffer,
   and template functions for reading and writing any data type. The template
   technique is adapted from Nick Gammon's I2C_Anything library.
   @section author Author
   Copyright (c) 2022 juh
   @section license License
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, version 2.
*/

#ifndef SimpleBuffer_h
#define SimpleBuffer_h


#include <Arduino.h>
//#include <cstring>
//#include <stdint.h>


class SimpleBuffer {
 public:
  /*!
   * @brief Allocate and reset buffer.
   * @param buflen Maximum length of buffer in bytes.
  */
  void init(uint8_t buflen);
  /*!
   * @brief Write any basic data type to the buffer at the current position and
   * increment the position pointer according to the type's size.
   * @param value Data to write.
  */
  template <typename T> void write(const T& value);
  /*!
   * @brief Read any basic data type from the buffer from the current position and
   * increment the position pointer according to the type's size.
   * @param value Variable to read to. Amount of data read depends on size of this
   * type.
  */
  template <typename T> void read(T& value);
  /*!
   * @brief Reset the position pointer to the start of the buffer (0) without
   * changing the buffer contents. Usually, this will be called before writing new
   * data *and* before reading it.
  */
  void reset();


  /*!
   * @brief Calculate CRC8 checksum for the currently used buffer ([1]...[idx-1])
   * and store it in the first byte [0].
   */
  void setCRC8();

  /*!
   * @brief Check for correct CRC8 checksum. First byte [0] holds the checksum,
   * rest of the currently used buffer is checked.
   * @returns true CRC8 matches rest of buffer.
   */
  bool checkCRC8();


  // the next three should be private, let's keep 'em public for quick'n'dirty direct access

  /*!
   * @brief The allocated buffer.
  */
  uint8_t * buffer;
  /*!
   * @brief The position pointer.
  */
  uint8_t idx; // next position to write to / read from
  /*!
  * @brief Maximum length of buffer in bytes. Read and write operations use this
  * to check for sufficient space (no error handling, though).
  */
  uint8_t maxLen;

 private:
  uint8_t calculateCRC8 ();
};


template <typename T> void SimpleBuffer::write(const T& value) {
  if (idx + sizeof (value) <= maxLen) { // enough space to write to?
    memcpy(&buffer[idx], &value, sizeof (value));
    idx += sizeof (value);
  }
}

// T should be initiated before calling this, as it might return unchanged due to if() statement
template <typename T> void SimpleBuffer::read(T& value) {
  if (idx + sizeof (value) <= maxLen) { // enough space to read from?
    memcpy(&value, &buffer[idx], sizeof (value));
    idx += sizeof (value);
  }
}

#endif
