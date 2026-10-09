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
 * @brief A fake channel for tests. Every byte sent comes back on the next receive(),
 *        like the M33 echo firmware.
 *
 * @note Test only. Never part of the flight program.
 * @note Its buffer has a fixed size, so it never allocates memory (NASA rule 3).
 */
class LoopbackTransport : public Transport
{
public:
  /**
   * @brief Creates an empty fake channel.
   */
  LoopbackTransport();

  // see Transport for send() and receive()
  Status send(const uint8_t * data, size_t length) override;
  Status receive(uint8_t * buffer, size_t capacity, size_t & received,
    uint32_t timeout_ms) override;

private:
  // ring buffer: waiting bytes start at head_ and wrap around to index 0
  std::array<uint8_t, config::kLoopbackCapacity> storage_;
  size_t head_;    // index of the oldest waiting byte
  size_t count_;   // how many bytes are waiting
};

#endif  // LINK_LOOPBACK_TRANSPORT_HPP
