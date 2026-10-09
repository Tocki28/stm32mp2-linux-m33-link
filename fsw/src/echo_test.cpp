#include "echo_test.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <limits>

#include "check.hpp"
#include "config.hpp"

namespace
{

// Pattern numbers. Both are odd, so (keeping the low 8 bits):
//   7:  the bytes inside one message are all different
//   31: each message length starts at a different value
// So a lost, extra, swapped or old byte always changes what comes back.
constexpr size_t kLengthStride = 31;
constexpr size_t kPositionStride = 7;

/**
 * @brief Fills buffer with the test pattern for one message.
 *
 * @param buffer Where to write the pattern.
 * @param length Message length, in bytes.
 * @return OK, or INVALID_ARGUMENT.
 */
Status fillPattern(uint8_t * buffer, size_t length)
{
  // check inputs
  if (!check(buffer != nullptr, "fillPattern: buffer is null")) {
    return Status::INVALID_ARGUMENT;
  }
  if (!check(length <= config::kMaxMessageLength, "fillPattern: length too large")) {
    return Status::INVALID_ARGUMENT;
  }

  // byte i = length * 31 + i * 7, keeping the low 8 bits
  for (size_t i = 0; i < length; i++) {   // bounded: length <= kMaxMessageLength
    buffer[i] = static_cast<uint8_t>((length * kLengthStride + i * kPositionStride) & 0xFFU);
  }
  return Status::OK;
}

/**
 * @brief Collects exactly length bytes, which may arrive in several pieces.
 *
 * @param transport The channel to read from.
 * @param buffer Where to put the bytes.
 * @param length How many bytes to collect.
 * @param timeout_ms Longest wait for each piece, in ms.
 * @return OK if all bytes arrived, TIMEOUT if not, or the error.
 * @note At most kMaxReadsPerMessage reads (NASA rule 2).
 */
Status receiveExactly(Transport & transport, uint8_t * buffer, size_t length,
  uint32_t timeout_ms)
{
  // check inputs
  if (!check(buffer != nullptr, "receiveExactly: buffer is null")) {
    return Status::INVALID_ARGUMENT;
  }
  if (!check(length > 0, "receiveExactly: nothing to receive")) {
    return Status::INVALID_ARGUMENT;
  }

  // read until we have all bytes, or give up
  size_t total = 0;
  for (int reads = 0; reads < config::kMaxReadsPerMessage && total < length; reads++) {
    size_t received = 0;
    Status status = transport.receive(buffer + total, length - total, received, timeout_ms);
    if (status != Status::OK) {
      return status;
    }
    total += received;
  }
  return (total == length) ? Status::OK : Status::TIMEOUT;
}

/**
 * @brief Counts one passing echo and records its time.
 *
 * @param result The result to update.
 * @param round_trip_us How long the echo took, in microseconds.
 */
void recordRoundTrip(EchoTestResult & result, int64_t round_trip_us)
{
  result.passed++;
  result.total_round_trip_us += round_trip_us;
  if (round_trip_us < result.min_round_trip_us) {
    result.min_round_trip_us = round_trip_us;
  }
  if (round_trip_us > result.max_round_trip_us) {
    result.max_round_trip_us = round_trip_us;
  }
}

/**
 * @brief Sends one message, waits for its echo and compares them.
 *
 * @param transport The channel to test.
 * @param length Message length, in bytes.
 * @param timeout_ms Longest wait for the echo, in ms.
 * @param result Counts and times, updated here.
 * @return OK if this step ran, or the Status that stopped it.
 * @note A lost or wrong echo still returns OK. It is counted in result.
 */
Status echoOnce(Transport & transport, size_t length, uint32_t timeout_ms,
  EchoTestResult & result)
{
  // check input
  std::array<uint8_t, config::kMaxMessageLength> sent_bytes{};
  std::array<uint8_t, config::kMaxMessageLength> echoed_bytes{};
  if (!check(length > 0 && length <= sent_bytes.size(), "echoOnce: length out of range")) {
    return Status::INVALID_ARGUMENT;
  }

  // make the message
  Status status = fillPattern(sent_bytes.data(), length);
  if (status != Status::OK) {
    return status;
  }

  // start the clock and send
  auto start = std::chrono::steady_clock::now();
  status = transport.send(sent_bytes.data(), length);
  if (status != Status::OK) {
    return status;
  }
  result.sent++;

  // wait for the echo and stop the clock
  status = receiveExactly(transport, echoed_bytes.data(), length, timeout_ms);
  auto stop = std::chrono::steady_clock::now();

  // lost: count it
  if (status == Status::TIMEOUT) {
    result.timeouts++;
    return Status::OK;
  }
  if (status != Status::OK) {
    return status;
  }

  // different: count it
  if (std::memcmp(sent_bytes.data(), echoed_bytes.data(), length) != 0) {
    result.mismatches++;
    return Status::OK;
  }

  // same: record the time
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
  recordRoundTrip(result, static_cast<int64_t>(elapsed.count()));
  return Status::OK;
}

}  // namespace

Status runEchoTest(Transport & transport, const EchoTestSettings & settings,
  EchoTestResult & result)
{
  // start from an empty result
  result = EchoTestResult{};
  result.min_round_trip_us = std::numeric_limits<int64_t>::max();

  // check inputs
  if (!check(settings.max_length > 0 && settings.max_length <= config::kMaxMessageLength,
        "runEchoTest: max_length out of range")) {
    return Status::INVALID_ARGUMENT;
  }
  if (!check(settings.timeout_ms <= config::kMaxTimeoutMs, "runEchoTest: timeout too long")) {
    return Status::INVALID_ARGUMENT;
  }

  // one message of each length
  for (size_t length = 1; length <= settings.max_length; length++) {   // bounded above
    Status status = echoOnce(transport, length, settings.timeout_ms, result);
    if (status != Status::OK) {
      return status;
    }
  }
  return Status::OK;
}
