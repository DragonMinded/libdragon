/**
 * @file debugcpp.cpp
 * @brief Debugging Support (C++)
 */

#include "debug.h"
#include "exception_internal.h"
#include <exception>
#include <cxxabi.h>
#include <cstdlib>

static void terminate_handler(void)
{
    std::exception_ptr eptr = std::current_exception();
    if (eptr) {
        try {
            std::rethrow_exception(eptr);
        }
        catch (const std::exception& e) 
        {
            char buf[1024]; size_t sz = sizeof(buf);
            char *demangled = abi::__cxa_demangle(typeid(e).name(), buf, &sz, NULL);
            __inspector_cppexception(demangled, e.what());
        }
        catch (...) 
        {
            __inspector_cppexception(NULL, "Unknown exception");
        }
    }
    else
    {
        __inspector_cppexception(NULL, "Direct std::terminate() call");
    }    
}

/** @brief Initialize debug support for C++ programs */
void __debug_init_cpp(void)
{
    std::set_terminate(terminate_handler);
}
