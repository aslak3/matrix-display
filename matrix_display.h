#define DEBUG_MSGS

#ifdef DEBUG_MSGS
#define DEBUG_printf(...) printf(__VA_ARGS__)
#else
#define DEBUG_printf(...)
#endif
