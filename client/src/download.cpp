// download.cpp — background HTTP downloader for Nyx03DL.
//
// Adapted from ricardoofnl/customskin-0.3.7-R5 (src/model/download.cpp), GPL-3.0.
// See download.h for the two deliberate changes (skinid in Result, fetch to
// ".part" + rename only after a passing CRC check).
#include "download.h"

#include "crc32.h"
#include "log.h"

#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <wininet.h>
#endif

namespace nyx {
namespace dl {

#ifdef _WIN32

bool Fetch(const std::string& url, const std::string& destPath) {
    HINTERNET hNet = InternetOpenA("SAMP/0.3", INTERNET_OPEN_TYPE_PRECONFIG, nullptr,
                                   nullptr, 0);
    if (!hNet) {
        NYX_LOGE("dl: InternetOpen failed (%lu)", GetLastError());
        return false;
    }

    DWORD to = 15000;  // 15 s connect/receive/send timeout for file downloads
    InternetSetOptionA(hNet, INTERNET_OPTION_CONNECT_TIMEOUT, &to, sizeof(to));
    InternetSetOptionA(hNet, INTERNET_OPTION_RECEIVE_TIMEOUT, &to, sizeof(to));
    InternetSetOptionA(hNet, INTERNET_OPTION_SEND_TIMEOUT, &to, sizeof(to));

    HINTERNET hUrl = InternetOpenUrlA(hNet, url.c_str(), nullptr, 0,
                                      INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE,
                                      0);
    if (!hUrl) {
        NYX_LOGE("dl: InternetOpenUrl failed for %s (%lu)", url.c_str(), GetLastError());
        InternetCloseHandle(hNet);
        return false;
    }

    FILE* f = std::fopen(destPath.c_str(), "wb");
    if (!f) {
        NYX_LOGE("dl: cannot open %s for writing", destPath.c_str());
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hNet);
        return false;
    }

    char buf[65536];
    DWORD read = 0;
    size_t total = 0;
    while (InternetReadFile(hUrl, buf, sizeof(buf), &read) && read > 0) {
        std::fwrite(buf, 1, read, f);
        total += read;
    }

    std::fclose(f);
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hNet);

    NYX_LOGI("dl: %s -> %s (%zu bytes)", url.c_str(), destPath.c_str(), total);
    return total > 0;
}

#else

bool Fetch(const std::string&, const std::string&) {
    return false;  // non-Windows stub (unit harness never calls this)
}

#endif

namespace {

struct Job {
    int skinid = 0;
    std::string url;
    std::string dest;
    uint32_t crc = 0;
    bool isDff = false;
};

std::mutex g_mtx;                  // guards g_jobs and g_done
std::condition_variable g_cv;
std::deque<Job> g_jobs;            // pending downloads (worker consumes)
std::vector<Result> g_done;        // finished downloads (monitor thread drains)
std::once_flag g_workerOnce;

// Single detached worker: fetches queued files serially and crc-checks each.
// It only touches the queues + its own file, never game state, so nothing is
// shared unsafely with the monitor thread.
void WorkerLoop() {
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lk(g_mtx);
            g_cv.wait(lk, [] { return !g_jobs.empty(); });
            job = g_jobs.front();
            g_jobs.pop_front();
        }
        Result r;
        r.skinid = job.skinid;
        r.crc = job.crc;
        r.isDff = job.isDff;
        std::string part = job.dest + ".part";
        if (Fetch(job.url, part)) {
            if (crc::File(part.c_str()) == job.crc) {
#ifdef _WIN32
                if (MoveFileExA(part.c_str(), job.dest.c_str(),
                                MOVEFILE_REPLACE_EXISTING))
                    r.ok = true;
                else
                    DeleteFileA(part.c_str());
#else
                std::remove(part.c_str());
#endif
            } else {
#ifdef _WIN32
                DeleteFileA(part.c_str());
#else
                std::remove(part.c_str());
#endif
            }
        } else {
#ifdef _WIN32
            DeleteFileA(part.c_str());
#else
            std::remove(part.c_str());
#endif
        }
        {
            std::lock_guard<std::mutex> lk(g_mtx);
            g_done.push_back(r);
        }
    }
}

}  // namespace

void Enqueue(int skinid, const std::string& url, const std::string& destPath,
             uint32_t crc, bool isDff) {
    std::call_once(g_workerOnce, [] { std::thread(WorkerLoop).detach(); });
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        g_jobs.push_back(Job{skinid, url, destPath, crc, isDff});
    }
    g_cv.notify_one();
}

std::vector<Result> Drain() {
    std::vector<Result> out;
    std::lock_guard<std::mutex> lk(g_mtx);
    out.swap(g_done);
    return out;
}

}  // namespace dl
}  // namespace nyx
