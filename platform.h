//  Load the platform-dependent system include files, e.g. Arduino.h
//  Also define the Serial buffer size.  Use platform.h instead of Arduino.h
//  for portability.
#ifndef PLATFORM_H_
#define PLATFORM_H_

#ifdef ARDUINO
//  Reduce the Serial buffer size from 64 to 16 to reduce RAM usage.
#define SERIAL_TX_BUFFER_SIZE 16
#define SERIAL_RX_BUFFER_SIZE 16
#include <Arduino.h>
#endif  //  ARDUINO

#endif  //  PLATFORM_H_
