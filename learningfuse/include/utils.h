#ifndef UTILS_H
#define UTILS_H

//https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html
#ifdef DEBUGGING
#if DEBUGGING
#define DEBUG(...) \
    do { printf(__VA_ARGS__); } while(0);
#else
#define DEBUG(str, ...)
#endif
#else
#define DEBUG(str, ...)
#endif

#endif //UTILS_H
