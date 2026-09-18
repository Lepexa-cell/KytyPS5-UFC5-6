#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_DEPTHBOUNDSSTATE_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_DEPTHBOUNDSSTATE_H_

#include <algorithm>
#include <cmath>

namespace Libs::Graphics {

// Host depth-bounds (depth-culling) state translated from the guest depth registers.
struct DepthBoundsState {
	bool  test_enable = false;
	float min_bounds  = 0.0f;
	float max_bounds  = 1.0f;
};

// The PS5 exposes DB_DEPTH_BOUNDS_MIN/MAX together with DB_DEPTH_CONTROL.DEPTH_BOUNDS_ENABLE, and
// titles use that pair for depth-culling passes such as contact shadows. Vulkan has no meaningful
// result for a reversed, non-finite, or empty range: the driver rejects every fragment of the pass,
// which shows up as the whole scene turning black while a separate HUD pass keeps drawing. Only a
// non-empty range inside [0,1] is forwarded; anything else is neutralized, because a stray bounds
// register may only ever make the frame darker and never safer.
[[nodiscard]] inline DepthBoundsState SanitizeDepthBounds(bool enable, float min_bounds,
                                                          float max_bounds, bool force_disable) {
	DepthBoundsState state {};
	if (!enable || force_disable) {
		return state;
	}
	if (!std::isfinite(min_bounds) || !std::isfinite(max_bounds)) {
		return state;
	}
	// Reverse-Z titles encode a near range the other way around.
	if (min_bounds > max_bounds) {
		const float swapped = min_bounds;
		min_bounds          = max_bounds;
		max_bounds          = swapped;
	}
	const auto clamped_min = std::clamp(min_bounds, 0.0f, 1.0f);
	const auto clamped_max = std::clamp(max_bounds, 0.0f, 1.0f);
	if (!(clamped_max > clamped_min)) {
		return state;
	}
	state.test_enable = true;
	state.min_bounds  = clamped_min;
	state.max_bounds  = clamped_max;
	return state;
}

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_DEPTHBOUNDSSTATE_H_
