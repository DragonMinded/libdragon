
#include <array>
#include <algorithm>

template<size_t size, typename T, typename C>
void my_stbrp_sort(T* ptr, std::size_t count, const C& comp) {
  static_assert(sizeof(T) == size);
  const auto begin = static_cast<T*>(ptr);
  const auto end = begin + count;
  std::sort(begin, end, [&](const T& a, const T& b) { return (comp(&a, &b) < 0); });
}
#define STBRP_SORT(PTR, COUNT, SIZE, COMP) my_stbrp_sort<(SIZE)>((PTR), (COUNT), (COMP))

#define STB_RECT_PACK_IMPLEMENTATION
#define STBRP_LARGE_RECTS
#include "stb_rect_pack.h"
