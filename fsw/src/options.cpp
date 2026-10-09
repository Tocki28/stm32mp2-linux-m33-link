#include "options.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "check.hpp"
#include "config.hpp"

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

bool parseOptions(int argc, const char * const * argv, Options & options)
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
