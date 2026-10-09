#ifndef LINK_CHECK_HPP
#define LINK_CHECK_HPP

#include <cstdio>

/**
 * @brief Checks something that must always be true (NASA rule 5).
 *
 * If it is false, prints the description on stderr. The program keeps running:
 * the caller returns an error instead.
 *
 * @param condition What must be true.
 * @param description What to print if it is false.
 * @return condition, unchanged.
 *
 * @note Unlike assert(), it never stops the program.
 * @note It is in the header (inline) so the static analyzer can see inside it.
 *       In its own .cpp file, the analyzer reported false errors (found 8 Oct 2026).
 *
 * Use:
 * @code
 * if (!check(data != nullptr, "send: data is null")) {
 *   return Status::INVALID_ARGUMENT;
 * }
 * @endcode
 */
[[nodiscard]] inline bool check(bool condition, const char * description)
{
  if (!condition) {
    const char * text = (description != nullptr) ? description : "(no description)";
    // ignore fprintf's result: if stderr fails, there is nowhere left to report it
    (void)std::fprintf(stderr, "CHECK FAILED: %s\n", text);
  }
  return condition;
}

#endif  // LINK_CHECK_HPP
