/**
 * @file test_main.cpp
 * @brief The test program: checks the link logic on the fake channel.
 *
 * @note Runs on the Mac, never on the board.
 * @note echo_test.cpp is the same file the flight program uses, so the logic tested
 *       here is the logic that flies.
 */

#include <array>
#include <cstdio>

#include "config.hpp"
#include "echo_test.hpp"
#include "loopback_transport.hpp"

namespace
{

// exit codes
constexpr int kExitAllPassed = 0;
constexpr int kExitSomeFailed = 1;

// the full-channel test fills the fake with whole messages,
// so the fake's size must be a whole number of messages
static_assert(config::kLoopbackCapacity % config::kMaxMessageLength == 0,
  "loopback capacity must hold a whole number of maximum-length messages");

/**
 * @brief Every length, from 1 to the largest message, must come back the same.
 *
 * @return true if the test passed.
 */
bool testEchoAllLengths()
{
  LoopbackTransport loopback;
  EchoTestSettings settings{config::kMaxMessageLength, config::kDefaultTimeoutMs};
  EchoTestResult result{};
  Status status = runEchoTest(loopback, settings, result);
  return status == Status::OK && result.sent == config::kMaxMessageLength &&
    result.passed == result.sent && result.timeouts == 0 && result.mismatches == 0;
}

/**
 * @brief Nothing was sent, so receive() must say TIMEOUT and give 0 bytes.
 *
 * @return true if the test passed.
 */
bool testReceiveWhenEmpty()
{
  LoopbackTransport loopback;
  std::array<uint8_t, config::kMaxMessageLength> buffer{};
  size_t received = 0;
  Status status = loopback.receive(buffer.data(), buffer.size(), received,
    config::kDefaultTimeoutMs);
  return status == Status::TIMEOUT && received == 0;
}

/**
 * @brief When the fake is full, send() must say BUFFER_FULL and not overwrite old bytes.
 *
 * @return true if the test passed.
 */
bool testSendWhenFull()
{
  LoopbackTransport loopback;
  std::array<uint8_t, config::kMaxMessageLength> message{};
  constexpr size_t kMessagesToFill = config::kLoopbackCapacity / config::kMaxMessageLength;

  // fill the fake with whole messages
  for (size_t i = 0; i < kMessagesToFill; i++) {   // bounded by kMessagesToFill
    if (loopback.send(message.data(), message.size()) != Status::OK) {
      return false;
    }
  }

  // one more must be refused
  return loopback.send(message.data(), message.size()) == Status::BUFFER_FULL;
}

/**
 * @brief Prints PASS or FAIL for one test, and counts failures.
 *
 * @param name The test's name.
 * @param passed The test's result.
 * @param failures Increased by one if the test failed.
 */
void report(const char * name, bool passed, int & failures)
{
  (void)std::printf("%s  %s\n", passed ? "PASS" : "FAIL", name);
  if (!passed) {
    failures++;
  }
}

}  // namespace

/**
 * @brief Runs every test.
 *
 * @return 0 if all passed, 1 if not.
 */
int main()
{
  int failures = 0;
  report("echo of every length 1..max", testEchoAllLengths(), failures);
  report("receive on empty channel -> TIMEOUT", testReceiveWhenEmpty(), failures);
  report("send on full channel -> BUFFER_FULL", testSendWhenFull(), failures);
  (void)std::printf("%d failed\n", failures);
  return (failures == 0) ? kExitAllPassed : kExitSomeFailed;
}
