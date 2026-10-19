/**
 * @file src/platform/macos/input.cpp
 * @brief Definitions for macOS input handling.
 */

// platform includes
#include <ApplicationServices/ApplicationServices.h>

// standard includes
#include <cstdint>
#include <optional>
#include <vector>

// local includes
#include "src/platform/common.h"

namespace platf {

  std::optional<util::point_t> get_mouse_loc(input_t & /*input*/) {
    const auto event = CGEventCreate(nullptr);
    if (!event) {
      return std::nullopt;
    }

    const auto current = CGEventGetLocation(event);
    CFRelease(event);
    return util::point_t {current.x, current.y};
  }

  platform_caps::caps_t get_capabilities() {
    // Virtual gamepads, pen and touch on macOS were provided by the Virtual HID Broker backend,
    // which this build does not ship. Keyboard and mouse injection still use CoreGraphics events,
    // so only the device-backed capabilities are absent.
    return 0;
  }

  std::vector<supported_gamepad_t> &supported_gamepads(input_t * /*input*/) {
    static std::vector<supported_gamepad_t> gamepads;
    gamepads.clear();
    return gamepads;
  }

}  // namespace platf
