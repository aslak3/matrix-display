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
