#ifdef __clang__
    #pragma clang diagnostic push
#else
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include "rect_pack/rect_pack.h"
#include "rect_pack/rect_pack.cpp"
#include "rect_pack/stb_rect_pack.cpp"
#include "rect_pack/MaxRectsBinPack.cpp"

#ifdef __clang__
    #pragma clang diagnostic pop
#else
    #pragma GCC diagnostic pop
#endif
