/* ServerLink DLL Export/Import Macros */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SERVERLINK_EXPORT_H
#define SERVERLINK_EXPORT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Symbol visibility and DLL export/import macros */
#if defined(_WIN32) || defined(__CYGWIN__)
    /* Windows DLL */
    #ifdef SL_BUILDING_DLL
        #ifdef __GNUC__
            #define SL_EXPORT __attribute__((dllexport))
        #else
            #define SL_EXPORT __declspec(dllexport)
        #endif
    #elif defined(SL_USING_DLL)
        #ifdef __GNUC__
            #define SL_EXPORT __attribute__((dllimport))
        #else
            #define SL_EXPORT __declspec(dllimport)
        #endif
    #else
        /* Static library */
        #define SL_EXPORT
    #endif
#else
    /* Unix-like systems with GCC/Clang */
    #if defined(__GNUC__) && __GNUC__ >= 4
        #define SL_EXPORT __attribute__((visibility("default")))
    #else
        #define SL_EXPORT
    #endif
#endif

/* Calling convention (Windows-specific) */
#if defined(_WIN32) && !defined(__GNUC__)
    #define SL_CALL __cdecl
#else
    #define SL_CALL
#endif

#ifdef __cplusplus
}
#endif

#endif /* SERVERLINK_EXPORT_H */
