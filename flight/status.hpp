#ifndef LINK_STATUS_HPP
#define LINK_STATUS_HPP

/**
 * @brief The result of every operation that can fail.
 *
 * @note [[nodiscard]]: the compiler warns if a Status is ignored (NASA rule 7).
 */
enum class [[nodiscard]] Status
{
  OK,                 // it worked
  TIMEOUT,            // nothing arrived in time
  IO_ERROR,           // Linux reported an error
  BUFFER_FULL,        // no room left in a fixed buffer
  INVALID_ARGUMENT,   // the caller passed bad values: this is a bug
  NOT_OPEN,           // the channel is not open
};

/**
 * @brief Gives the name of a Status, for printing.
 *
 * @param status The Status to name.
 * @return The name, for example "TIMEOUT".
 */
const char * statusName(Status status);

#endif  // LINK_STATUS_HPP
