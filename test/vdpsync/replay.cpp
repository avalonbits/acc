// Replays what acc's VDP functions send into the VDP's own firmware, built
// for the host, and checks that every command is exactly as long as the
// VDP takes it to be.
//
// Each line of the input is "name: hex bytes", as test/agonlib/capture.h
// writes them. After each command's bytes this sends a general poll, VDU
// 23,0,&80,n, with n a number of its own, and waits for the VDP to answer
// it with n. A command one byte short leaves the VDP waiting for that byte
// and it takes the poll's first byte as an argument; one byte long, and the
// extra byte is read as a command of its own before the poll. Either way
// the answer that comes back, if one does, is not n.
//
// After a failure the link is brought back into step with a run of zero
// bytes -- VDU 0 does nothing -- and a poll of its own, so that one bad
// command is reported once and the rest are still checked.
//
// The link is the emulator's: rust_glue.cpp in fab-agon-emulator exposes
// the eZ80 serial port as a pair of queues, and this drives them as MOS
// would.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

extern "C" void vdp_setup();
extern "C" void vdp_shutdown();
extern "C" void z80_send_to_vdp(uint8_t b);
extern "C" bool z80_recv_from_vdp(uint8_t *out);
extern "C" void setVdpDebugLogging(bool state);
extern "C" void signal_vblank();

static std::atomic<bool> pumping{false};
static std::thread pump;

// The general poll's answer carrying n, or -1 when none came in time. Other
// packets -- a mode change, a pixel read -- are passed over.
static int await_poll(int timeout_ms)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    uint8_t b;

    while (std::chrono::steady_clock::now() < deadline) {
        if (!z80_recv_from_vdp(&b)) {
            std::this_thread::sleep_for(std::chrono::microseconds(200));
            continue;
        }
        if (!(b & 0x80))
            continue;
        int type = b & 0x7f;
        uint8_t len = 0;
        bool got = false;
        while (std::chrono::steady_clock::now() < deadline) {
            if (z80_recv_from_vdp(&len)) { got = true; break; }
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
        if (!got)
            break;
        std::vector<uint8_t> data;
        while (data.size() < len && std::chrono::steady_clock::now() < deadline) {
            if (z80_recv_from_vdp(&b))
                data.push_back(b);
            else
                std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
        // The poll's number first; a VDP that reports its version sends
        // that after it.
        if (type == 0 && data.size() >= 1)
            return data[0];
    }

    return -1;
}

static int poll_number = 0;

static bool in_step(int timeout_ms)
{
    int n = (poll_number = (poll_number + 1) % 100) + 1;

    z80_send_to_vdp(23);
    z80_send_to_vdp(0);
    z80_send_to_vdp(0x80);
    z80_send_to_vdp((uint8_t) n);

    for (;;) {
        int got = await_poll(timeout_ms);

        if (got == n)
            return true;
        if (got < 0)
            return false;
        // A poll answered from before a resync: look again.
    }
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <captured.txt>\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) {
        perror(argv[1]);
        return 2;
    }

    setVdpDebugLogging(getenv("VDP_DEBUG") != nullptr);
    pumping = true;
    pump = std::thread([]() {
        while (pumping) {
            signal_vblank();
            std::this_thread::sleep_for(std::chrono::microseconds(16667));
        }
    });
    vdp_setup();
    if (!in_step(10000)) {
        printf("  FAIL the VDP did not answer a general poll after booting\n");
        fflush(stdout);
        _exit(1);
    }

    char line[8192];
    int pass = 0, fail = 0;

    while (fgets(line, sizeof line, f)) {
        char *colon = strstr(line, "): ");
        if (!colon) {
            colon = strstr(line, "):");
            if (!colon)
                continue;
        }
        std::string name(line, colon + 1 - line);
        char *p = colon + 2;
        unsigned v;
        int used;

        while (sscanf(p, " %x%n", &v, &used) == 1) {
            z80_send_to_vdp((uint8_t) v);
            p += used;
        }
        if (in_step(5000)) {
            pass++;
            continue;
        }
        printf("  FAIL %s: the VDP was out of step after it\n", name.c_str());
        fail++;
        for (int i = 0; i < 64; i++)
            z80_send_to_vdp(0);
        in_step(5000);
    }

    printf("  %d commands in step, %d out\n", pass, fail);
    fflush(stdout);

    // The firmware's own tasks do not all stop at vdp_shutdown, and a
    // process that returns with them running aborts: leave directly.
    _exit(fail ? 1 : 0);
}
