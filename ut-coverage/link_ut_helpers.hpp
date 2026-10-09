#ifndef LINK_UT_HELPERS_HPP
#define LINK_UT_HELPERS_HPP

// Guard 1: this file is test-only. Only the test build defines LINK_TEST_BUILD
// (see the Makefile). If this file reaches the flight build, the compiler stops here.
#ifndef LINK_TEST_BUILD
#error "link_ut_helpers.hpp is test-only: it must never be in the flight program"
#endif

#include <array>
#include <cstddef>
#include <cstdint>

/**
 * @brief Prints PASS or FAIL for one test, and counts failures.
 *
 * @param name The test's name.
 * @param passed The test's result.
 * @param failures Increased by one if the test failed.
 */
void report(const char * name, bool passed, int & failures);

/**
 * @brief A pseudo-terminal: a pair of linked files the operating system creates on
 *        request. Bytes written into one end come out of the other.
 *
 * The far end is a terminal file, the same kind of file as /dev/ttyRPMSG0 on the
 * board, so RpmsgPort can open it and be tested on the Mac. The test plays the M33
 * on the near end. (The operating system calls the near end "master" and the far
 * end "slave".)
 *
 * @note Test only. Never part of the flight program.
 */
class PseudoTerminal
{
public:
  /**
   * @brief Creates the pair and opens the near end.
   *
   * @note A constructor cannot return a result. Call isOpen() after it.
   */
  PseudoTerminal();

  /**
   * @brief Closes the near end, if still open.
   */
  ~PseudoTerminal();

  // the pair cannot be copied: two objects would close the same file
  PseudoTerminal(const PseudoTerminal &) = delete;
  PseudoTerminal & operator=(const PseudoTerminal &) = delete;

  /**
   * @brief Tells if the constructor created the pair.
   *
   * @return true if open, false if not.
   */
  bool isOpen() const;

  /**
   * @brief The far end's file name, for example /dev/ttys008. Give it to RpmsgPort.
   *
   * @return The name, or "" if the pair was not created.
   */
  const char * farEndPath() const;

  /**
   * @brief Writes bytes into the near end, so they come out of the far end.
   *
   * @param data The bytes.
   * @param length How many bytes.
   * @return true if all bytes were written.
   */
  bool writeBytes(const uint8_t * data, size_t length);

  /**
   * @brief Reads exactly length bytes that came into the near end from the far end.
   *
   * @param buffer Where to put the bytes.
   * @param length How many bytes to read.
   * @param timeout_ms Longest wait for each piece, in ms.
   * @return true if all bytes arrived in time.
   */
  bool readExactly(uint8_t * buffer, size_t length, int timeout_ms);

  /**
   * @brief Closes the near end now, as if the M33 stopped.
   */
  void hangUp();

private:
  int near_fd_;                        // the near end's file descriptor; -1 when closed
  std::array<char, 128> far_path_;     // the far end's file name
};

#endif  // LINK_UT_HELPERS_HPP
