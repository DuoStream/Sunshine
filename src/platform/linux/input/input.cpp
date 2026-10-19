/**
 * @file src/platform/linux/input/input.cpp
 * @brief Definitions for Unix input handling.
 */

// standard includes
#include <optional>
#include <vector>

// platform includes
#ifdef SUNSHINE_BUILD_X11
  #include <X11/Xlib.h>
#endif

// local includes
#include "src/platform/common.h"

using namespace std::literals;

namespace platf {
  platform_caps::caps_t get_capabilities() {
    // Virtual gamepads, pen and touch on Unix were provided by the Virtual HID Broker backend,
    // which this build does not ship. Keyboard and mouse injection still use the platform's own
    // event paths, so only the device-backed capabilities are absent.
    return 0;
  }

  std::optional<util::point_t> get_mouse_loc(input_t & /*input*/) {
#ifdef SUNSHINE_BUILD_X11
    auto *display = XOpenDisplay(nullptr);
    if (!display) {
      return std::nullopt;
    }

    const auto root = DefaultRootWindow(display);
    Window root_return {};
    Window child_return {};
    int root_x = 0;
    int root_y = 0;
    int window_x = 0;
    int window_y = 0;
    unsigned int mask = 0;
    const auto queried = XQueryPointer(display, root, &root_return, &child_return, &root_x, &root_y, &window_x, &window_y, &mask);
    XCloseDisplay(display);

    if (!queried) {
      return std::nullopt;
    }

    return util::point_t {
      static_cast<double>(root_x),
      static_cast<double>(root_y)
    };
#else
    return std::nullopt;
#endif
  }

  std::vector<supported_gamepad_t> &supported_gamepads(input_t * /*input*/) {
    static std::vector<supported_gamepad_t> gamepads;
    gamepads.clear();
    return gamepads;
  }
}  // namespace platf
