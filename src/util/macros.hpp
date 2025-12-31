/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_MACROS_HPP_INCLUDED
#define SL_MACROS_HPP_INCLUDED

/******************************************************************************/
/*  ServerLink Internal Use                                                   */
/******************************************************************************/

#define SL_UNUSED(object) (void) object

#define SL_DELETE(p_object) \
    do { \
        delete p_object; \
        p_object = nullptr; \
    } while (0)

// C++11 keywords - always available since we require C++11
#define SL_NOEXCEPT noexcept
#define SL_OVERRIDE override
#define SL_FINAL final
#define SL_DEFAULT = default

// Non-copyable and non-movable class macro
#define SL_NON_COPYABLE_NOR_MOVABLE(classname) \
  public: \
    classname(const classname &) = delete; \
    classname &operator=(const classname &) = delete; \
    classname(classname &&) = delete; \
    classname &operator=(classname &&) = delete;

// Debug logging - only enabled when explicitly requested
#ifdef SL_ENABLE_DEBUG_LOG
    #include <cstdio>
    #define SL_DEBUG_LOG(...) fprintf(stderr, __VA_ARGS__)
#else
    #define SL_DEBUG_LOG(...)
#endif

#endif
