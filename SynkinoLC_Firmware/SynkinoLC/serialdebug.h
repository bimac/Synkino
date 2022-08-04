#pragma once

#ifdef SERIALDEBUG
  #define MYSERIAL Serial
#elif defined HWSERIALDEBUG
  #define MYSERIAL Serial1
#endif

#if defined(MYSERIAL)
  #define PRINT(...)    MYSERIAL.print(__VA_ARGS__)
  #define PRINTF(...)   MYSERIAL.printf(__VA_ARGS__)
  #define PRINTLN(...)  MYSERIAL.println(__VA_ARGS__)
#else
  #define PRINT(...)
  #define PRINTF(...)
  #define PRINTLN(...)
#endif
