#pragma once

#ifdef SERIALDEBUG
  #define PRINT(...)    Serial.print(__VA_ARGS__)
  #define PRINTF(...)   Serial.printf(__VA_ARGS__)
  #define PRINTLN(...)  Serial.println(__VA_ARGS__)
#else
  #define PRINT(...)
  #define PRINTF(...)
  #define PRINTLN(...)
#endif
