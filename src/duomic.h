#pragma once
#include "duomic_protocol.h"
#include <memory>
#include <string_view>

namespace duomic {
  bool available();

  // Remove this session's own microphone from loopback audio sent back to the client.
  void suppress_monitor(float *interleaved, int frames, int channels);

  class microphone {
  public:
    microphone();
    ~microphone();
    microphone(const microphone &) = delete;
    microphone &operator=(const microphone &) = delete;
    void submit(std::string_view packet);
  private:
    struct impl;
    std::unique_ptr<impl> state;
  };
}
