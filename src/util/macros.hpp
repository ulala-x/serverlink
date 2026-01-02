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

// Non-copyable and non-movable class macro
#define SL_NON_COPYABLE_NOR_MOVABLE(classname) \
  public: \
    classname(const classname &) = delete; \
    classname &operator=(const classname &) = delete; \
    classname(classname &&) = delete; \
    classname &operator=(classname &&) = delete;

// Debug logging - only enabled when explicitly requested
#ifdef SL_ENABLE_DEBUG_LOG
    #include <serverlink/config.h>
    #if SL_HAVE_STD_FORMAT
        #include <format>
        #include <iostream>
        // std::format-based debug logging (C++20)
        // Note: Only used in debug builds, not performance-critical
        template<typename... Args>
        inline void sl_debug_log_impl(std::format_string<Args...> fmt, Args&&... args) {
            std::cerr << std::format(fmt, std::forward<Args>(args)...);
        }
        #define SL_DEBUG_LOG(...) ::sl_debug_log_impl(__VA_ARGS__)
    #else
        #include <cstdio>
        #define SL_DEBUG_LOG(...) fprintf(stderr, __VA_ARGS__)
    #endif
#else
    #define SL_DEBUG_LOG(...)
#endif

#endif
