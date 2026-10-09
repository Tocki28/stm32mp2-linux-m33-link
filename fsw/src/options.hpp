#ifndef LINK_OPTIONS_HPP
#define LINK_OPTIONS_HPP

#include "echo_test.hpp"

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
void printUsage(const char * program);

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
[[nodiscard]] bool parseNumber(const char * text, unsigned long min, unsigned long max,
  unsigned long & value);

/**
 * @brief Reads the command line into options.
 *
 * @param argc How many words were typed, including the program's name.
 * @param argv The words typed.
 * @param options Set to the defaults, then to what was typed.
 * @return true if every word was understood, false if not.
 * @note It is in its own file, not in main.cpp, so the tests can call it.
 */
[[nodiscard]] bool parseOptions(int argc, const char * const * argv, Options & options);

#endif  // LINK_OPTIONS_HPP
