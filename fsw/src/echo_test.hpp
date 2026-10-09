#ifndef LINK_ECHO_TEST_HPP
#define LINK_ECHO_TEST_HPP

#include <cstddef>
#include <cstdint>

#include "status.hpp"
#include "transport.hpp"

/**
 * @brief Settings of the echo test.
 */
struct EchoTestSettings
{
  size_t max_length;     // send messages of 1, 2, ... up to this many bytes
  uint32_t timeout_ms;   // how long to wait for each echo, in ms
};

/**
 * @brief What the echo test measured.
 */
struct EchoTestResult
{
  size_t sent;                   // messages sent
  size_t passed;                 // came back complete and the same
  size_t timeouts;               // did not come back in time
  size_t mismatches;             // came back, but different
  int64_t min_round_trip_us;     // fastest passing echo, in microseconds
  int64_t max_round_trip_us;     // slowest passing echo, in microseconds
  int64_t total_round_trip_us;   // all passing echoes added up, for the average
};

/**
 * @brief Sends one message of each length from 1 to max_length, and checks each echo.
 *
 * @param transport The channel to test (real or fake).
 * @param settings Longest message and wait per echo.
 * @param result Filled with the counts and times.
 * @return OK if the test ran to the end, or the Status that stopped it.
 * @note A lost or wrong echo does not stop the test. It is counted in result.
 */
Status runEchoTest(Transport & transport, const EchoTestSettings & settings,
  EchoTestResult & result);

#endif  // LINK_ECHO_TEST_HPP
