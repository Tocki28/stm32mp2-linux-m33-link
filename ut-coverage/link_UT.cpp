/**
 * @file link_UT.cpp
 * @brief Coverage tests of the flight code (cFE layout: ut-coverage/).
 *
 * @note Runs on the Mac, never on the board.
 * @note It compiles the same flight files as the board program (fsw/src/), so the code
 *       tested here is the code that flies. Only main.cpp is left out: it needs the real
 *       M33 channel, so it is tested by running link_test on the board.
 * @note Lines starting with "CHECK FAILED" are expected: some tests pass bad values on
 *       purpose, to prove they are refused.
 */

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>

#include "config.hpp"
#include "echo_test.hpp"
#include "link_ut_helpers.hpp"
#include "loopback_transport.hpp"
#include "options.hpp"
#include "rpmsg_port.hpp"
#include "status.hpp"

namespace
{

// exit codes
constexpr int kExitAllPassed = 0;
constexpr int kExitSomeFailed = 1;

// the full-channel test fills the fake with whole messages,
// so the fake's size must be a whole number of messages
static_assert(config::kLoopbackCapacity % config::kMaxMessageLength == 0,
  "loopback capacity must hold a whole number of maximum-length messages");

// how long the pseudo-terminal tests wait for bytes, in ms
constexpr int kWaitMs = static_cast<int>(config::kDefaultTimeoutMs);

// how long to wait to be sure no extra byte comes; the pseudo-terminal is in memory,
// so 10 ms is plenty (TBD: chosen by hand, not measured)
constexpr int kNothingMoreMs = 10;

// a file that does not exist, to test a failed open
constexpr const char * kMissingPath = "/dev/link-test-no-such-channel";

// ---------------------------------------------------------------------------------
// options.cpp
// ---------------------------------------------------------------------------------

// longest command line in these tests, in words, program name included
constexpr size_t kMaxWords = 7;

// a command line: the words, then nullptr for the unused places
using CommandLine = std::array<const char *, kMaxWords>;

/**
 * @brief Runs parseOptions on a command line.
 *
 * @param words The words; the list ends at the first nullptr.
 * @param options Filled by parseOptions.
 * @return What parseOptions returned.
 */
bool parseCommandLine(const CommandLine & words, Options & options)
{
  // count the words
  size_t count = 0;
  while (count < words.size() && words[count] != nullptr) {   // bounded by words.size()
    count++;
  }
  return parseOptions(static_cast<int>(count), words.data(), options);
}

/**
 * @brief No words after the program name: every option takes its default.
 */
bool testOptionsDefaults()
{
  Options options{};
  CommandLine words = {{"link_test"}};
  return parseCommandLine(words, options) &&
    std::strcmp(options.device_path, config::kDefaultDevicePath) == 0 &&
    options.test.timeout_ms == config::kDefaultTimeoutMs &&
    options.test.max_length == config::kDefaultTestMaxLength;
}

/**
 * @brief Every option given: each one is read.
 */
bool testOptionsAllGiven()
{
  Options options{};
  CommandLine words = {{"link_test", "--device", "/dev/ttyRPMSG1", "--timeout-ms", "250",
    "--max-length", "32"}};
  return parseCommandLine(words, options) &&
    std::strcmp(options.device_path, "/dev/ttyRPMSG1") == 0 &&
    options.test.timeout_ms == 250 && options.test.max_length == 32;
}

/**
 * @brief The smallest and the largest allowed numbers are accepted.
 */
bool testOptionsLimitsAccepted()
{
  // the largest values, as text, taken from config.hpp
  std::array<char, 24> max_timeout{};
  std::array<char, 24> max_length{};
  (void)std::snprintf(max_timeout.data(), max_timeout.size(), "%u",
    static_cast<unsigned int>(config::kMaxTimeoutMs));
  (void)std::snprintf(max_length.data(), max_length.size(), "%zu", config::kMaxMessageLength);

  Options low{};
  Options high{};
  CommandLine low_words = {{"link_test", "--timeout-ms", "1", "--max-length", "1"}};
  CommandLine high_words = {{"link_test", "--timeout-ms", max_timeout.data(),
    "--max-length", max_length.data()}};
  return parseCommandLine(low_words, low) && parseCommandLine(high_words, high) &&
    low.test.timeout_ms == 1 && low.test.max_length == 1 &&
    high.test.timeout_ms == config::kMaxTimeoutMs &&
    high.test.max_length == config::kMaxMessageLength;
}

/**
 * @brief Every bad command line is refused.
 */
bool testOptionsBadRefused()
{
  // one above each largest value, as text, taken from config.hpp
  std::array<char, 24> above_timeout{};
  std::array<char, 24> above_length{};
  (void)std::snprintf(above_timeout.data(), above_timeout.size(), "%u",
    static_cast<unsigned int>(config::kMaxTimeoutMs) + 1U);
  (void)std::snprintf(above_length.data(), above_length.size(), "%zu",
    config::kMaxMessageLength + 1U);

  const std::array<CommandLine, 13> bad = {{
    {{"link_test", "--timeout-ms", "abc"}},                       // not a number
    {{"link_test", "--timeout-ms", "12x"}},                       // extra characters
    {{"link_test", "--timeout-ms", ""}},                          // empty
    {{"link_test", "--timeout-ms", "0"}},                         // below 1
    {{"link_test", "--timeout-ms", above_timeout.data()}},        // above the largest
    {{"link_test", "--timeout-ms", "-5"}},                        // negative: strtoul makes it huge
    {{"link_test", "--timeout-ms", "99999999999999999999999"}},   // too big for unsigned long
    {{"link_test", "--max-length", "0"}},                         // below 1
    {{"link_test", "--max-length", above_length.data()}},         // above the largest
    {{"link_test", "--timeout-ms"}},                              // value missing
    {{"link_test", "--device"}},                                  // value missing
    {{"link_test", "--verbose"}},                                 // unknown word
    {{"link_test", "/dev/ttyRPMSG0"}},                            // value without its word
  }};

  bool all_refused = true;
  for (size_t row = 0; row < bad.size(); row++) {   // bounded by bad.size()
    Options options{};
    if (parseCommandLine(bad[row], options)) {
      (void)std::printf("  wrongly accepted: bad command line %zu\n", row);
      all_refused = false;
    }
  }
  return all_refused;
}

/**
 * @brief parseNumber refuses a null text and an empty range, and leaves the value
 *        unchanged when it refuses.
 */
bool testParseNumberChecks()
{
  unsigned long value = 7;
  bool null_refused = !parseNumber(nullptr, 1, 10, value);
  bool empty_range_refused = !parseNumber("5", 10, 1, value);
  bool bad_text_refused = !parseNumber("abc", 1, 10, value);
  return null_refused && empty_range_refused && bad_text_refused && value == 7;
}

// ---------------------------------------------------------------------------------
// echo_test.cpp
// ---------------------------------------------------------------------------------

// how many reads echo_test allows per message, as a size
constexpr size_t kReads = static_cast<size_t>(config::kMaxReadsPerMessage);

/**
 * @brief Runs the echo test on a fake channel with the given faults.
 *
 * @param faults Which faults the fake injects.
 * @param max_length Longest message.
 * @param result Filled with the counts and times.
 * @return What runEchoTest returned.
 */
Status echoWithFaults(const LoopbackFaults & faults, size_t max_length, EchoTestResult & result)
{
  LoopbackTransport loopback(faults);
  EchoTestSettings settings{max_length, config::kDefaultTimeoutMs};
  return runEchoTest(loopback, settings, result);
}

/**
 * @brief Every length, from 1 to the largest message, comes back the same, and the
 *        times are recorded.
 */
bool testEchoAllLengths()
{
  EchoTestResult result{};
  Status status = echoWithFaults(LoopbackFaults{0, false, false, false},
    config::kMaxMessageLength, result);
  return status == Status::OK && result.sent == config::kMaxMessageLength &&
    result.passed == result.sent && result.timeouts == 0 && result.mismatches == 0 &&
    result.min_round_trip_us >= 0 && result.min_round_trip_us <= result.max_round_trip_us;
}

/**
 * @brief An echo that arrives in pieces is put back together, up to the read limit:
 *        the longest message needs exactly kMaxReadsPerMessage pieces.
 */
bool testEchoInPieces()
{
  constexpr size_t kPiece = (config::kDefaultTestMaxLength + kReads - 1) / kReads;
  EchoTestResult result{};
  Status status = echoWithFaults(LoopbackFaults{kPiece, false, false, false},
    config::kDefaultTestMaxLength, result);
  return status == Status::OK && result.sent == config::kDefaultTestMaxLength &&
    result.passed == result.sent;
}

/**
 * @brief An echo that needs more reads than kMaxReadsPerMessage counts as lost, even
 *        though its bytes are there: one byte per read, so the message one byte longer
 *        than the read limit is the first to fail.
 */
bool testEchoTooManyPieces()
{
  EchoTestResult result{};
  Status status = echoWithFaults(LoopbackFaults{1, false, false, false}, kReads + 1, result);
  return status == Status::OK && result.sent == kReads + 1 && result.passed == kReads &&
    result.timeouts == 1 && result.mismatches == 0;
}

/**
 * @brief An echo that comes back different is counted as a mismatch, never as a pass.
 */
bool testEchoDifferent()
{
  EchoTestResult result{};
  Status status = echoWithFaults(LoopbackFaults{0, true, false, false}, 4, result);
  return status == Status::OK && result.sent == 4 && result.mismatches == 4 &&
    result.passed == 0 && result.timeouts == 0;
}

/**
 * @brief An echo that arrives incomplete is counted as a timeout, never as a pass.
 */
bool testEchoIncomplete()
{
  EchoTestResult result{};
  Status status = echoWithFaults(LoopbackFaults{0, false, true, false}, 4, result);
  return status == Status::OK && result.sent == 4 && result.timeouts == 4 &&
    result.passed == 0 && result.mismatches == 0;
}

/**
 * @brief An extra byte is caught. The first message still matches; its extra byte stays
 *        in the channel and shifts every later echo, so each later one counts as
 *        different. None passes falsely, but the test never gets back in step: nothing
 *        in a raw echo marks where a message starts.
 */
bool testEchoExtraByte()
{
  EchoTestResult result{};
  Status status = echoWithFaults(LoopbackFaults{0, false, false, true}, 4, result);
  return status == Status::OK && result.sent == 4 && result.passed == 1 &&
    result.mismatches == 3 && result.timeouts == 0;
}

/**
 * @brief Bad settings are refused before anything is sent.
 */
bool testEchoBadSettings()
{
  LoopbackTransport loopback;
  EchoTestResult result{};
  EchoTestSettings zero_length{0, config::kDefaultTimeoutMs};
  EchoTestSettings too_long{config::kMaxMessageLength + 1, config::kDefaultTimeoutMs};
  EchoTestSettings wait_too_long{1, config::kMaxTimeoutMs + 1};
  return runEchoTest(loopback, zero_length, result) == Status::INVALID_ARGUMENT &&
    runEchoTest(loopback, too_long, result) == Status::INVALID_ARGUMENT &&
    runEchoTest(loopback, wait_too_long, result) == Status::INVALID_ARGUMENT &&
    result.sent == 0;
}

/**
 * @brief A channel that is not open stops the test with NOT_OPEN; nothing is counted.
 */
bool testEchoChannelNotOpen()
{
  RpmsgPort port(kMissingPath);
  EchoTestSettings settings{1, config::kDefaultTimeoutMs};
  EchoTestResult result{};
  return runEchoTest(port, settings, result) == Status::NOT_OPEN && result.sent == 0;
}

// ---------------------------------------------------------------------------------
// rpmsg_port.cpp, on a pseudo-terminal (see link_ut_helpers.hpp)
// ---------------------------------------------------------------------------------

/**
 * @brief Fills buffer with 0, 1, 2, ... so a 256-byte buffer holds every byte value.
 *
 * @param buffer Where to write.
 * @param length How many bytes.
 */
void fillCounting(uint8_t * buffer, size_t length)
{
  for (size_t i = 0; i < length; i++) {   // bounded by length
    buffer[i] = static_cast<uint8_t>(i & 0xFFU);
  }
}

/**
 * @brief Collects exactly length bytes from the port, which may arrive in pieces.
 *
 * @param port The port to read.
 * @param buffer Where to put the bytes.
 * @param length How many bytes to collect.
 * @return OK, TIMEOUT, or the port's error.
 */
Status receiveAll(RpmsgPort & port, uint8_t * buffer, size_t length)
{
  // each read gets at least one byte, so length reads are always enough
  size_t total = 0;
  for (size_t reads = 0; reads < length && total < length; reads++) {
    size_t received = 0;
    Status status = port.receive(buffer + total, length - total, received,
      config::kDefaultTimeoutMs);
    if (status != Status::OK) {
      return status;
    }
    total += received;
  }
  return (total == length) ? Status::OK : Status::TIMEOUT;
}

/**
 * @brief The port opens a terminal file.
 */
bool testPortOpens()
{
  PseudoTerminal pty;
  RpmsgPort port(pty.farEndPath());
  return pty.isOpen() && port.isOpen() && port.openErrno() == 0;
}

/**
 * @brief Raw mode, sending: every byte value arrives unchanged, and nothing extra.
 *        Without raw mode the terminal would turn the newline byte into two bytes.
 */
bool testPortSendRaw()
{
  PseudoTerminal pty;
  RpmsgPort port(pty.farEndPath());
  std::array<uint8_t, config::kMaxMessageLength> sent{};
  std::array<uint8_t, config::kMaxMessageLength> arrived{};
  std::array<uint8_t, 1> extra{};
  fillCounting(sent.data(), sent.size());
  return pty.isOpen() && port.isOpen() &&
    port.send(sent.data(), sent.size()) == Status::OK &&
    pty.readExactly(arrived.data(), arrived.size(), kWaitMs) &&
    std::memcmp(sent.data(), arrived.data(), sent.size()) == 0 &&
    !pty.readExactly(extra.data(), extra.size(), kNothingMoreMs);
}

/**
 * @brief Raw mode, receiving: every byte value arrives unchanged, without waiting for a
 *        newline. Without raw mode the terminal would hold the bytes until a newline
 *        and act on control bytes (found on the board, 8 Oct 2026).
 */
bool testPortReceiveRaw()
{
  PseudoTerminal pty;
  RpmsgPort port(pty.farEndPath());
  std::array<uint8_t, config::kMaxMessageLength> sent{};
  std::array<uint8_t, config::kMaxMessageLength> arrived{};
  fillCounting(sent.data(), sent.size());
  return pty.isOpen() && port.isOpen() &&
    pty.writeBytes(sent.data(), sent.size()) &&
    receiveAll(port, arrived.data(), arrived.size()) == Status::OK &&
    std::memcmp(sent.data(), arrived.data(), sent.size()) == 0;
}

/**
 * @brief Nothing sent: receive() gives TIMEOUT and 0 bytes.
 */
bool testPortReceiveTimeout()
{
  PseudoTerminal pty;
  RpmsgPort port(pty.farEndPath());
  std::array<uint8_t, 16> buffer{};
  size_t received = 0;
  return pty.isOpen() && port.isOpen() &&
    port.receive(buffer.data(), buffer.size(), received, config::kDefaultTimeoutMs) ==
      Status::TIMEOUT &&
    received == 0;
}

/**
 * @brief The other end closes (as if the M33 stopped): receive() and send() give
 *        IO_ERROR.
 */
bool testPortPeerGone()
{
  PseudoTerminal pty;
  RpmsgPort port(pty.farEndPath());
  if (!pty.isOpen() || !port.isOpen()) {
    return false;
  }
  pty.hangUp();
  std::array<uint8_t, 16> buffer{};
  size_t received = 0;
  Status receive_status = port.receive(buffer.data(), buffer.size(), received,
    config::kDefaultTimeoutMs);
  Status send_status = port.send(buffer.data(), buffer.size());
  return receive_status == Status::IO_ERROR && send_status == Status::IO_ERROR;
}

/**
 * @brief A missing file: the port is not open, says why, and refuses to send or receive.
 */
bool testPortMissingFile()
{
  RpmsgPort port(kMissingPath);
  std::array<uint8_t, 4> buffer{};
  size_t received = 0;
  return !port.isOpen() && port.openErrno() == ENOENT &&
    port.send(buffer.data(), buffer.size()) == Status::NOT_OPEN &&
    port.receive(buffer.data(), buffer.size(), received, 1) == Status::NOT_OPEN;
}

/**
 * @brief A file that is not a terminal: raw mode cannot be set, so the port closes it
 *        again and says why.
 *
 * @note Which reason depends on the system: on macOS tcgetattr() on /dev/null gives
 *       ENODEV (measured 9 Oct 2026); Linux documents ENOTTY. So the test checks that a
 *       reason is given, not which one.
 */
bool testPortNotATerminal()
{
  RpmsgPort port("/dev/null");
  return !port.isOpen() && port.openErrno() != 0;
}

/**
 * @brief No path at all: the port is not open and says EINVAL.
 */
bool testPortNullPath()
{
  RpmsgPort port(nullptr);
  return !port.isOpen() && port.openErrno() == EINVAL;
}

/**
 * @brief Bad arguments are refused, even on an open port.
 */
bool testPortBadArguments()
{
  PseudoTerminal pty;
  RpmsgPort port(pty.farEndPath());
  std::array<uint8_t, config::kMaxMessageLength + 1> buffer{};
  size_t received = 0;
  return pty.isOpen() && port.isOpen() &&
    port.send(nullptr, 1) == Status::INVALID_ARGUMENT &&
    port.send(buffer.data(), 0) == Status::INVALID_ARGUMENT &&
    port.send(buffer.data(), buffer.size()) == Status::INVALID_ARGUMENT &&
    port.receive(nullptr, 1, received, 1) == Status::INVALID_ARGUMENT &&
    port.receive(buffer.data(), 0, received, 1) == Status::INVALID_ARGUMENT &&
    port.receive(buffer.data(), buffer.size(), received, config::kMaxTimeoutMs + 1) ==
      Status::INVALID_ARGUMENT;
}

// ---------------------------------------------------------------------------------
// status.cpp
// ---------------------------------------------------------------------------------

/**
 * @brief Every Status has its own name, and a value outside the list gets "UNKNOWN".
 *
 * @note A new Status must be added to kAll below, or this test does not see it.
 */
bool testStatusNames()
{
  constexpr std::array<Status, 6> kAll = {{Status::OK, Status::TIMEOUT, Status::IO_ERROR,
    Status::BUFFER_FULL, Status::INVALID_ARGUMENT, Status::NOT_OPEN}};

  // each name exists, is not UNKNOWN, and differs from every other name
  for (size_t i = 0; i < kAll.size(); i++) {         // bounded by kAll.size()
    const char * name = statusName(kAll[i]);
    if (name == nullptr || std::strcmp(name, "UNKNOWN") == 0) {
      return false;
    }
    for (size_t j = i + 1; j < kAll.size(); j++) {   // bounded by kAll.size()
      // a plain loop, like the C it would be; std::any_of would hide the bound
      // cppcheck-suppress useStlAlgorithm
      if (std::strcmp(name, statusName(kAll[j])) == 0) {
        return false;
      }
    }
  }

  // a value no Status has
  return std::strcmp(statusName(static_cast<Status>(99)), "UNKNOWN") == 0;
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

  // options.cpp
  report("options: no words -> defaults", testOptionsDefaults(), failures);
  report("options: every option given -> each one read", testOptionsAllGiven(), failures);
  report("options: smallest and largest numbers -> accepted", testOptionsLimitsAccepted(),
    failures);
  report("options: 13 bad command lines -> all refused", testOptionsBadRefused(), failures);
  report("options: parseNumber null / empty range -> refused, value kept",
    testParseNumberChecks(), failures);

  // echo_test.cpp
  report("echo: every length 1..max -> all pass, times recorded", testEchoAllLengths(),
    failures);
  report("echo: arrives in pieces, within the read limit -> all pass", testEchoInPieces(),
    failures);
  report("echo: needs more reads than the limit -> TIMEOUT", testEchoTooManyPieces(),
    failures);
  report("echo: comes back different -> MISMATCH", testEchoDifferent(), failures);
  report("echo: arrives incomplete -> TIMEOUT", testEchoIncomplete(), failures);
  report("echo: extra byte -> later echoes MISMATCH, none passes", testEchoExtraByte(),
    failures);
  report("echo: bad settings -> INVALID_ARGUMENT", testEchoBadSettings(), failures);
  report("echo: channel not open -> NOT_OPEN", testEchoChannelNotOpen(), failures);

  // rpmsg_port.cpp
  report("port: opens a terminal file", testPortOpens(), failures);
  report("port: send, every byte value -> unchanged, nothing extra", testPortSendRaw(),
    failures);
  report("port: receive, every byte value -> unchanged, no newline needed",
    testPortReceiveRaw(), failures);
  report("port: nothing sent -> TIMEOUT", testPortReceiveTimeout(), failures);
  report("port: other end closed -> IO_ERROR", testPortPeerGone(), failures);
  report("port: missing file -> not open, ENOENT, NOT_OPEN", testPortMissingFile(),
    failures);
  report("port: not a terminal -> not open, reason given", testPortNotATerminal(), failures);
  report("port: null path -> not open, EINVAL", testPortNullPath(), failures);
  report("port: bad arguments -> INVALID_ARGUMENT", testPortBadArguments(), failures);

  // status.cpp
  report("status: every Status named, unknown -> UNKNOWN", testStatusNames(), failures);

  (void)std::printf("%d failed\n", failures);
  return (failures == 0) ? kExitAllPassed : kExitSomeFailed;
}
