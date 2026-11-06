#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

namespace duomic {
  inline constexpr std::uint16_t packet_type = 0x5510;
  inline constexpr std::uint32_t host_feature = 0x100;
  inline constexpr std::uint32_t client_feature = 0x04;
  inline constexpr int frame_samples = 960;
  inline constexpr std::size_t max_opus_bytes = 1500;

  struct packet_view {
    std::uint16_t sequence;
    std::uint32_t timestamp;
    std::span<const unsigned char> opus;
  };

  inline bool allowed(std::uint32_t features, int protocol, bool decrypted) {
    return (features & client_feature) && protocol == 13 && decrypted;
  }

  inline bool parse(std::span<const unsigned char> bytes, packet_view &result) {
    if (bytes.size() < 9 || bytes.size() > 8 + max_opus_bytes) return false;
    const auto size = (std::size_t(bytes[2]) << 8) | bytes[3];
    if (size == 0 || size > max_opus_bytes || size + 8 != bytes.size()) return false;
    result.sequence = (std::uint16_t(bytes[0]) << 8) | bytes[1];
    result.timestamp = (std::uint32_t(bytes[4]) << 24) | (std::uint32_t(bytes[5]) << 16) |
                       (std::uint32_t(bytes[6]) << 8) | bytes[7];
    result.opus = bytes.subspan(8);
    return true;
  }

  inline bool newer(std::uint16_t incoming, std::uint16_t previous) {
    const auto distance = std::uint16_t(incoming - previous);
    return distance != 0 && distance < 0x8000;
  }
}
