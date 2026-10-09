#include "loopback_transport.hpp"

#include "check.hpp"

LoopbackTransport::LoopbackTransport()
: LoopbackTransport(LoopbackFaults{0, false, false, false})
{
}

LoopbackTransport::LoopbackTransport(const LoopbackFaults & faults)
: faults_(faults), storage_{}, head_(0), count_(0)
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

  // how many bytes to keep: all, all but the last one, or all plus the last one again
  size_t kept = faults_.drop_last_byte ? length - 1 : length;
  size_t total = faults_.repeat_last_byte ? kept + 1 : kept;

  // refuse if there is no room
  if (total > storage_.size() - count_) {
    return Status::BUFFER_FULL;
  }

  // store the bytes after the waiting ones, wrapping around at the end
  for (size_t i = 0; i < total; i++) {   // bounded: total <= kMaxMessageLength + 1
    uint8_t byte = (i < kept) ? data[i] : data[length - 1];   // past kept: the repeat
    if (i == 0 && faults_.flip_first_byte) {
      byte = static_cast<uint8_t>(byte ^ 0x01U);   // fault: the echo comes back different
    }
    size_t tail = (head_ + count_ + i) % storage_.size();
    storage_[tail] = byte;
  }
  count_ += total;
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

  // copy out as many bytes as fit, oldest first; at most piece_size if set (fault)
  size_t take = (capacity < count_) ? capacity : count_;
  if (faults_.piece_size > 0 && take > faults_.piece_size) {
    take = faults_.piece_size;
  }
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
