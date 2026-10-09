/**
 * @file main.cpp
 * @brief The flight program: tests the real channel to the M33.
 *
 * @note The fake channel is not in this program. It is in the test program
 *       (test_main.cpp), which the Makefile builds separately. So no typo or wrong
 *       setting can make this program use the fake.
 */

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "check.hpp"
#include "config.hpp"
#include "echo_test.hpp"
#include "rpmsg_port.hpp"

namespace
{

// exit codes: tell scripts and systemd what happened
constexpr int kExitAllPassed = 0;    // every echo came back correct
constexpr int kExitSomeFailed = 1;   // some echoes were lost or wrong
constexpr int kExitUsage = 2;        // bad command line
constexpr int kExitCannotRun = 3;    // the channel did not open, or the test could not run

/**
 * @brief What the command line asked for.
 */
struct Options
{
  const char * device_path;   // the channel to test
  EchoTestSettings test;      // longest message and wait per echo
};

/**
 * @brief Prints how to use the program.
 *
 * @param program The program's name, as typed (argv[0]).
 */
void printUsage(const char * program)
{
  (void)std::fprintf(stderr,
    "usage: %s [--device PATH] [--timeout-ms N] [--max-length N]\n"
    "  --device PATH   rpmsg channel to test (default %s)\n"
    "  --timeout-ms N  wait per echo, 1..%u ms (default %u)\n"
    "  --max-length N  longest message, 1..%zu bytes (default %zu)\n",
    program, config::kDefaultDevicePath, config::kMaxTimeoutMs, config::kDefaultTimeoutMs,
    config::kMaxMessageLength, config::kDefaultTestMaxLength);
}

/**
 * @brief Turns text into a whole number between min and max.
 *
 * @param text The text, for example "100".
 * @param min Smallest allowed value.
 * @param max Largest allowed value.
 * @param value Set to the number, only if the text is valid.
 * @return true if valid. false if it is not a number, has extra characters,
 *         or is out of range.
 */
bool parseNumber(const char * text, unsigned long min, unsigned long max, unsigned long & value)
{
  // check inputs
  if (!check(text != nullptr, "parseNumber: text is null")) {
    return false;
  }
  if (!check(min <= max, "parseNumber: empty range")) {
    return false;
  }

  // convert; end is set to the first character that is not a digit
  errno = 0;
  char * end = nullptr;
  unsigned long parsed = std::strtoul(text, &end, 10);

  // reject: too big, no digits, extra characters, or out of range
  if (errno != 0 || end == text || *end != '\0' || parsed < min || parsed > max) {
    return false;
  }
  value = parsed;
  return true;
}

/**
 * @brief Reads the command line into options.
 *
 * @param argc How many words were typed, including the program's name.
 * @param argv The words typed.
 * @param options Set to the defaults, then to what was typed.
 * @return true if every word was understood, false if not.
 */
bool parseOptions(int argc, char ** argv, Options & options)
{
  // start from the defaults in config.hpp
  options.device_path = config::kDefaultDevicePath;
  options.test.max_length = config::kDefaultTestMaxLength;
  options.test.timeout_ms = config::kDefaultTimeoutMs;

  // read the words one by one
  for (int i = 1; i < argc; i++) {   // bounded by argc
    const char * arg = argv[i];
    bool has_value = (i + 1 < argc);
    unsigned long number = 0;

    if (std::strcmp(arg, "--device") == 0 && has_value) {
      options.device_path = argv[++i];
    } else if (std::strcmp(arg, "--timeout-ms") == 0 && has_value &&
               parseNumber(argv[++i], 1, config::kMaxTimeoutMs, number)) {
      options.test.timeout_ms = static_cast<uint32_t>(number);
    } else if (std::strcmp(arg, "--max-length") == 0 && has_value &&
               parseNumber(argv[++i], 1, config::kMaxMessageLength, number)) {
      options.test.max_length = static_cast<size_t>(number);
    } else {
      return false;   // unknown word, or a bad value
    }
  }
  return true;
}

/**
 * @brief Runs the echo test and prints the result.
 *
 * @param transport The channel to test.
 * @param name The channel's name, for printing.
 * @param settings Longest message and wait per echo.
 * @return The exit code: 0 all passed, 1 some failed, 3 the test could not run.
 */
int runAndReport(Transport & transport, const char * name, const EchoTestSettings & settings)
{
  // run the test
  EchoTestResult result{};
  Status status = runEchoTest(transport, settings, result);
  if (status != Status::OK) {
    (void)std::fprintf(stderr, "echo test on %s stopped: %s\n", name, statusName(status));
    return kExitCannotRun;
  }

  // print the counts
  (void)std::printf("echo test on %s: %zu sent, %zu passed, %zu timeouts, %zu mismatches\n",
    name, result.sent, result.passed, result.timeouts, result.mismatches);

  // print the times, if any echo passed
  if (result.passed > 0) {
    (void)std::printf("round trip: min %lld us, average %lld us, max %lld us\n",
      static_cast<long long>(result.min_round_trip_us),
      static_cast<long long>(result.total_round_trip_us / static_cast<int64_t>(result.passed)),
      static_cast<long long>(result.max_round_trip_us));
  }
  return (result.passed == result.sent) ? kExitAllPassed : kExitSomeFailed;
}

}  // namespace

/**
 * @brief Reads the command line, opens the channel, runs the test.
 *
 * @return 0 all passed, 1 some failed, 2 bad command line, 3 cannot run.
 */
int main(int argc, char ** argv)
{
  // 1. read the command line
  Options options{};
  if (!parseOptions(argc, argv, options)) {
    printUsage(argv[0]);
    return kExitUsage;
  }

  // 2. open the channel
  RpmsgPort port(options.device_path);
  if (!port.isOpen()) {
    (void)std::fprintf(stderr, "cannot open %s: %s\n", options.device_path,
      std::strerror(port.openErrno()));
    return kExitCannotRun;
  }

  // 3. run the test (the channel closes when main ends)
  return runAndReport(port, options.device_path, options.test);
}
