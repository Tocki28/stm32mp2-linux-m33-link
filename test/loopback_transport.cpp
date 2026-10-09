#include "loopback_transport.hpp"

#include "check.hpp"

LoopbackTransport::LoopbackTransport()
: storage_{}, head_(0), count_(0)
{
}

Status LoopbackTransport::send(const uint8_t * data, size_t length)
{
  // check inputs
  if (!check(data != nullptr, "LoopbackTransport::send: data is null")) {
    return Status::INVALID_ARGUMENT;
  }
  if (!check(length > 0 && length <= config::kMaxMessageLength,
        "LoopbackTransport::send: length out of range")) {
    return Status::INVALID_ARGUMENT;
  }

  // refuse if there is no room
  if (length > storage_.size() - count_) {
    return Status::BUFFER_FULL;
  }

  // store the bytes after the waiting ones, wrapping around at the end
  for (size_t i = 0; i < length; i++) {   // bounded: length <= kMaxMessageLength
    size_t tail = (head_ + count_ + i) % storage_.size();
    storage_[tail] = data[i];
  }
  count_ += length;
  return Status::OK;
}

Status LoopbackTransport::receive(uint8_t * buffer, size_t capacity, size_t & received,
  uint32_t timeout_ms)
{
  received = 0;
  (void)timeout_ms;   // nothing to wait for: the bytes are already here, or never coming

  // check inputs
  if (!check(buffer != nullptr && capacity > 0, "LoopbackTransport::receive: no room for data")) {
    return Status::INVALID_ARGUMENT;
  }

  // nothing waiting
  if (count_ == 0) {
    return Status::TIMEOUT;
  }

  // copy out as many bytes as fit, oldest first
  size_t take = (capacity < count_) ? capacity : count_;
  for (size_t i = 0; i < take; i++) {     // bounded: take <= kLoopbackCapacity
    buffer[i] = storage_[(head_ + i) % storage_.size()];
  }
  head_ = (head_ + take) % storage_.size();
  count_ -= take;
  received = take;

  // the count can never be larger than the buffer
  if (!check(count_ <= storage_.size(), "LoopbackTransport: count exceeds capacity")) {
    return Status::IO_ERROR;
  }
  return Status::OK;
}
