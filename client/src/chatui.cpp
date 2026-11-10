// chatui.cpp — in-game download-status UI for Nyx03DL (v1.1.0).
//
// Queue lives here (mutex-guarded); the actual CChat::AddMessage call
// is the reference repo's DebugChat pattern, verbatim:
//   CChat::AddMessage(this, D3DCOLOR color, const char* text)  thiscall
//   this = *reinterpret_cast<void**>(samp.dll + kRefChat)
//   fn   = samp.dll + kCChat_AddMessage
// R5-gated: Drain() does nothing unless samp::IsR5Build() is true.
#include "chatui.h"

#include "log.h"
#include "samp.h"

#include <mutex>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <urmem.hpp>
#endif

namespace chatui {
namespace {

struct Line {
    uint32_t color;
    std::string text;
};

std::mutex g_mutex;
std::vector<Line> g_queue;
constexpr size_t kMaxQueued = 8;

}  // namespace

void Enqueue(uint32_t color, const std::string& text) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_queue.size() >= kMaxQueued) g_queue.erase(g_queue.begin());
    g_queue.push_back(Line{color, text});
}

void Drain() {
#ifdef _WIN32
    std::vector<Line> lines;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_queue.empty()) return;
        lines.swap(g_queue);
    }
    if (!nyx::samp::IsR5Build()) return;
    void* chat = *reinterpret_cast<void**>(nyx::samp::Addr(nyx::samp::r5::kRefChat));
    if (!chat) return;
    for (const auto& line : lines) {
        urmem::call_function<urmem::calling_convention::thiscall, void>(
            nyx::samp::Addr(nyx::samp::r5::kCChat_AddMessage), chat,
            static_cast<unsigned long>(line.color), line.text.c_str());
        NYX_LOGI("[chat] %s", line.text.c_str());
    }
#else
    (void)0;  // non-Windows stub (unit harness never calls this)
#endif
}

}  // namespace chatui
