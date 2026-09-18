#include "graphics/host_gpu/renderer/depthBoundsState.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {

using namespace Libs::Graphics;

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "DepthBoundsFallbackTests: failed: %s\n", text);
		std::abort();
	}
}

void CheckDisabled(const DepthBoundsState& state, const char* text) {
	Check(!state.test_enable, text);
	// Disabled bounds still carry the canonical full range so the pipeline key stays stable.
	Check(state.min_bounds == 0.0f && state.max_bounds == 1.0f,
	      "neutralized depth bounds did not use the canonical full range");
}

void CheckRange(const DepthBoundsState& state, float min_bounds, float max_bounds,
                const char* text) {
	Check(state.test_enable && state.min_bounds == min_bounds && state.max_bounds == max_bounds,
	      text);
}

void TestDisabledAndForcedRequests() {
	CheckDisabled(SanitizeDepthBounds(false, 0.25f, 0.75f, false),
	              "a disabled depth bounds request enabled the host test");
	CheckDisabled(SanitizeDepthBounds(true, 0.25f, 0.75f, true),
	              "the forced fallback did not disable an otherwise usable depth bounds range");
}

void TestUsableRangeIsForwarded() {
	CheckRange(SanitizeDepthBounds(true, 0.0f, 1.0f, false), 0.0f, 1.0f,
	           "a full depth bounds range was not forwarded");
	// Reverse-Z near range, the shape contact-shadow and depth-culling passes use.
	CheckRange(SanitizeDepthBounds(true, 0.9f, 1.0f, false), 0.9f, 1.0f,
	           "a usable near depth bounds range was not forwarded");
	CheckRange(SanitizeDepthBounds(true, 0.25f, 0.5f, false), 0.25f, 0.5f,
	           "a usable depth bounds range was not forwarded");
}

void TestReversedRangeIsReordered() {
	CheckRange(SanitizeDepthBounds(true, 0.9f, 0.1f, false), 0.1f, 0.9f,
	           "a reversed depth bounds range was not reordered");
	CheckRange(SanitizeDepthBounds(true, 1.0f, 0.0f, false), 0.0f, 1.0f,
	           "a fully reversed depth bounds range was not reordered");
}

void TestEmptyRangeIsNeutralized() {
	CheckDisabled(SanitizeDepthBounds(true, 0.0f, 0.0f, false),
	              "an empty depth bounds range was forwarded and would cull every fragment");
	CheckDisabled(SanitizeDepthBounds(true, 0.5f, 0.5f, false),
	              "a degenerate depth bounds range was forwarded and would cull every fragment");
	CheckDisabled(SanitizeDepthBounds(true, 1.5f, 2.0f, false),
	              "an out-of-range depth bounds range collapsed outside [0,1] without a fallback");
}

void TestOutOfRangeRangeIsClamped() {
	CheckRange(SanitizeDepthBounds(true, -1.0f, 2.0f, false), 0.0f, 1.0f,
	           "an oversized depth bounds range was not clamped to [0,1]");
	CheckRange(SanitizeDepthBounds(true, -0.5f, 0.25f, false), 0.0f, 0.25f,
	           "a negative depth bounds minimum was not clamped to [0,1]");
	CheckRange(SanitizeDepthBounds(true, 0.75f, 1.5f, false), 0.75f, 1.0f,
	           "an oversized depth bounds maximum was not clamped to [0,1]");
}

void TestNonFiniteRangeIsNeutralized() {
	constexpr float nan       = std::numeric_limits<float>::quiet_NaN();
	constexpr float infinity  = std::numeric_limits<float>::infinity();
	CheckDisabled(SanitizeDepthBounds(true, nan, 1.0f, false),
	              "a NaN depth bounds minimum was forwarded to the host");
	CheckDisabled(SanitizeDepthBounds(true, 0.0f, nan, false),
	              "a NaN depth bounds maximum was forwarded to the host");
	CheckDisabled(SanitizeDepthBounds(true, -infinity, infinity, false),
	              "infinite depth bounds were forwarded to the host");
	CheckDisabled(SanitizeDepthBounds(true, infinity, 0.0f, false),
	              "an infinite reversed depth bounds range was forwarded to the host");
}

} // namespace

int main() {
	TestDisabledAndForcedRequests();
	TestUsableRangeIsForwarded();
	TestReversedRangeIsReordered();
	TestEmptyRangeIsNeutralized();
	TestOutOfRangeRangeIsClamped();
	TestNonFiniteRangeIsNeutralized();
	std::puts("DepthBoundsFallbackTests: all cases passed");
	return 0;
}
