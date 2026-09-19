// Throwaway host-side check for the depth-bounds safe fallback added to
// src/graphics/host_gpu/renderer/depthRenderTarget.cpp.
//
// The functions below are the VERBATIM text of the new helpers in that file (kept as `static` and
// named identically) plus a re-implementation of the call site, so the predicate that decides
// whether a pass is culled can be exercised without a GPU, without the game dump, and without the
// 3rdparty submodules. Delete this file once the emulator tree builds normally.
//
//   clang-cl /std:c++20 /EHsc /W4 /WX depth_bounds_check.cpp
//
// A tiny harness instead of gtest: no submodule checkout is needed on this machine.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

static int g_failures = 0;

static void Check(const char* name, bool condition) {
	if (condition) {
		std::printf("[ok]   %s\n", name);
	} else {
		std::printf("[FAIL] %s\n", name);
		g_failures++;
	}
}

// ---------------------------------------------------------------------------------------------
// BEGIN verbatim copy from depthRenderTarget.cpp
// ---------------------------------------------------------------------------------------------

[[nodiscard]] static bool DepthBoundsRangeIsDegenerate(float min_bounds, float max_bounds) {
	// Written as !(...) rather than (>=) so a NaN operand is caught as well.
	return !(min_bounds < max_bounds);
}

[[nodiscard]] static bool DepthBoundsFallbackEnabled() {
	static const bool enabled = [] {
		const char* env = std::getenv("KYTY_DEPTH_BOUNDS_FALLBACK");
		return env == nullptr || env[0] != '0';
	}();
	return enabled;
}

[[nodiscard]] static bool DepthBoundsLogEnabled() {
	static const bool enabled = [] {
		const char* env = std::getenv("KYTY_DEPTH_BOUNDS_LOG");
		return env == nullptr || env[0] != '0';
	}();
	return enabled;
}

// END verbatim copy
// ---------------------------------------------------------------------------------------------

struct DepthState {
	bool  bounds_enable = false;
	float min_bounds    = 0.0f;
	float max_bounds    = 0.0f;
};

// Re-implementation of the call site inserted into ResolveRenderDepthTarget, minus the logging.
static void ApplyDepthBoundsFallback(DepthState& r) {
	if (r.bounds_enable && DepthBoundsRangeIsDegenerate(r.min_bounds, r.max_bounds)) {
		const bool fallback = DepthBoundsFallbackEnabled();
		if (fallback) {
			r.bounds_enable = false;
		}
	}
}

static void TestPredicate() {
	struct Case {
		float min_bounds;
		float max_bounds;
		bool  degenerate;
	};
	const auto nan = std::numeric_limits<float>::quiet_NaN();

	// The guest's own defaults are min=0, max=1 (hardwareContext.h) - a proper interval.
	const std::vector<Case> cases {
	    {0.0f, 1.0f, false},
	    {0.5f, 0.75f, false},
	    {1.0f, 1.1f, false},
	    {-1.0f, 2.0f, false},
	    {1.0f, 0.0f, true},   // inverted: undefined in Vulkan, culls on AMD
	    {0.0f, 1.0f, false},
	    {0.0f, 0.0f, true},   // min == max: culls every fragment
	    {1.0f, 1.0f, true},
	    {1.0f, 0.0f, true},
	    // A never-programmed register pair reads as (0, 0): max is initialised to 1.0f, but a
	    // guest that writes only DB_DEPTH_BOUNDS_MIN to 0 leaves the pair untouched, so the
	    // interesting reset-like shapes are the two above. NaN is the "garbage register" case.
	    {nan, 1.0f, true},
	    {0.0f, nan, true},
	    {nan, nan, true},
	};

	for (const auto& item: cases) {
		const auto name = std::string("degenerate(") + std::to_string(item.min_bounds) + ", " +
		                  std::to_string(item.max_bounds) + ") == " +
		                  (item.degenerate ? "true" : "false");
		Check(name.c_str(),
		      DepthBoundsRangeIsDegenerate(item.min_bounds, item.max_bounds) == item.degenerate);
	}
}

static void TestCallSite() {
	// A proper range must be left completely alone.
	auto good = DepthState {true, 0.0f, 1.0f};
	ApplyDepthBoundsFallback(good);
	Check("proper range keeps the bounds test enabled",
	      good.bounds_enable && good.min_bounds == 0.0f && good.max_bounds == 1.0f);

	// Bounds disabled: the predicate must not even be consulted, and the values stay untouched.
	auto disabled = DepthState {false, 1.0f, 0.0f};
	ApplyDepthBoundsFallback(disabled);
	Check("disabled bounds stays disabled and untouched",
	      !disabled.bounds_enable && disabled.min_bounds == 1.0f && disabled.max_bounds == 0.0f);

	// The regression: a degenerate range with the fallback ON must not cull the pass.
	std::system("set KYTY_DEPTH_BOUNDS_FALLBACK=1");
	auto degenerate = DepthState {true, 1.0f, 0.0f};
	ApplyDepthBoundsFallback(degenerate);
	Check("degenerate range with fallback on no longer culls the pass", !degenerate.bounds_enable);

	auto equal_range = DepthState {true, 0.25f, 0.25f};
	ApplyDepthBoundsFallback(equal_range);
	Check("min == max with fallback on no longer culls the pass", !equal_range.bounds_enable);

	auto nan_range = DepthState {true, std::numeric_limits<float>::quiet_NaN(), 1.0f};
	ApplyDepthBoundsFallback(nan_range);
	Check("NaN range with fallback on no longer culls the pass", !nan_range.bounds_enable);
}

int main() {
	Check("logging is on unless explicitly disabled", DepthBoundsLogEnabled());
	TestPredicate();
	TestCallSite();

	if (g_failures != 0) {
		std::printf("\n%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("\nall checks passed\n");
	return 0;
}