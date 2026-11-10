// http.h — minimal blocking HTTP GET for Nyx03DL (manifest probe).
//
// Written for Nyx03DL (M2). WinINet on Windows; on any other platform the
// call is a stub that returns false (the .asi only ever runs on Windows;
// the Linux unit harness never calls it).
//
// Timeouts are set explicitly (connect/receive/send) so probing a server
// that is not a Nyx server can never hang the monitor thread.
#pragma once

#include <string>

namespace nyx {
namespace http {

// Blocking GET. Returns true only if the request completed AND the server
// answered HTTP 200; `body` then holds the full response body. Any
// failure (no route, timeout, non-200) returns false.
bool Get(const std::string& url, int timeoutMs, long& statusCode, std::string& body);

}  // namespace http
}  // namespace nyx
