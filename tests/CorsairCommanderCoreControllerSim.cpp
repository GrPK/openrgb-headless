/*---------------------------------------------------------*\
| CorsairCommanderCoreControllerSim.cpp                     |
|                                                           |
|   Runs the Commander Core controller against a simulated  |
|   device behind the hidapi calls: cooling polling, duty   |
|   writes on change only, no interleaving with the         |
|   lighting stream, no data while the device is silent.    |
|                                                           |
|   Build from the repository root with the include flags   |
|   of the generated Makefile (INCPATH) and link:           |
|     this file, CorsairCommanderCoreController.cpp,        |
|     CorsairDeviceGuard.cpp and the interop/DeviceGuard*   |
|     sources. Takes about 30 seconds to run.               |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <map>
#include <vector>
#include <hidapi.h>
#include "CorsairCommanderCoreController.h"

using namespace std::chrono_literals;
typedef std::vector<unsigned char> Bytes;

struct Cmd { unsigned char cmd, handle; Bytes data; };

static std::mutex g_mutex;
static std::condition_variable g_cv;
static std::deque<Bytes> g_replies;
static std::vector<Cmd> g_log;
static std::map<int, Bytes> g_open;      // handle -> endpoint
static Bytes g_last_table;               // last software duty table written
static int g_table_writes = 0;
static std::atomic<bool> g_mute_cooling{false};
static std::atomic<int> g_interleave_violations{0};
static std::atomic<bool> g_cooling_txn_open{false};
static int g_packet = 97;

static Bytes Reply(unsigned char cmd, const Bytes& payload)
{
    Bytes r(g_packet - 1, 0);   // reports come back without the report id byte
    r[0] = 0x00; r[1] = cmd; r[2] = 0x00;
    for(size_t i = 0; i < payload.size() && 3 + i < r.size(); i++) r[3 + i] = payload[i];
    return r;
}

static Bytes ReadPayload(unsigned char ep)
{
    switch(ep)
    {
    case 0x20: { Bytes p = {0x0f, 0x00, 0x07}; for(int i = 0; i < 7; i++) { p.push_back(2); p.push_back(0); p.push_back(i == 0 ? 29 : 0); p.push_back(0);} return p; }
    case 0x17: return {0x06, 0x00, 0x07, 0x35, 0x09, 0x96, 0x03, 0x87, 0x03, 0xf5, 0x01, 0x50, 0x04, 0x20, 0x07, 0x68, 0x00};
    case 0x1a: return {0x09, 0x00, 0x07, 0x07, 0x07, 0x07, 0x01, 0x01, 0x01, 0x01};
    case 0x21: return {0x10, 0x00, 0x02, 0x00, 0x7b, 0x00, 0x01, 0x00, 0x00};
    }
    return {0xff, 0xff};
}

extern "C" {
int hid_write(hid_device*, const unsigned char* data, size_t len)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if(len < 4 || data[1] != 0x08) return -1;
    Cmd c{data[2], data[3], Bytes(data + 4, data + len)};
    g_log.push_back(c);
    bool cooling = (c.handle == 0x01 && c.cmd != 0x01 && c.cmd != 0x02) ;
    if(cooling && g_mute_cooling) return (int)len;   // no reply at all
    Bytes payload;
    switch(c.cmd)
    {
    case 0x0d: g_open[c.handle] = Bytes(c.data.begin(), c.data.begin() + 2); if(c.handle == 1) { if(g_cooling_txn_open) g_interleave_violations++; g_cooling_txn_open = true; } break;
    case 0x08: payload = ReadPayload(g_open[c.handle][0]); break;
    case 0x02: payload = {1, 2, 3}; break;
    case 0x06:
        if(g_open[c.handle][0] == 0x18) { { int length = c.data[0] + (c.data[1] << 8); g_last_table.assign(c.data.begin() + 6, c.data.begin() + 6 + length - 2); } g_table_writes++; }
        break;
    case 0x05: if(c.handle == 1) g_cooling_txn_open = false; break;
    }
    // any non-cooling command inside an open cooling transaction means two transactions interleaved
    if(g_cooling_txn_open && c.handle != 1 && c.cmd != 0x0d) g_interleave_violations++;
    g_replies.push_back(Reply(c.cmd, payload));
    g_cv.notify_all();
    return (int)len;
}
int hid_read_timeout(hid_device*, unsigned char* data, size_t len, int ms)
{
    std::unique_lock<std::mutex> lock(g_mutex);
    if(!g_cv.wait_for(lock, std::chrono::milliseconds(ms), []{ return !g_replies.empty(); })) return 0;
    Bytes r = g_replies.front(); g_replies.pop_front();
    size_t n = std::min(len, r.size());
    memcpy(data, r.data(), n);
    return (int)n;
}
int hid_read(hid_device* d, unsigned char* data, size_t len) { return hid_read_timeout(d, data, len, 5000); }
void hid_close(hid_device*) {}
}

#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)
static int fails = 0;

static void Show(const char* what, const Bytes& b) { printf("%s:", what); for(auto x : b) printf(" %02x", x); printf("\n"); }

int main()
{
    CorsairCommanderCoreController* ctrl = new CorsairCommanderCoreController((hid_device*)1, "sim", 0x0C1C, "Corsair Commander Core");

    std::vector<unsigned char> st;
    CHECK(ctrl->NexusCoolingGetStatus(st));
    CHECK(st.size() > 7 && st[1] == 0);          // no data on first call
    std::this_thread::sleep_for(1500ms);
    CHECK(ctrl->NexusCoolingGetStatus(st));
    Show("status", st);
    CHECK(st[0] == 1 && st[1] == 1);              // valid
    CHECK(st[2] == 0x1c && st[3] == 0x0c);        // pid
    CHECK(st[6] == 7);                            // channels
    CHECK(st[7] == 1 && st[8] == 0x35 && st[9] == 0x09 && st[10] == 0xff);   // ch0 connected, 2357 rpm, no duty yet

    // duties: written once, then only on change
    Bytes d1 = {1, 5, 0, 75, 1, 100, 2, 100, 3, 100, 4, 22};
    CHECK(ctrl->NexusCoolingSetDuties(d1.data(), (unsigned)d1.size()));
    std::this_thread::sleep_for(700ms);
    CHECK(g_table_writes == 1);
    Show("table", g_last_table);
    CHECK(g_last_table == Bytes({5, 0, 0, 75, 0, 1, 0, 100, 0, 2, 0, 100, 0, 3, 0, 100, 0, 4, 0, 22, 0}));
    CHECK(ctrl->NexusCoolingSetDuties(d1.data(), (unsigned)d1.size()));
    std::this_thread::sleep_for(700ms);
    CHECK(g_table_writes == 1);
    Bytes d2 = {1, 5, 0, 80, 1, 100, 2, 100, 3, 100, 4, 22};
    CHECK(ctrl->NexusCoolingSetDuties(d2.data(), (unsigned)d2.size()));
    std::this_thread::sleep_for(700ms);
    CHECK(g_table_writes == 2);
    ctrl->NexusCoolingGetStatus(st);
    CHECK(st[10] == 80);                          // applied duty reported
    Bytes bad = {1, 1, 9, 50};
    CHECK(!ctrl->NexusCoolingSetDuties(bad.data(), (unsigned)bad.size()));

    // every cooling transaction used handle 1 and the vendor sequence
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        int txn = 0;
        for(size_t i = 0; i + 3 < g_log.size(); i++)
        {
            if(g_log[i].cmd == 0x0d && g_log[i].handle == 1)
            {
                txn++;
                CHECK(g_log[i + 1].cmd == 0x09 && g_log[i + 1].handle == 1);
                CHECK((g_log[i + 2].cmd == 0x08 || g_log[i + 2].cmd == 0x06) && g_log[i + 2].handle == 1);
                CHECK(g_log[i + 3].cmd == 0x05 && g_log[i + 3].handle == 1 && g_log[i + 3].data[0] == 1);
            }
        }
        printf("cooling transactions: %d\n", txn);
        CHECK(txn >= 6);
    }

    // lighting at ~30 Hz alongside cooling: serialized, never interleaved
    std::atomic<bool> run{true};
    std::atomic<int> frames{0};
    std::thread lighting([&]{
        std::vector<RGBColor> colors(29 + 6 * 34, ToRGBColor(10, 20, 30));
        std::vector<zone> zones(7);
        zones[0].leds_count = 29;
        for(int i = 1; i < 7; i++) zones[i].leds_count = 34;
        while(run) { ctrl->SetDirectColor(colors, zones); frames++; std::this_thread::sleep_for(33ms); }
    });
    std::this_thread::sleep_for(3500ms);
    run = false; lighting.join();
    printf("lighting calls: %d, interleave violations: %d\n", frames.load(), g_interleave_violations.load());
    CHECK(g_interleave_violations == 0);

    // device stops answering cooling: status turns to no data, set duties still returns at once
    g_mute_cooling = true;
    std::this_thread::sleep_for(7s);
    ctrl->NexusCoolingGetStatus(st);
    CHECK(st[1] == 0);
    auto t0 = std::chrono::steady_clock::now();
    CHECK(ctrl->NexusCoolingSetDuties(d1.data(), (unsigned)d1.size()));
    CHECK(std::chrono::steady_clock::now() - t0 < 50ms);
    g_mute_cooling = false;
    std::this_thread::sleep_for(11s);              // backoff expires, polling resumes
    ctrl->NexusCoolingGetStatus(st);
    CHECK(st[1] == 1);

    t0 = std::chrono::steady_clock::now();
    delete ctrl;
    printf("destructor %lld ms\n", (long long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count());
    printf(fails ? "FAILED %d\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
