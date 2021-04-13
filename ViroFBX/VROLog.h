//
//  VROLog.h
//  ViroRenderer
//
//  Created by Raj Advani on 10/21/15.
//  Copyright © 2015 Viro Media. All rights reserved.
//

#include "VRODefines.h"

#pragma once
//// Test debug defines
//#ifdef NDEBUG
//#pragma message "NDEBUG is #defined."
//#else
//#pragma message "NDEBUG is not #defined."
//#endif

#undef PASSERT_INNERLOOPS_ENABLED // Do not define, unless you want passert_innerloop() asserts
// to run, which are passert_msg() calls that are made inside inner loops, which would slow down
// the app massively if on.  Used for debugging those particular loops only, and left off to not slow
// the app down for other developers.

/////////////////////////////////////////////////////////////////////////////////
//
//  Linux: Logging
//
/////////////////////////////////////////////////////////////////////////////////
#pragma mark -
#pragma mark Linux Logging

#if VRO_PLATFORM_LINUX

/* #include <android/log.h> */
#include <jni.h>
/* #include <sys/system_properties.h> */

// --------------------------------
// Escape sequences for logging color:
// 16-color = 4-bits = [highlight][blue][green][red]
// These are #defines instead of const char [] so they can be used more easily in perr() and pfatal().
// http://en.wikipedia.org/wiki/ANSI_escape_code

// --------
// special
#define ANSINoColor "\e[0m" // reset / normal
#define ANSIBold "\e[1m" // bold

// NOTE: the following uses 0 (reset) and 1 (bold) for intensities,
// which is used by BOTH foreground and background,
// and 0 will reset BOTH the foreground and background,
// thus this doesn't work: ANSIBackBlue ANSICyan
// since the "0" in Cyan is a reset, which resets the background.

// --------
// foreground
// light colors
#define ANSIBlack "\e[0;30m"
#define ANSIRed "\e[0;31m"
#define ANSIGreen "\e[0;32m"
#define ANSIBrown "\e[0;33m"
#define ANSIDarkYellow "\e[0;33m"
#define ANSIBlue "\e[0;34m"
#define ANSIMagenta "\e[0;35m"
#define ANSICyan "\e[0;36m"
#define ANSILightGray "\e[0;37m"
// bright colors
#define ANSIDarkGray "\e[1;30m"
#define ANSILightRed "\e[1;31m"
#define ANSILightGreen "\e[1;32m"
#define ANSIYellow "\e[1;33m"
#define ANSILightBlue "\e[1;34m"
#define ANSILightMagenta "\e[1;35m"
#define ANSILightCyan "\e[1;36m"
#define ANSIWhite "\e[1;37m"
// default text color
#define ANSIDefault "\e[1;39m"

// --------
// background
// light colors
#define ANSIBackBlack "\e[0;40m"
#define ANSIBackRed "\e[0;41m"
#define ANSIBackGreen "\e[0;42m"
#define ANSIBackBrown "\e[0;43m"
#define ANSIBackBlue "\e[0;44m"
#define ANSIBackMagenta "\e[0;45m"
#define ANSIBackCyan "\e[0;46m"
#define ANSIBackLightGray "\e[0;47m"
// bright colors
#define ANSIBackDarkGray "\e[1;40m"
#define ANSIBackLightRed "\e[1;41m"
#define ANSIBackLightGreen "\e[1;42m"
#define ANSIBackYellow "\e[1;43m"
#define ANSIBackLightBlue "\e[1;44m"
#define ANSIBackLightMagenta "\e[1;45m"
#define ANSIBackLightCyan "\e[1;46m"
#define ANSIBackWhite "\e[1;47m"
// default background color
#define ANSIBackDefault "\e[1;49m"

#elif VRO_PLATFORM_MAC

/*
 ANSI colors don't resolve on the iOS debug console, so we
 deactivate them.
 */
#define ANSINoColor ""
#define ANSIBold ""
#define ANSIBlack ""
#define ANSIRed ""
#define ANSIGreen ""
#define ANSIBrown ""
#define ANSIDarkYellow ""
#define ANSIBlue ""
#define ANSIMagenta ""
#define ANSICyan ""
#define ANSILightGray ""
#define ANSIDarkGray ""
#define ANSILightRed ""
#define ANSILightGreen ""
#define ANSIYellow ""
#define ANSILightBlue ""
#define ANSILightMagenta ""
#define ANSILightCyan ""
#define ANSIWhite ""
#define ANSIDefault ""
#define ANSIBackBlack ""
#define ANSIBackRed ""
#define ANSIBackGreen ""
#define ANSIBackBrown ""
#define ANSIBackBlue ""
#define ANSIBackMagenta ""
#define ANSIBackCyan ""
#define ANSIBackLightGray ""
#define ANSIBackDarkGray ""
#define ANSIBackLightRed ""
#define ANSIBackLightGreen ""
#define ANSIBackYellow ""
#define ANSIBackLightBlue ""
#define ANSIBackLightMagenta ""
#define ANSIBackLightCyan ""
#define ANSIBackWhite ""
#define ANSIBackDefault ""

#endif

/////////////////////////////////////////////////////////////////////////////////
//
//  Linux: Logging
//
/////////////////////////////////////////////////////////////////////////////////
#pragma mark -
#pragma mark Linux Logging

#if VRO_PLATFORM_LINUX

#define pinfo(message,...) \
do { \
printf((message), ##__VA_ARGS__); \
printf("\n"); \
} while (0)

#define pwarn(message,...) \
do { \
printf((message), ##__VA_ARGS__); \
printf("\n"); \
} while (0)

#define perr(message,...) \
do { \
printf((message), ##__VA_ARGS__); \
printf("\n"); \
} while (0)

#define pfatal(message,...) \
do { \
printf((message), ##__VA_ARGS__); \
printf("\n"); \
} while (0)

// __android_log_assert():
// Log an assertion failure and SIGTRAP the process to have a chance
// to inspect it. This uses the FATAL priority.
#define passert(condition) \
    do \
    { \
        if (!(condition)) {                               \
            _pabort(__FILE__, __LINE__, __func__,                   \
                    "ASSERTION FAILED\n"                            \
                    "  Expression: %s",                             \
                    #condition);                                    \
        }                                                           \
    } while (0)

#elif VRO_PLATFORM_MAC

/////////////////////////////////////////////////////////////////////////////////
//
//  MacOS: Logging
//
/////////////////////////////////////////////////////////////////////////////////
#pragma mark -
#pragma mark iOS Logging

#include <assert.h>
#include <Foundation/Foundation.h>

#define passert(condition) (assert(condition))

#define pinfo(message,...) \
do { \
NSLog(@#message, ##__VA_ARGS__); \
} while (0)

#define pwarn(message,...) \
do { \
NSLog(@#message, ##__VA_ARGS__); \
} while (0)

#define perr(message,...) \
do { \
NSLog(@"Error: "#message, ##__VA_ARGS__); \
} while (0)

#define pfatal(message,...) \
do { \
NSLog(@"Fatal Error: "#message, ##__VA_ARGS__); \
} while (0)

#endif

/////////////////////////////////////////////////////////////////////////////////
//
//  Common: Stack Trace
//
/////////////////////////////////////////////////////////////////////////////////
#pragma mark -
#pragma mark (Common) Stack Trace

/*
 Print out a message and a stacktrace, including the current stackframe
 */
void pstack(const char *fmt, ...)
    __attribute__ ((format(printf, 1, 2)));

/*
 Print out a stacktrace, including the current stackframe
 */
void pstack();

/////////////////////////////////////////////////////////////////////////////////
//
//  Common: Abort
//
/////////////////////////////////////////////////////////////////////////////////
#pragma mark -
#pragma mark Common: Abort

/*
 Abort the program.
 Print out a stacktrace; then cause a SEG fault to halt execution.
 SEG fault will be at 0xdecafbad.
 */
#define pabort(...) _pabort(__FILE__, __LINE__, __func__, ##__VA_ARGS__ )

void _pabort(const char *file, int line, const char *func) __attribute__((noreturn));
void _pabort(const char* file, int line, const char *func, const char *fmt, ...)
    __attribute__ ((format(printf, 4, 5), noreturn));

/////////////////////////////////////////////////////////////////////////////////
//
//  Common: Assert with Message
//
/////////////////////////////////////////////////////////////////////////////////
#pragma mark -
#pragma mark Common: Assert with Message

/*
 Abort the program if 'condition' is false.
 Print out a stacktrace and formatted message; then cause a SEG fault to halt execution.
 SEG fault will be at 0xdecafbad.
 */
# define passert_msg(condition, ...)                                    \
    do {                                                                \
        if (! (condition))                                    \
            _pabort(__FILE__, __LINE__, __func__, ## __VA_ARGS__);      \
    } while (false)
