#ifndef LINK_TRANSPORT_HPP
#define LINK_TRANSPORT_HPP

#include <cstddef>
#include <cstdint>

#include "status.hpp"

/**
 * @brief Any channel that carries bytes to the M33 and back.
 *
 * The link code only uses this class, so the channel under it can change
 * (real rpmsg, fake, serial) without changing the link code.
 *
 * @note NASA rule 9 says no function pointers. Virtual functions use a hidden table
 *       of function pointers. Accepted: there are only two possible targets
 *       (RpmsgPort, LoopbackTransport), and a channel is never swapped once created.
 */
class Transport
{
public:
  Transport() = default;
  virtual ~Transport() = default;

  // a channel cannot be copied: two objects would own the same channel
  Transport(const Transport &) = delete;
  Transport & operator=(const Transport &) = delete;

  /**
   * @brief Sends bytes.
   *
   * @param data The bytes to send.
   * @param length How many bytes.
   * @return OK if all bytes were sent, otherwise why not.
   */
  virtual Status send(const uint8_t * data, size_t length) = 0;

  /**
   * @brief Waits for bytes and copies them into buffer.
   *
   * @param buffer Where to copy the bytes.
   * @param capacity Size of buffer, in bytes.
   * @param received Set to how many bytes were copied.
   * @param timeout_ms Longest wait, in ms.
   * @return OK, TIMEOUT if nothing arrived in time, or the error.
   */
  virtual Status receive(uint8_t * buffer, size_t capacity, size_t & received,
    uint32_t timeout_ms) = 0;
};

#endif  // LINK_TRANSPORT_HPP
