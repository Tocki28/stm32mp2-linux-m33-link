#ifndef LINK_LOOPBACK_TRANSPORT_HPP
#define LINK_LOOPBACK_TRANSPORT_HPP

// Guard 1: this file is test-only. Only the test build defines LINK_TEST_BUILD
// (see the Makefile). If this file reaches the flight build, the compiler stops here.
#ifndef LINK_TEST_BUILD
#error "loopback_transport.hpp is test-only: it must never be in the flight program"
#endif

#include <array>

#include "config.hpp"
#include "transport.hpp"

/**
 * @brief Faults the fake channel can inject, so the tests can reach the error paths.
 *
 * @note All off by default: the fake then gives back every byte, at once, unchanged.
 */
struct LoopbackFaults
{
  size_t piece_size;      // 0: receive() gives all waiting bytes. N: at most N bytes per
                          //   receive(), so one echo arrives in several pieces
  bool flip_first_byte;   // send() flips the lowest bit of each message's first byte,
                          //   so the echo comes back different
  bool drop_last_byte;    // send() does not keep each message's last byte,
                          //   so the echo arrives incomplete
  bool repeat_last_byte;  // send() keeps each message's last byte twice,
                          //   so one extra byte stays in the channel
};

/**
 * @brief A fake channel for tests. Every byte sent comes back on the next receive(),
 *        like the M33 echo firmware, unless a fault is switched on.
 *
 * @note Test only. Never part of the flight program.
 * @note Its buffer has a fixed size, so it never allocates memory (NASA rule 3).
 */
class LoopbackTransport : public Transport
{
public:
  /**
   * @brief Creates an empty fake channel with no faults.
   */
  LoopbackTransport();

  /**
   * @brief Creates an empty fake channel that injects the given faults.
   *
   * @param faults Which faults to inject.
   */
  explicit LoopbackTransport(const LoopbackFaults & faults);

  // see Transport for send() and receive()
  Status send(const uint8_t * data, size_t length) override;
  Status receive(uint8_t * buffer, size_t capacity, size_t & received,
    uint32_t timeout_ms) override;

private:
  LoopbackFaults faults_;   // which faults to inject

  // ring buffer: waiting bytes start at head_ and wrap around to index 0
  std::array<uint8_t, config::kLoopbackCapacity> storage_;
  size_t head_;    // index of the oldest waiting byte
  size_t count_;   // how many bytes are waiting
};

#endif  // LINK_LOOPBACK_TRANSPORT_HPP
