#ifndef __util_h__
#define __util_h__ 1

#ifdef UNUSED
#elif defined(__GNUC__)
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
#else
# define UNUSED(x) x
#endif

#define log_error(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)

#ifdef DEBUG
#define log_debug(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#else
#define log_debug(fmt, ...) ;
#endif

extern unsigned int maxbit;

#endif
