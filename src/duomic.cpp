// JimothySnicket/moonlight-mic v0.1.0 transport -> the current Duo session.
// Authentication/decryption remains in Sunshine's existing control stream.
#include "duomic.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <opus/opus.h>

namespace duomic {
  namespace {
    constexpr int reference_len = 48000;
    std::mutex reference_lock;
    std::vector<float> reference(reference_len);
    int reference_pos = 0;
    int reference_count = 0;

    void record_reference(const std::int16_t *pcm, int count) {
      std::lock_guard<std::mutex> lock(reference_lock);
      for (int i = 0; i < count; ++i) {
        reference[reference_pos] = static_cast<float>(pcm[i]) * (1.f / 32768.f);
        reference_pos = (reference_pos + 1) % reference_len;
        if (reference_count < reference_len) ++reference_count;
      }
    }

    float reference_at(const std::vector<float> &samples, int pos, int ago) {
      auto index = pos - 1 - ago;
      index %= reference_len;
      if (index < 0) index += reference_len;
      return samples[index];
    }
  }

  void suppress_monitor(float *interleaved, int frames, int channels) {
    constexpr int window = 960;
    if (!interleaved || frames < window || channels < 1) return;

    std::vector<float> samples;
    int pos = 0;
    int count = 0;
    {
      std::lock_guard<std::mutex> lock(reference_lock);
      if (reference_count < window + 4800) return;
      samples = reference;
      pos = reference_pos;
      count = reference_count;
    }

    const int max_delay = std::min(count - window, 24000);
    const int start = frames - window;
    int best_delay = -1;
    double best_corr = 0;
    double best_gain = 0;
    for (int delay = 0; delay <= max_delay; delay += 48) {
      double dot = 0, mic_energy = 0, play_energy = 0;
      for (int i = 0; i < window; ++i) {
        const int idx = start + i;
        float mono = 0;
        for (int c = 0; c < channels; ++c) mono += interleaved[idx * channels + c];
        mono /= static_cast<float>(channels);
        const float mic = reference_at(samples, pos, delay + (frames - 1 - idx));
        dot += mono * mic;
        mic_energy += mic * mic;
        play_energy += mono * mono;
      }
      if (mic_energy < 0.2 || play_energy < 1e-6) continue;
      const double corr = dot / std::sqrt(mic_energy * play_energy);
      if (corr > best_corr) {
        best_corr = corr;
        best_gain = dot / mic_energy;
        best_delay = delay;
      }
    }
    if (best_delay < 0 || best_corr < 0.45 || best_gain < 0.05 || best_gain > 4.0) return;

    for (int step = std::max(0, best_delay - 48); step <= std::min(max_delay, best_delay + 48); ++step) {
      double dot = 0, mic_energy = 0, play_energy = 0;
      for (int i = 0; i < window; ++i) {
        const int idx = start + i;
        float mono = 0;
        for (int c = 0; c < channels; ++c) mono += interleaved[idx * channels + c];
        mono /= static_cast<float>(channels);
        const float mic = reference_at(samples, pos, step + (frames - 1 - idx));
        dot += mono * mic;
        mic_energy += mic * mic;
        play_energy += mono * mono;
      }
      if (mic_energy < 0.2 || play_energy < 1e-6) continue;
      const double corr = dot / std::sqrt(mic_energy * play_energy);
      if (corr > best_corr) {
        best_corr = corr;
        best_gain = dot / mic_energy;
        best_delay = step;
      }
    }
    if (best_corr < 0.45 || best_gain < 0.05 || best_gain > 4.0) return;

    for (int idx = 0; idx < frames; ++idx) {
      const float mic = reference_at(samples, pos, best_delay + (frames - 1 - idx));
      for (int c = 0; c < channels; ++c) {
        interleaved[idx * channels + c] -= static_cast<float>(best_gain) * mic;
      }
    }
  }

  constexpr std::uint32_t magic = 0x33434d44; // DMC3, little endian
  constexpr std::size_t slot_count = 16;

  struct alignas(8) header {
    std::uint32_t magic_value, version, session_id, slots;
    volatile LONG sequence, writer_pid;
    volatile LONG64 heartbeat;
    unsigned char reserved[32];
  };
  struct alignas(8) slot {
    volatile LONG stamp;
    std::uint32_t frames;
    std::uint64_t written_at;
    std::int16_t pcm[frame_samples];
  };
  struct ring { header h; slot slots[slot_count]; };
  static_assert(sizeof(header) == 64);
  static_assert(sizeof(slot) == 1936);
  static_assert(sizeof(ring) == 31040);

  class mapping {
  public:
    HANDLE handle = nullptr;
    ring *data = nullptr;
    DWORD session_id = 0;
    ~mapping() { close(); }
    void close() {
      if (data) UnmapViewOfFile(data);
      if (handle) CloseHandle(handle);
      data = nullptr; handle = nullptr;
    }
    bool fresh() const {
      if (!data || data->h.magic_value != magic || data->h.version != 3 ||
          data->h.session_id != session_id || data->h.slots != slot_count) return false;
      const auto heartbeat = std::uint64_t(InterlockedCompareExchange64(&data->h.heartbeat, 0, 0));
      const auto now = GetTickCount64();
      return heartbeat <= now && now - heartbeat < 1500;
    }
    bool open() {
      if (fresh()) return true;
      close();
      if (!ProcessIdToSessionId(GetCurrentProcessId(), &session_id) || session_id == 0) return false;
      const auto name = L"Global\\DuoMic.Sunshine." + std::to_wstring(session_id);
      handle = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, name.c_str());
      if (!handle) return false;
      data = static_cast<ring *>(MapViewOfFile(handle, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(ring)));
      if (!fresh()) { close(); return false; }
      return true;
    }
  };

  bool available() { mapping view; return view.open(); }
  static std::atomic<const void *> active_owner {nullptr};

  struct microphone::impl {
    mapping view;
    OpusDecoder *decoder = nullptr;
    bool have_sequence = false;
    std::uint16_t last_sequence = 0;
    double tokens = 8;
    std::uint64_t budget_time = GetTickCount64();
    std::array<opus_int16, frame_samples> samples {};

    ~impl() {
      const void *owner = this;
      if (active_owner.compare_exchange_strong(owner, nullptr) && view.data) {
        InterlockedCompareExchange(&view.data->h.writer_pid, 0, LONG(GetCurrentProcessId()));
      }
      if (decoder) opus_decoder_destroy(decoder);
    }

    void submit(std::string_view payload) {
      if (!view.open()) return;
      auto &h = view.data->h;
      packet_view packet {};
      if (!parse({reinterpret_cast<const unsigned char *>(payload.data()), payload.size()}, packet) ||
          (have_sequence && !newer(packet.sequence, last_sequence))) {
        return;
      }
      const auto now = GetTickCount64();
      tokens = (std::min)(8.0, tokens + double(now - budget_time) * .05);
      budget_time = now;
      if (tokens < 1) { return; }
      tokens -= 1;
      const void *owner = nullptr;
      if (!active_owner.compare_exchange_strong(owner, this) && owner != this) return;
      const auto pid = LONG(GetCurrentProcessId());
      const auto writer = InterlockedCompareExchange(&h.writer_pid, pid, 0);
      if (writer != 0 && writer != pid) return;
      if (!decoder) {
        int error = 0;
        decoder = opus_decoder_create(48000, 1, &error);
        if (!decoder || error != OPUS_OK) return;
      }
      const auto decoded = opus_decode(decoder, packet.opus.data(), int(packet.opus.size()), samples.data(), frame_samples, 0);
      if (decoded != frame_samples) { return; }
      have_sequence = true; last_sequence = packet.sequence;
      const auto sequence = std::uint32_t(InterlockedCompareExchange(&h.sequence, 0, 0)) + 1;
      auto &target = view.data->slots[sequence % slot_count];
      InterlockedExchange(&target.stamp, LONG(sequence * 2 + 1));
      target.frames = frame_samples;
      target.written_at = now;
      std::memcpy(target.pcm, samples.data(), sizeof(target.pcm));
      record_reference(samples.data(), frame_samples);
      MemoryBarrier();
      InterlockedExchange(&target.stamp, LONG(sequence * 2 + 2));
      InterlockedExchange(&h.sequence, LONG(sequence));
    }
  };
  microphone::microphone() : state(std::make_unique<impl>()) {}
  microphone::~microphone() = default;
  void microphone::submit(std::string_view payload) { state->submit(payload); }
}
#else
namespace duomic {
  struct microphone::impl {};
  bool available() { return false; }
  microphone::microphone() : state(std::make_unique<impl>()) {}
  microphone::~microphone() = default;
  void microphone::submit(std::string_view) {}
  void suppress_monitor(float *, int, int) {}
}
#endif
