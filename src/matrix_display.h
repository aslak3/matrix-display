#include <stdio.h>
#include <stdarg.h>

#if PICO_SDK
#include "pico/stdlib.h"
#elif ESP32_SDK
#include <stdlib.h>
#else
#error "PICO_SDK or ESP32_SDK needs to be set"
#endif

#define DEBUG_MSGS

// two macros ensures any macro passed will
// be expanded before being stringified
#define STRINGIZE_DETAIL(x) #x
#define STRINGIZE(x) STRINGIZE_DETAIL(x)

#ifdef DEBUG_MSGS
#define DEBUG_printf(...) printf(__FILE__ ":" STRINGIZE(__LINE__) " " __VA_ARGS__)
#else
#define DEBUG_printf(...)
#endif

#if ESP32_SDK
void panic(const char *format, ...);
#endif

#if PICO_SDK
#define GET_CORE_NUMBER get_core_num
#elif ESP32_SDK
#define GET_CORE_NUMBER xPortGetCoreID
#endif
