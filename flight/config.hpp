#ifndef LINK_CONFIG_HPP
#define LINK_CONFIG_HPP

#include <cstddef>
#include <cstdint>

/**
 * @brief All settings of the link test, each with the reason for its value.
 *
 * @note No other file may contain a number that could change.
 */
namespace config
{

/**
 * @brief The channel to the M33.
 *
 * @note The M33 echo firmware creates this channel first, so Linux names it ttyRPMSG0.
 */
constexpr const char * kDefaultDevicePath = "/dev/ttyRPMSG0";

/**
 * @brief How long to wait for each echo, in ms. After that it counts as lost.
 *
 * @note TBD: set from measured round-trip times (Mon 12 Oct 2026).
 */
constexpr uint32_t kDefaultTimeoutMs = 100;

/**
 * @brief Longest wait allowed, in ms.
 *
 * @note A link that needs more than 10 s is dead, not slow.
 */
constexpr uint32_t kMaxTimeoutMs = 10000;

/**
 * @brief Largest message, in bytes.
 *
 * @note An rpmsg buffer is 512 bytes with a 16-byte header, so at most 496 bytes fit
 *       (Linux drivers/rpmsg/virtio_rpmsg_bus.c). 256 leaves a margin.
 */
constexpr size_t kMaxMessageLength = 256;

/**
 * @brief The echo test sends messages of 1, 2, ... up to this many bytes.
 */
constexpr size_t kDefaultTestMaxLength = 64;

/**
 * @brief Size of the fake channel's buffer: room for 4 full messages.
 */
constexpr size_t kLoopbackCapacity = 4 * kMaxMessageLength;

/**
 * @brief How many times send() calls write() before it gives up.
 *
 * @note write() can send only part of the data, or be interrupted.
 * @note NASA rule 2: every loop has a fixed limit.
 * @note TBD: 8 was chosen by hand, not measured.
 */
constexpr int kMaxWriteAttempts = 8;

/**
 * @brief How many reads are allowed to collect one echo.
 *
 * @note An echo can arrive in several pieces.
 * @note TBD: 8 was chosen by hand, not measured.
 */
constexpr int kMaxReadsPerMessage = 8;

}  // namespace config

#endif  // LINK_CONFIG_HPP
