/**
 * @file debugcpp.cpp
 * @brief Debugging Support (C++)
 */

#include "debug.h"
#include "dlfcn_internal.h"
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

static char *demangle_name(char *name)
{
    return abi::__cxa_demangle(name, NULL, NULL, NULL);
}

/** @brief Initialize debug support for C++ programs */
void __debug_init_cpp(void)
{
    static bool init = false;
    if (init) return;
    std::set_terminate(terminate_handler);
    __dl_demangle_func = demangle_name;
    init = true;
}
