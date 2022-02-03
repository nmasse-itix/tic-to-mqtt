// This is a C to C++ bridge. It is required to call the LibTeleinfo (C++) from FreeRTOS (C).

#ifndef __LIBTELEINFO_H__
#define __LIBTELEINFO_H__

// The magic lays here...
#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

// Needed for uint8_t
#include <stdint.h>
// Needed for time_t
#include <time.h>

#define LIBTELEINFO_FLAGS_NONE     0x00
#define LIBTELEINFO_FLAGS_NOTHING  0x01
#define LIBTELEINFO_FLAGS_ADDED    0x02
#define LIBTELEINFO_FLAGS_EXIST    0x04
#define LIBTELEINFO_FLAGS_UPDATED  0x08
#define LIBTELEINFO_FLAGS_ALERT    0x80

typedef void(*libteleinfo_data_callback)(time_t,uint8_t,char*,char*);
typedef void(*libteleinfo_adps_callback)(uint8_t);

EXTERNC void libteleinfo_init(libteleinfo_data_callback, libteleinfo_adps_callback);
EXTERNC void libteleinfo_process(uint8_t* buffer, int len);

#endif
