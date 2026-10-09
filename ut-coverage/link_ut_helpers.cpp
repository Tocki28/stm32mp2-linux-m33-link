#include "link_ut_helpers.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

void report(const char * name, bool passed, int & failures)
{
  (void)std::printf("%s  %s\n", passed ? "PASS" : "FAIL", name);
  if (!passed) {
    failures++;
  }
}

PseudoTerminal::PseudoTerminal()
: near_fd_(-1), far_path_{}
{
  // create the pair and open the near end
  int fd = posix_openpt(O_RDWR | O_NOCTTY);
  if (fd < 0) {
    return;
  }

  // allow the far end to be opened
  if (grantpt(fd) != 0 || unlockpt(fd) != 0) {
    (void)close(fd);   // ignore the result: we are giving up on this pair anyway
    return;
  }

  // ptsname() returns a buffer it reuses, so copy the far end's name now
  const char * name = ptsname(fd);
  if (name == nullptr || std::strlen(name) >= far_path_.size()) {
    (void)close(fd);
    return;
  }
  (void)std::snprintf(far_path_.data(), far_path_.size(), "%s", name);
  near_fd_ = fd;
}

PseudoTerminal::~PseudoTerminal()
{
  hangUp();
}

bool PseudoTerminal::isOpen() const
{
  return near_fd_ >= 0;
}

const char * PseudoTerminal::farEndPath() const
{
  return far_path_.data();
}

bool PseudoTerminal::writeBytes(const uint8_t * data, size_t length)
{
  // check inputs
  if (data == nullptr || near_fd_ < 0) {
    return false;
  }

  // one write: the test messages are small enough to go in at once
  ssize_t written = write(near_fd_, data, length);
  return written == static_cast<ssize_t>(length);
}

bool PseudoTerminal::readExactly(uint8_t * buffer, size_t length, int timeout_ms)
{
  // check inputs
  if (buffer == nullptr || near_fd_ < 0) {
    return false;
  }

  // read until all bytes are here; each read gets at least one byte,
  // so length reads are always enough
  size_t total = 0;
  for (size_t reads = 0; reads < length && total < length; reads++) {
    pollfd watch{};
    watch.fd = near_fd_;
    watch.events = POLLIN;
    if (poll(&watch, 1, timeout_ms) <= 0) {
      return false;   // error, or nothing in time
    }
    ssize_t count = read(near_fd_, buffer + total, length - total);
    if (count <= 0) {
      return false;
    }
    total += static_cast<size_t>(count);
  }
  return total == length;
}

void PseudoTerminal::hangUp()
{
  if (near_fd_ >= 0) {
    (void)close(near_fd_);   // ignore the result: the test only needs the end gone
    near_fd_ = -1;
  }
}
