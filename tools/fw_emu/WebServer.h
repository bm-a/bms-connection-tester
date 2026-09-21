// POSIX-socket WebServer shim for the firmware emulation harness (fw_emu).
// Same handler-facing API as Arduino-ESP32 WebServer (and the test_web
// stub) so the REAL web_ui.cpp compiles unchanged and serves real HTTP:
// routes, upload streaming (incl. multipart file upload), headers, args.
// Emu-only: NEVER in firmware, never in unit tests. Control plane (virtual
// time, link state) lives in emu_main.cpp's side server, sharing the same
// stub globals (g_mock_millis, g_wifi_status, Update, ...).
#pragma once
#include "Arduino.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <map>
#include <sstream>
#include <string>
#include <vector>

enum { HTTP_ANY = 0, HTTP_GET = 1, HTTP_POST = 2 };
typedef void (*HandlerFn)();

enum { UPLOAD_FILE_START = 1, UPLOAD_FILE_WRITE = 2, UPLOAD_FILE_END = 3 };
struct HTTPUpload {
  int status = 0;
  String filename;
  String name;
  const uint8_t *buf = nullptr;
  size_t currentSize = 0;
  size_t totalSize = 0;
};

namespace emu_detail {
inline std::string lower(std::string s) {
  for (char &c : s) c = (char)tolower(c);
  return s;
}
inline std::string trim(const std::string &s) {
  size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
  return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
}  // namespace emu_detail

class WebServer {
 public:
  struct Resp {
    int code = 0;
    std::string type = "text/plain";
    std::string body;
    std::map<std::string, std::string> headers;
  };

  explicit WebServer(int) {
    const char *p = getenv("FW_EMU_PORT");
    port_ = p ? atoi(p) : 8080;
  }
  void collectHeaders(const char **keys, size_t n) {
    wanted_.clear();
    for (size_t i = 0; i < n && keys && keys[i]; i++)
      wanted_.push_back(emu_detail::lower(keys[i]));
  }
  void on(const char *path, HandlerFn h) { routes_[Key(path, HTTP_ANY)] = h; }
  void on(const char *path, int method, HandlerFn h) {
    routes_[Key(path, method)] = h;
  }
  void on(const char *path, int method, HandlerFn h, HandlerFn uh) {
    routes_[Key(path, method)] = h;
    uploads_[Key(path, method)] = uh;
  }
  void onNotFound(HandlerFn h) { notFound_ = h; }
  void begin() {
    if (listen_ >= 0) return;
    listen_ = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(listen_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons((uint16_t)port_);
    if (bind(listen_, (sockaddr *)&a, sizeof(a)) || listen(listen_, 4)) {
      close(listen_);
      listen_ = -1;
      return;
    }
    up_ = true;
  }
  void stop() {
    up_ = false;
    if (listen_ >= 0) {
      close(listen_);
      listen_ = -1;
    }
  }
  // Serve at most one pending connection (non-blocking poll; the emu main
  // loop and the virtual-time grain loop call this hot, so zero timeout).
  void handleClient() {
    if (listen_ < 0) return;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(listen_, &fds);
    timeval tv{0, 0};
    if (select(listen_ + 1, &fds, nullptr, nullptr, &tv) <= 0) return;
    int c = accept(listen_, nullptr, nullptr);
    if (c < 0) return;
    serve(c);
    close(c);
  }
  HTTPUpload &upload() { return cur().upload; }

  void send(int code) { respond(code, "", ""); }
  void send(int code, const char *type, const String &content) {
    respond(code, type ? type : "", content.stl());
  }
  void send_P(int code, const char *type, const char *content) {
    respond(code, type ? type : "", content ? content : "");
  }
  void sendHeader(const char *name, const String &value) {
    cur().resp.headers[name ? name : ""] = value.stl();
  }
  void sendHeader(const char *name, const char *value) {
    cur().resp.headers[name ? name : ""] = value ? value : "";
  }
  bool hasHeader(const String &name) {
    return cur().req_headers.count(name.stl()) > 0;
  }
  String header(const String &name) {
    std::string want = emu_detail::lower(name.stl());
    for (auto &kv : cur().req_headers) {
      if (emu_detail::lower(kv.first) == want) return String(kv.second);
    }
    return String();
  }
  String arg(const String &name) {
    auto it = cur().args.find(name.stl());
    return it == cur().args.end() ? String() : String(it->second);
  }
  String uri() { return String(cur().path.c_str()); }

 private:
  struct Key {
    std::string path;
    int method;
    Key() {}
    Key(const std::string &p, int m) : path(p), method(m) {}
    bool operator<(const Key &o) const {
      return path < o.path || (path == o.path && method < o.method);
    }
  };
  struct Ctx {
    int method = 0;
    std::string path;
    std::map<std::string, std::string> req_headers;
    std::map<std::string, std::string> args;
    HTTPUpload upload;
    Resp resp;
  };

  int port_ = 8080;
  int listen_ = -1;
  bool up_ = false;
  std::vector<std::string> wanted_;
  std::map<Key, HandlerFn> routes_;
  std::map<Key, HandlerFn> uploads_;
  HandlerFn notFound_ = nullptr;

  static Ctx &cur_ref() {
    static thread_local Ctx c;
    return c;
  }
  Ctx &cur() { return cur_ref(); }
  void respond(int code, const std::string &type, const std::string &body) {
    cur().resp.code = code;
    if (!type.empty()) cur().resp.type = type;
    cur().resp.body = body;
  }

  static std::string read_all(int fd, size_t max_bytes = 4 << 20) {
    std::string out;
    char buf[8192];
    // Read head first (headers), then the declared body.
    while (out.size() < max_bytes) {
      fd_set fds;
      FD_ZERO(&fds);
      FD_SET(fd, &fds);
      timeval tv{2, 0};
      if (select(fd + 1, &fds, nullptr, nullptr, &tv) <= 0) break;
      ssize_t n = recv(fd, buf, sizeof(buf), 0);
      if (n <= 0) break;
      out.append(buf, (size_t)n);
      size_t he = out.find("\r\n\r\n");
      if (he != std::string::npos) {
        // Parse Content-Length; keep reading until the full body arrived.
        size_t cl = 0;
        std::string head = emu_detail::lower(out.substr(0, he));
        size_t cp = head.find("content-length:");
        if (cp != std::string::npos)
          cl = (size_t)atol(head.c_str() + cp + 15);
        if (out.size() >= he + 4 + cl) break;
      }
    }
    return out;
  }

  void serve(int fd) {
    cur() = Ctx();
    std::string raw = read_all(fd);
    if (raw.empty()) return;
    std::istringstream head(raw.substr(0, raw.find("\r\n")));
    std::string method, target, version;
    head >> method >> target >> version;
    cur().method = (method == "POST") ? HTTP_POST : HTTP_GET;
    size_t qm = target.find('?');
    cur().path = (qm == std::string::npos) ? target : target.substr(0, qm);
    size_t he = raw.find("\r\n\r\n");
    std::string hblock = he == std::string::npos ? "" : raw.substr(0, he);
    std::string body = he == std::string::npos ? "" : raw.substr(he + 4);
    // Headers (first line is the request line; skip it).
    size_t pos = hblock.find("\r\n");
    std::string content_type;
    while (pos != std::string::npos) {
      size_t eol = hblock.find("\r\n", pos + 2);
      std::string line = hblock.substr(pos + 2, eol == std::string::npos
                                                      ? std::string::npos
                                                      : eol - pos - 2);
      size_t colon = line.find(':');
      if (colon != std::string::npos) {
        std::string k = emu_detail::trim(line.substr(0, colon));
        std::string v = emu_detail::trim(line.substr(colon + 1));
        cur().req_headers[k] = v;
        if (emu_detail::lower(k) == "content-type") content_type = v;
      }
      if (eol == std::string::npos) break;
      pos = eol;
    }
    if (cur().method == HTTP_POST) {
      if (content_type.find("multipart/form-data") != std::string::npos) {
        size_t bp = content_type.find("boundary=");
        std::string b =
            bp == std::string::npos ? "" : content_type.substr(bp + 9);
        drive_multipart(body, b);
      } else {
        cur().args["plain"] = body;
      }
    }
    auto it = routes_.find(Key(cur().path, cur().method));
    if (it == routes_.end()) it = routes_.find(Key(cur().path, HTTP_ANY));
    if (it != routes_.end()) {
      it->second();
    } else if (notFound_) {
      notFound_();
    } else {
      cur().resp.code = 404;
    }
    std::string status = status_text(cur().resp.code);
    std::string out = "HTTP/1.1 " + std::to_string(cur().resp.code) +
                      " " + status + "\r\nContent-Type: " + cur().resp.type +
                      "\r\nContent-Length: " +
                      std::to_string(cur().resp.body.size()) +
                      "\r\nConnection: close\r\n";
    for (auto &kv : cur().resp.headers)
      out += kv.first + ": " + kv.second + "\r\n";
    out += "\r\n" + cur().resp.body;
    size_t off = 0;
    while (off < out.size()) {
      ssize_t n = ::send(fd, out.data() + off, out.size() - off, 0);
      if (n <= 0) break;
      off += (size_t)n;
    }
    cur() = Ctx();
  }

  static std::string status_text(int c) {
    switch (c) {
      case 200: return "OK";
      case 302: return "Found";
      case 400: return "Bad Request";
      case 403: return "Forbidden";
      case 404: return "Not Found";
      case 500: return "Internal Error";
      default: return "OK";
    }
  }

  // Drive the registered upload handler through field/file parts exactly
  // like Arduino-ESP32 (START once per file part, WRITE chunks, END).
  void drive_multipart(const std::string &body, const std::string &boundary) {
    if (boundary.empty()) return;
    std::string delim = "--" + boundary;
    size_t pos = 0;
    auto uh = uploads_.find(Key(cur().path, HTTP_POST));
    HandlerFn fn = (uh == uploads_.end()) ? nullptr : uh->second;
    if (!fn) return;
    static std::string hold;  // chunk backing (upload.buf points into it)
    while (true) {
      size_t ds = body.find(delim, pos);
      if (ds == std::string::npos) break;
      size_t hs = ds + delim.size();
      if (body.compare(hs, 2, "--") == 0) break;  // closing delimiter
      size_t hse = body.find("\r\n\r\n", hs);
      if (hse == std::string::npos) break;
      std::string ph = body.substr(hs, hse - hs);
      size_t de = body.find(delim, hse + 4);
      if (de == std::string::npos) break;
      std::string data = body.substr(hse + 4, de - hse - 4);
      while (data.size() >= 2 &&
             data.compare(data.size() - 2, 2, "\r\n") == 0)
        data.resize(data.size() - 2);
      std::string name, filename;
      size_t np = ph.find("name=\"");
      if (np != std::string::npos) {
        size_t ne = ph.find('"', np + 6);
        name = ph.substr(np + 6, ne - np - 6);
      }
      size_t fp = ph.find("filename=\"");
      if (fp != std::string::npos) {
        size_t fe = ph.find('"', fp + 10);
        filename = ph.substr(fp + 10, fe - fp - 10);
      }
      if (filename.empty()) {
        cur().upload.status = UPLOAD_FILE_WRITE;
        cur().upload.filename = String("");
        cur().upload.name = String(name);
        hold = data;
        cur().upload.buf = (const uint8_t *)hold.data();
        cur().upload.currentSize = hold.size();
        cur().upload.totalSize = hold.size();
        fn();
      } else {
        cur().upload.status = UPLOAD_FILE_START;
        cur().upload.filename = String(filename);
        cur().upload.name = String(name);
        cur().upload.buf = nullptr;
        cur().upload.currentSize = 0;
        cur().upload.totalSize = data.size();
        fn();
        for (size_t off = 0; off < data.size(); off += 4096) {
          size_t n = data.size() - off;
          if (n > 4096) n = 4096;
          hold.assign(data.data() + off, n);
          cur().upload.status = UPLOAD_FILE_WRITE;
          cur().upload.buf = (const uint8_t *)hold.data();
          cur().upload.currentSize = n;
          fn();
        }
        cur().upload.status = UPLOAD_FILE_END;
        cur().upload.buf = nullptr;
        cur().upload.currentSize = 0;
        fn();
      }
      pos = de;
    }
  }
};
