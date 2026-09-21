// dut_emu: host-side DUT emulator for virtual-bus tests.
// Links the REAL src/bms_protocol.cpp (parser + dispatcher + tracker) and
// speaks JBD over a serial PTY at 9600 8N1, exactly like main.cpp's loop:
//   feed bytes -> note_poll -> reply_for -> write reply.
// LED state (green/red) is reported on stderr on every change (250 ms grid).
// Usage: dut_emu /dev/pts/N  (or a socat-created /tmp/ttyDUT symlink)
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include "bms_protocol.h"

static int open_tty(const char *dev) {
  int fd = open(dev, O_RDWR | O_NOCTTY);
  if (fd < 0) { printf("open %s: %s\n", dev, strerror(errno)); return -1; }
  termios t{};
  if (tcgetattr(fd, &t) != 0) { printf("tcgetattr: %s\n", strerror(errno)); return -1; }
  cfmakeraw(&t);
  cfsetspeed(&t, B9600);
  t.c_cc[VMIN] = 0; t.c_cc[VTIME] = 1;  // 100 ms read timeout
  if (tcsetattr(fd, TCSANOW, &t) != 0) { printf("tcsetattr: %s\n", strerror(errno)); return -1; }
  return fd;
}

int main(int argc, char **argv) {
  if (argc != 2) { printf("usage: dut_emu TTY\n"); return 2; }
  int fd = open_tty(argv[1]);
  if (fd < 0) return 1;
  printf("DUT-EMU listening on %s @9600\n", argv[1]);
  fflush(stdout);

  JbdParser parser;
  PollTracker tracker;
  JbdFrame f;
  bool led = false;  // false = red, true = green
  fprintf(stderr, "LED RED (boot)\n");

  // millis-ish clock for the tracker (monotonic ms since start)
  unsigned long start_ms = 0;
  auto now_ms = [&]() -> unsigned long {
    // use select timeout accumulation instead of clock_gettime for portability
    return start_ms;
  };
  (void)now_ms;

  unsigned long ms = 0;  // advances 50 ms per loop iteration
  unsigned long last_eval = 0;
  uint8_t byte;
  while (true) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    timeval tv{0, 50000};  // 50 ms tick
    int r = select(fd + 1, &rfds, nullptr, nullptr, &tv);
    ms += 50;
    if (r > 0 && FD_ISSET(fd, &rfds)) {
      ssize_t n = read(fd, &byte, 1);
      if (n == 1) {
        if (parser.feed(byte, f)) {
          tracker.note_poll(ms);
          size_t rl = 0;
          const uint8_t *reply = reply_for(f.reg, f.is_write, rl);
          if (reply) {
            // half-duplex guard like the firmware (scaled down, PTY is instant)
            usleep(2000);
            ssize_t w = write(fd, reply, rl);
            if (w != (ssize_t)rl) fprintf(stderr, "short write\n");
          }
        }
      }
    }
    if (ms - last_eval >= 250) {
      last_eval = ms;
      bool on = tracker.active(ms);
      if (on != led) {
        led = on;
        fprintf(stderr, "LED %s @%lums\n", on ? "GREEN" : "RED", ms);
      }
    }
  }
  return 0;
}
