#include "rpmsg_port.hpp"

#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include "check.hpp"
#include "config.hpp"

RpmsgPort::RpmsgPort(const char * path)
: fd_(-1), open_errno_(0)
{
  // check input
  if (!check(path != nullptr, "RpmsgPort: path is null")) {
    open_errno_ = EINVAL;
    return;
  }

  // open the channel
  fd_ = open(path, O_RDWR | O_NOCTTY);
  if (fd_ < 0) {
    open_errno_ = errno;
    return;
  }

  // set raw mode; if that fails, close the channel again
  if (configureRawMode() != Status::OK) {
    open_errno_ = errno;
    (void)close(fd_);   // ignore the result: we are giving up on this channel anyway
    fd_ = -1;
  }
}

RpmsgPort::~RpmsgPort()
{
  // close the channel; a destructor cannot return a Status, so print any failure
  if (fd_ >= 0 && close(fd_) != 0) {
    (void)std::fprintf(stderr, "RpmsgPort: close() failed, errno %d\n", errno);
  }
}

bool RpmsgPort::isOpen() const
{
  return fd_ >= 0;
}

int RpmsgPort::openErrno() const
{
  return open_errno_;
}

Status RpmsgPort::configureRawMode()
{
  // check the channel is open
  if (!check(fd_ >= 0, "RpmsgPort::configureRawMode: channel not open")) {
    return Status::NOT_OPEN;
  }

  // read the settings, switch them to raw, write them back
  termios settings{};
  if (tcgetattr(fd_, &settings) != 0) {
    return Status::IO_ERROR;
  }
  cfmakeraw(&settings);
  if (tcsetattr(fd_, TCSANOW, &settings) != 0) {
    return Status::IO_ERROR;
  }
  return Status::OK;
}

Status RpmsgPort::send(const uint8_t * data, size_t length)
{
  // check inputs
  if (!check(data != nullptr, "RpmsgPort::send: data is null")) {
    return Status::INVALID_ARGUMENT;
  }
  if (!check(length > 0 && length <= config::kMaxMessageLength,
        "RpmsgPort::send: length out of range")) {
    return Status::INVALID_ARGUMENT;
  }
  if (fd_ < 0) {
    return Status::NOT_OPEN;
  }

  // write until all bytes are sent, at most kMaxWriteAttempts times
  size_t sent = 0;
  for (int attempt = 0; attempt < config::kMaxWriteAttempts && sent < length; attempt++) {
    ssize_t written = write(fd_, data + sent, length - sent);
    if (written < 0) {
      if (errno == EINTR || errno == EAGAIN) {
        continue;   // interrupted or busy: try again
      }
      return Status::IO_ERROR;
    }
    sent += static_cast<size_t>(written);
  }

  return (sent == length) ? Status::OK : Status::IO_ERROR;
}

Status RpmsgPort::receive(uint8_t * buffer, size_t capacity, size_t & received,
  uint32_t timeout_ms)
{
  // check inputs
  received = 0;
  if (!check(buffer != nullptr && capacity > 0, "RpmsgPort::receive: no room for data")) {
    return Status::INVALID_ARGUMENT;
  }
  if (!check(timeout_ms <= config::kMaxTimeoutMs, "RpmsgPort::receive: timeout too long")) {
    return Status::INVALID_ARGUMENT;
  }
  if (fd_ < 0) {
    return Status::NOT_OPEN;
  }

  // wait until there is data, at most timeout_ms
  pollfd watch{};
  watch.fd = fd_;
  watch.events = POLLIN;
  int ready = poll(&watch, 1, static_cast<int>(timeout_ms));
  if (ready < 0) {
    return Status::IO_ERROR;
  }
  if (ready == 0) {
    return Status::TIMEOUT;
  }
  if ((watch.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
    return Status::IO_ERROR;
  }

  // read what arrived
  ssize_t count = read(fd_, buffer, capacity);
  if (count <= 0) {
    return Status::IO_ERROR;   // 0 = channel closed, below 0 = error
  }
  received = static_cast<size_t>(count);
  return Status::OK;
}
