#ifndef LINK_RPMSG_PORT_HPP
#define LINK_RPMSG_PORT_HPP

#include "transport.hpp"

/**
 * @brief The real channel to the M33. Linux shows it as a file: /dev/ttyRPMSG0.
 *
 * The constructor opens the channel. The destructor closes it.
 */
class RpmsgPort : public Transport
{
public:
  /**
   * @brief Opens the channel and sets it to raw mode.
   *
   * @param path The channel's file, for example /dev/ttyRPMSG0.
   * @note A constructor cannot return a Status. Call isOpen() after it.
   */
  explicit RpmsgPort(const char * path);

  /**
   * @brief Closes the channel.
   */
  ~RpmsgPort() override;

  /**
   * @brief Tells if the constructor opened the channel.
   *
   * @return true if open, false if not.
   */
  bool isOpen() const;

  /**
   * @brief Tells why opening failed.
   *
   * @return The errno from opening, or 0 if it worked.
   */
  int openErrno() const;

  // see Transport for send() and receive()
  Status send(const uint8_t * data, size_t length) override;
  Status receive(uint8_t * buffer, size_t capacity, size_t & received,
    uint32_t timeout_ms) override;

private:
  /**
   * @brief Sets the channel to raw mode: bytes pass through at once, unchanged.
   *
   * @return OK, NOT_OPEN or IO_ERROR.
   * @note Without it, Linux holds bytes until it sees a newline (found 8 Oct 2026).
   */
  Status configureRawMode();

  int fd_;           // the channel's file descriptor; -1 when not open
  int open_errno_;   // why opening failed; 0 when it worked
};

#endif  // LINK_RPMSG_PORT_HPP
