#include "status.hpp"

const char * statusName(Status status)
{
  switch (status) {
    case Status::OK:               return "OK";
    case Status::TIMEOUT:          return "TIMEOUT";
    case Status::IO_ERROR:         return "IO_ERROR";
    case Status::BUFFER_FULL:      return "BUFFER_FULL";
    case Status::INVALID_ARGUMENT: return "INVALID_ARGUMENT";
    case Status::NOT_OPEN:         return "NOT_OPEN";
  }
  return "UNKNOWN";   // only if a new Status is added and not listed above
}
