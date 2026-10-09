/**
 * @file main.cpp
 * @brief The flight program: tests the real channel to the M33.
 *
 * @note The fake channel is not in this program. It is in the test program
 *       (ut-coverage/link_UT.cpp), which the Makefile builds separately. So no typo or
 *       wrong setting can make this program use the fake.
 * @note This is the only flight file the test program leaves out: it needs the real
 *       M33 channel, so it is tested by running link_test on the board.
 */

#include <cstdio>
#include <cstring>

#include "echo_test.hpp"
#include "options.hpp"
#include "rpmsg_port.hpp"

namespace
{

// exit codes: tell scripts and systemd what happened
constexpr int kExitAllPassed = 0;    // every echo came back correct
constexpr int kExitSomeFailed = 1;   // some echoes were lost or wrong
constexpr int kExitUsage = 2;        // bad command line
constexpr int kExitCannotRun = 3;    // the channel did not open, or the test could not run

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
