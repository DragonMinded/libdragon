
#define STBRP_LARGE_RECTS
#include "rect_pack.h"
#include "MaxRectsBinPack.h"
#include "stb_rect_pack.h"
#include <optional>
#include <algorithm>
#include <cmath>
#include <cassert>

namespace rect_pack {

namespace {
  const auto first_Skyline_method = Method::Skyline_BottomLeft;
  const auto last_Skyline_method = Method::Skyline_BestFit;
  const auto first_MaxRects_method = Method::MaxRects_BestShortSideFit;
  const auto last_MaxRects_method = Method::MaxRects_ContactPointRule;

  int floor(int v, int q) { return (v / q) * q; };
  int ceil(int v, int q) { return ((v + q - 1) / q) * q; };
  int sqrt(int a) { return static_cast<int>(std::sqrt(a)); }
  int div_ceil(int a, int b) { return (b > 0 ? (a + b - 1) / b : -1); }

  int ceil_to_pot(int value) {
    for (auto pot = 1; ; pot <<= 1)
      if (pot >= value)
        return pot;
  }

  int floor_to_pot(int value) {
    for (auto pot = 1; ; pot <<= 1)
      if (pot > value)
        return (pot >> 1);
  }

  bool is_stb_method(Method method) {
    const auto first = static_cast<int>(first_Skyline_method);
    const auto last = static_cast<int>(last_Skyline_method);
    const auto index = static_cast<int>(method);
    return (index >= first && index <= last);
  }

  bool is_rbp_method(Method method) {
    const auto first = static_cast<int>(first_MaxRects_method);
    const auto last = static_cast<int>(last_MaxRects_method);
    const auto index = static_cast<int>(method);
    return (index >= first && index <= last);
  }

  int to_stb_method(Method method) {
    assert(is_stb_method(method));
    return static_cast<int>(method) - static_cast<int>(first_Skyline_method);
  }

  rbp::MaxRectsBinPack::FreeRectChoiceHeuristic to_rbp_method(Method method) {
    assert(is_rbp_method(method));
    return static_cast<rbp::MaxRectsBinPack::FreeRectChoiceHeuristic>(
      static_cast<int>(method) - static_cast<int>(first_MaxRects_method));
  }

  std::vector<Method> get_concrete_methods(Method settings_method) {
    auto methods = std::vector<Method>();
    const auto add_skyline_methods = [&]() {
      methods.insert(end(methods), {
        Method::Skyline_BottomLeft,
        Method::Skyline_BestFit
      });
    };
    const auto add_maxrect_methods = [&]() {
      methods.insert(end(methods), {
        Method::MaxRects_BestShortSideFit,
        Method::MaxRects_BestLongSideFit,
        Method::MaxRects_BestAreaFit,
        Method::MaxRects_BottomLeftRule,
        // do not automatically try costy contact point rule
        // Method::MaxRects_ContactPointRule,
      });
    };
    switch (settings_method) {
      case Method::Best:
        add_skyline_methods();
        add_maxrect_methods();
        break;

      case Method::Best_Skyline:
        add_skyline_methods();
        break;

      case Method::Best_MaxRects:
        add_maxrect_methods();
        break;

      default:
        methods.push_back(settings_method);
        break;
    }
    return methods;
  }

  bool can_fit(const Settings& settings, int width, int height) {
    return ((width <= settings.max_width &&
             height <= settings.max_height) ||
             (settings.allow_rotate &&
              width <= settings.max_height &&
              height <= settings.max_width));
  }

  void apply_padding(const Settings& settings, int& width, int& height, bool indent) {
    const auto dir = (indent ? 1 : -1);
    width -= dir * settings.border_padding * 2;
    height -= dir * settings.border_padding * 2;
    width += dir * settings.over_allocate;
    height += dir * settings.over_allocate;
  }

  bool correct_settings(Settings& settings, std::vector<Size>& sizes) {
    // clamp min and max (not to numeric_limits<int>::max() to prevent overflow)
    const auto size_limit = 1'000'000'000;
    if (settings.max_width <= 0 || settings.max_width > size_limit)
      settings.max_width = size_limit;
    if (settings.max_height <= 0 || settings.max_height > size_limit)
      settings.max_height = size_limit;
    settings.min_width = std::clamp(settings.min_width, 0, settings.max_width);
    settings.min_height = std::clamp(settings.min_height, 0, settings.max_height);

    // immediately apply padding and over allocation, only relevant for power-of-two and alignment constraint
    apply_padding(settings, settings.min_width, settings.min_height, true);
    apply_padding(settings, settings.max_width, settings.max_height, true);

    auto max_rect_width = 0;
    auto max_rect_height = 0;
    for (auto it = begin(sizes); it != end(sizes); )
      if (it->width <= 0 ||
          it->height <= 0 ||
          !can_fit(settings, it->width, it->height)) {
        it = sizes.erase(it);
      }
      else {
        if (settings.allow_rotate && it->height > it->width) {
          max_rect_width = std::max(max_rect_width, it->height);
          max_rect_height = std::max(max_rect_height, it->width);
        }
        else {
          max_rect_width = std::max(max_rect_width, it->width);
          max_rect_height = std::max(max_rect_height, it->height);
        }
        ++it;
      }

    settings.min_width = std::max(settings.min_width, max_rect_width);
    settings.min_height = std::max(settings.min_height, max_rect_height);
    return true;
  }

  struct Run {
    Method method;
    int width;
    int height;
    std::vector<Sheet> sheets;
    int total_area;
  };

  void correct_size(const Settings& settings, int& width, int& height) {
    width = std::max(width, settings.min_width);
    height = std::max(height, settings.min_height);
    apply_padding(settings, width, height, false);

    if (settings.power_of_two) {
      width = ceil_to_pot(width);
      height = ceil_to_pot(height);
    }

    if (settings.align_width)
      width = ceil(width, settings.align_width);

    if (settings.square)
      width = height = std::max(width, height);

    apply_padding(settings, width, height, true);
    width = std::min(width, settings.max_width);
    height = std::min(height, settings.max_height);
    apply_padding(settings, width, height, false);

    if (settings.power_of_two) {
      width = floor_to_pot(width);
      height = floor_to_pot(height);
    }

    if (settings.align_width)
      width = floor(width, settings.align_width);

    if (settings.square)
      width = height = std::min(width, height);

    apply_padding(settings, width, height, true);
  }

  bool is_better_than(const Run& a, const Run& b, bool a_incomplete = false) {
    if (a_incomplete) {
      if (b.sheets.size() <= a.sheets.size())
        return false;
    }
    else {
      if (a.sheets.size() < b.sheets.size())
        return true;
      if (b.sheets.size() < a.sheets.size())
        return false;
    }
    return (a.total_area < b.total_area);
  }

  int get_perfect_area(const std::vector<Size>& sizes) {
    auto area = 0;
    for (const auto& size : sizes)
      area += size.width * size.height;
    return area;
  }

  std::pair<int, int> get_run_size(const Settings& settings, int area) {
    auto width = sqrt(area);
    auto height = div_ceil(area, width);
    if (width < settings.min_width || width > settings.max_width) {
      width = std::clamp(width, settings.min_width, settings.max_width);
      height = div_ceil(area, width);
    }
    else if (height < settings.min_height || height > settings.max_height) {
      height = std::clamp(height, settings.min_height, settings.max_height);
      width = div_ceil(area, height);
    }
    correct_size(settings, width, height);
    return { width, height };
  }

  std::pair<int, int> get_initial_run_size(const Settings& settings, int perfect_area) {
    return get_run_size(settings, perfect_area * 5 / 4);
  }

  enum class OptimizationStage {
    first_run,
    minimize_sheet_count,
    shrink_square,
    shrink_width_fast,
    shrink_height_fast,
    shrink_width_slow,
    shrink_height_slow,
    end
  };

  struct OptimizationState {
    const int perfect_area;
    int width;
    int height;
    OptimizationStage stage;
    int iteration;
  };

  bool advance(OptimizationStage& stage) {
    if (stage == OptimizationStage::end)
      return false;
    stage = static_cast<OptimizationStage>(static_cast<int>(stage) + 1);
    return true;
  }

  // returns true when stage should be kept, false to advance
  bool optimize_stage(OptimizationState& state,
      const Settings& pack_settings, const Run& best_run) {

    switch (state.stage) {
      case OptimizationStage::first_run:
      case OptimizationStage::end:
        return false;

      case OptimizationStage::minimize_sheet_count: {
        if (best_run.sheets.size() <= 1 ||
            state.iteration > 5)
          return false;

        const auto& last_sheet = best_run.sheets.back();
        auto area = last_sheet.width * last_sheet.height;
        for (auto i = 0; area > 0; ++i) {
          if (state.width == pack_settings.max_width &&
              state.height == pack_settings.max_height)
            break;
          if (state.height == pack_settings.max_height ||
              (state.width < pack_settings.max_width && i % 2)) {
            ++state.width;
            area -= state.height;
          }
          else {
            ++state.height;
            area -= state.width;
          }
        }
        return true;
      }

      case OptimizationStage::shrink_square: {
        if (state.width != best_run.width ||
            state.height != best_run.height ||
            state.iteration > 5)
          return false;

        const auto [width, height] = get_run_size(pack_settings, state.perfect_area);
        state.width = (state.width + width) / 2;
        state.height = (state.height + height) / 2;
        return true;
      }

      case OptimizationStage::shrink_width_fast:
      case OptimizationStage::shrink_height_fast:
      case OptimizationStage::shrink_width_slow:
      case OptimizationStage::shrink_height_slow: {
        if (state.iteration > 5)
          return false;

        const auto [width, height] = get_run_size(pack_settings, state.perfect_area);
        switch (state.stage) {
          default:
          case OptimizationStage::shrink_width_fast:
            if (state.width > width + 4)
              state.width = (state.width + width) / 2;
            break;
          case OptimizationStage::shrink_height_fast:
            if (state.height > height + 4)
              state.height = (state.height + height) / 2;
            break;
          case OptimizationStage::shrink_width_slow:
            if (state.width > width)
              --state.width;
            break;
          case OptimizationStage::shrink_height_slow:
            if (state.height > height)
              --state.height;
            break;
        }
        return true;
      }
    }
    return false;
  }

  bool optimize_run_settings(OptimizationState& state,
      const Settings& pack_settings, const Run& best_run) {

    const auto previous_state = state;
    for (;;) {
      if (!optimize_stage(state, pack_settings, best_run))
        if (advance(state.stage)) {
          state.width = best_run.width;
          state.height = best_run.height;
          state.iteration = 0;
          continue;
        }

      if (state.stage == OptimizationStage::end)
        return false;

      ++state.iteration;

      auto width = state.width;
      auto height = state.height;
      correct_size(pack_settings, width, height);
      if (width != previous_state.width ||
          height != previous_state.height) {
        state.width = width;
        state.height = height;
        return true;
      }
    }
  }

  template<typename T>
  void copy_vector(const T& source, T& dest) {
    dest.resize(source.size());
    std::copy(begin(source), end(source), begin(dest));
  }

  struct RbpState {
    rbp::MaxRectsBinPack max_rects;
    std::vector<rbp::Rect> rects;
    std::vector<rbp::RectSize> rect_sizes;
    std::vector<rbp::RectSize> run_rect_sizes;
  };

  RbpState init_rbp_state(const std::vector<Size>& sizes) {
    auto rbp = RbpState();
    rbp.rects.reserve(sizes.size());
    rbp.rect_sizes.reserve(sizes.size());
    for (const auto& size : sizes)
      rbp.rect_sizes.push_back({ size.width, size.height,
        static_cast<int>(rbp.rect_sizes.size()) });

    // to preserve order of identical rects (RBP_REVERSE_ORDER is also defined)
    std::reverse(begin(rbp.rect_sizes), end(rbp.rect_sizes));
    return rbp;
  }

  bool run_rbp_method(RbpState& rbp, const Settings& settings, Run& run,
      const std::optional<Run>& best_run, const std::vector<Size>& sizes) {
    copy_vector(rbp.rect_sizes, rbp.run_rect_sizes);
    auto cancelled = false;
    while (!rbp.run_rect_sizes.empty()) {
      rbp.rects.clear();
      rbp.max_rects.Init(run.width, run.height, settings.allow_rotate);
      rbp.max_rects.Insert(rbp.run_rect_sizes, rbp.rects, to_rbp_method(run.method));
      auto [width, height] = rbp.max_rects.BottomRight();

      correct_size(settings, width, height);
      run.total_area += width * height;

      apply_padding(settings, width, height, false);
      auto& sheet = run.sheets.emplace_back(Sheet{ width, height, { } });

      // cancel when not making any progress
      if (rbp.rects.empty())
        return false;

      // cancel when already worse than best run
      const auto done = rbp.run_rect_sizes.empty();
      if (best_run && !is_better_than(run, *best_run, !done)) {
        cancelled = true;
        break;
      }

      sheet.rects.reserve(rbp.rects.size());
      for (auto& rbp_rect : rbp.rects) {
        const auto& size = sizes[static_cast<size_t>(rbp_rect.id)];
        sheet.rects.push_back({
          size.id,
          rbp_rect.x + settings.border_padding,
          rbp_rect.y + settings.border_padding,
          rbp_rect.width,
          rbp_rect.height,
          (rbp_rect.width != size.width)
        });
      }
    }
    return !cancelled;
  }

  struct StbState {
    stbrp_context context{ };
    std::vector<stbrp_node> nodes;
    std::vector<stbrp_rect> rects;
    std::vector<stbrp_rect> run_rects;
  };

  StbState init_stb_state(const Settings& settings, const std::vector<Size>& sizes) {
    auto stb = StbState{ };
    stb.rects.reserve(sizes.size());
    stb.run_rects.reserve(sizes.size());
    for (const auto& size : sizes)
      stb.rects.push_back({ static_cast<int>(stb.rects.size()),
        size.width, size.height, 0, 0, false });

    if (settings.allow_rotate)
      for (auto& rect : stb.rects)
        if (rect.w > settings.max_width || rect.h > settings.max_height)
          std::swap(rect.w, rect.h);

    return stb;
  }

  bool run_stb_method(StbState& stb, const Settings& settings, Run& run,
      const std::optional<Run>& best_run, const std::vector<Size>& sizes) {
    copy_vector(stb.rects, stb.run_rects);
    stb.nodes.resize(std::max(stb.nodes.size(), static_cast<size_t>(run.width)));

    auto cancelled = false;
    while (!stb.run_rects.empty()) {
      stbrp_init_target(&stb.context, run.width, run.height,
        stb.nodes.data(), static_cast<int>(stb.nodes.size()));
      stbrp_setup_heuristic(&stb.context, to_stb_method(run.method));

      [[maybe_unused]] const auto all_packed =
        (stbrp_pack_rects(&stb.context, stb.run_rects.data(),
          static_cast<int>(stb.run_rects.size())) == 1);

      auto width = 0;
      auto height = 0;
      auto rects = std::vector<Rect>();
      rects.reserve(stb.run_rects.size());
      stb.run_rects.erase(std::remove_if(begin(stb.run_rects), end(stb.run_rects),
        [&](const stbrp_rect& stb_rect) {
          if (!stb_rect.was_packed)
            return false;

          width = std::max(width, stb_rect.x + stb_rect.w);
          height = std::max(height, stb_rect.y + stb_rect.h);

          const auto& size = sizes[static_cast<size_t>(stb_rect.id)];
          rects.push_back({
            size.id,
            stb_rect.x + settings.border_padding,
            stb_rect.y + settings.border_padding,
            stb_rect.w, stb_rect.h,
            (stb_rect.w != size.width)
          });
          return true;
        }), end(stb.run_rects));

      correct_size(settings, width, height);
      run.total_area += width * height;

      apply_padding(settings, width, height, false);
      const auto& sheet = run.sheets.emplace_back(Sheet{ width, height, std::move(rects) });
      const auto done = stb.run_rects.empty();
      if (sheet.rects.empty() ||
          (best_run && !is_better_than(run, *best_run, !done))) {
        cancelled = true;
        break;
      }
    }
    return !cancelled;
  }
} // namespace

std::vector<Sheet> pack(Settings settings, std::vector<Size> sizes) {
  if (!correct_settings(settings, sizes))
    return { };

  if (sizes.empty())
    return { };

  auto stb_state = std::optional<StbState>();
  if (settings.method == Method::Best ||
      settings.method == Method::Best_Skyline ||
      is_stb_method(settings.method))
    stb_state.emplace(init_stb_state(settings, sizes));

  auto rbp_state = std::optional<RbpState>();
  if (settings.method == Method::Best ||
      settings.method == Method::Best_MaxRects ||
      is_rbp_method(settings.method))
    rbp_state.emplace(init_rbp_state(sizes));

  const auto perfect_area = get_perfect_area(sizes);
  const auto target_area = perfect_area + perfect_area / 100;
  const auto [initial_width, initial_height] = get_initial_run_size(settings, perfect_area);

  auto total_best_run = std::optional<Run>{ };
  const auto methods = get_concrete_methods(settings.method);
  for (const auto& method : methods) {
    auto best_run = std::optional<Run>{ };
    auto state = OptimizationState{
      perfect_area,
      initial_width,
      initial_height,
      OptimizationStage::first_run,
      0,
    };
    for (;;) {
      if (best_run.has_value() &&
          best_run->sheets.size() == 1 &&
          best_run->total_area <= target_area)
        break;

      auto run = Run{ method, state.width, state.height, { }, 0 };
      const auto succeeded = is_rbp_method(run.method) ?
        run_rbp_method(*rbp_state, settings, run, best_run, sizes) :
        run_stb_method(*stb_state, settings, run, best_run, sizes);

      if (succeeded && (!best_run || is_better_than(run, *best_run)))
        best_run = std::move(run);

      if (!best_run.has_value() ||
          !optimize_run_settings(state, settings, *best_run))
        break;
    }
    if (best_run && (!total_best_run || is_better_than(*best_run, *total_best_run))) {
      total_best_run = std::move(best_run);
    }
  }

  if (!total_best_run)
    return { };

  if (settings.max_sheets &&
      settings.max_sheets < static_cast<int>(total_best_run->sheets.size()))
    total_best_run->sheets.resize(static_cast<size_t>(settings.max_sheets));

  return std::move(total_best_run->sheets);
}

} // namespace
