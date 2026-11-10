// http.cpp — minimal blocking HTTP GET for Nyx03DL (manifest probe).
//
// Written for Nyx03DL (M2). WinINet, User-Agent "SAMP/0.3" (same as the
// reference .asi; the M1 server does not check it, but the value is kept
// for consistency). Short explicit timeouts: a non-Nyx server must fail
// fast instead of hanging the monitor thread.
#include "http.h"

#ifdef _WIN32
#include <windows.h>
#include <wininet.h>

#include <cstdio>
#endif

namespace nyx {
namespace http {

#ifdef _WIN32

bool Get(const std::string& url, int timeoutMs, long& statusCode, std::string& body) {
    statusCode = 0;
    body.clear();

    HINTERNET hNet = InternetOpenA("SAMP/0.3", INTERNET_OPEN_TYPE_PRECONFIG, nullptr,
                                   nullptr, 0);
    if (!hNet) return false;

    DWORD to = static_cast<DWORD>(timeoutMs);
    InternetSetOptionA(hNet, INTERNET_OPTION_CONNECT_TIMEOUT, &to, sizeof(to));
    InternetSetOptionA(hNet, INTERNET_OPTION_RECEIVE_TIMEOUT, &to, sizeof(to));
    InternetSetOptionA(hNet, INTERNET_OPTION_SEND_TIMEOUT, &to, sizeof(to));

    HINTERNET hUrl = InternetOpenUrlA(
        hNet, url.c_str(), nullptr, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_COOKIES,
        0);
    if (!hUrl) {
        InternetCloseHandle(hNet);
        return false;
    }

    // Apply the same timeouts to the request handle (per WinINet docs the
    // session-level options are inherited, but setting them again is cheap
    // insurance against a server that accepts the TCP connect and then stalls).
    InternetSetOptionA(hUrl, INTERNET_OPTION_RECEIVE_TIMEOUT, &to, sizeof(to));

    DWORD status = 0;
    DWORD statusLen = sizeof(status);
    if (!HttpQueryInfoA(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status,
                        &statusLen, nullptr)) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hNet);
        return false;
    }
    statusCode = static_cast<long>(status);
    if (status != 200) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hNet);
        return false;
    }

    char buf[8192];
    DWORD read = 0;
    while (InternetReadFile(hUrl, buf, sizeof(buf), &read) && read > 0)
        body.append(buf, read);

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hNet);
    return true;
}

#else

bool Get(const std::string&, int, long& statusCode, std::string& body) {
    statusCode = 0;
    body.clear();
    return false;  // non-Windows stub (unit harness never calls this)
}

#endif

}  // namespace http
}  // namespace nyx
