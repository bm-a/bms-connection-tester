// Host stub: WebServer (test_web ONLY). Registers routes like the real
// server; the static request() driver injects a fake HTTP exchange so tests
// execute the real handlers from web_ui.cpp and inspect status/headers/body.
#pragma once
#include "Arduino.h"

enum { HTTP_ANY = 0, HTTP_GET = 1, HTTP_POST = 2 };
typedef void (*HandlerFn)();

// Upload states (mirror Arduino-ESP32 HTTPUpload).
enum { UPLOAD_FILE_START = 1, UPLOAD_FILE_WRITE = 2, UPLOAD_FILE_END = 3 };
struct HTTPUpload {
  int status = 0;
  String filename;
  String name;
  const uint8_t *buf = nullptr;
  size_t currentSize = 0;
  size_t totalSize = 0;
};

class WebServer {
 public:
  struct Resp {
    int code = 0;
    std::string type;
    std::string body;
    std::map<std::string, std::string> headers;
  };

  explicit WebServer(int) {}
  void collectHeaders(const char **, size_t) {}
  void on(const char *path, HandlerFn h) { routes()[Key(path, HTTP_ANY)] = h; }
  void on(const char *path, int method, HandlerFn h) {
    routes()[Key(path, method)] = h;
  }
  void on(const char *path, int method, HandlerFn h, HandlerFn uh) {
    routes()[Key(path, method)] = h;
    uploads()[Key(path, method)] = uh;
  }
  void onNotFound(HandlerFn h) { notFound() = h; }
  void begin() {}
  void stop() {}
  void handleClient() {}
  HTTPUpload &upload() { return cur().upload; }

  void send(int code) { respond(code, "", ""); }
  void send(int code, const char *type, const String &content) {
    respond(code, type ? type : "", content.stl());
  }
  void send_P(int code, const char *type, const char *content) {
    respond(code, type ? type : "", content ? content : "");
  }
  void sendHeader(const char *name, const char *value) {
    cur().resp.headers[name ? name : ""] = value ? value : "";
  }
  void sendHeader(const char *name, const String &value) {
    cur().resp.headers[name ? name : ""] = value.stl();
  }
  bool hasHeader(const String &name) {
    return cur().req_headers.count(name.stl()) > 0;
  }
  String header(const String &name) {
    auto it = cur().req_headers.find(name.stl());
    return it == cur().req_headers.end() ? String() : String(it->second);
  }
  String arg(const String &name) {
    auto it = cur().args.find(name.stl());
    return it == cur().args.end() ? String() : String(it->second);
  }

  // ---- test driver ----
  static Resp request(const std::string &method, const std::string &path,
                      const std::map<std::string, std::string> &headers,
                      const std::string &body,
                      const std::map<std::string, std::string> &args) {
    Ctx c;
    c.method = (method == "POST") ? HTTP_POST : HTTP_GET;
    c.path = path;
    c.req_headers = headers;
    c.args = args;
    if (c.method == HTTP_POST && args.count("plain") == 0 && !body.empty())
      c.args["plain"] = body;
    cur() = c;
    auto &rt = routes();
    auto it = rt.find(Key(path, cur().method));
    if (it == rt.end()) it = rt.find(Key(path, HTTP_ANY));
    if (it != rt.end()) {
      it->second();
    } else if (notFound()) {
      notFound()();
    } else {
      cur().resp.code = 404;
    }
    Resp r = cur().resp;
    cur() = Ctx();
    return r;
  }
  static Resp get(const std::string &path,
                  const std::map<std::string, std::string> &headers =
                      std::map<std::string, std::string>()) {
    return request("GET", path, headers, "", std::map<std::string, std::string>());
  }
  static Resp post(const std::string &path, const std::string &body,
                   const std::map<std::string, std::string> &headers =
                       std::map<std::string, std::string>()) {
    std::map<std::string, std::string> a;
    a["plain"] = body;
    return request("POST", path, headers, body, a);
  }
  // Simulate a multipart firmware upload: an optional "pass" form-field
  // part first (like the real /update form), then START + WRITE chunks +
  // END through the registered upload handler, then the POST done-handler.
  static Resp upload(const std::string &path, const std::string &filename,
                     const std::string &data,
                     const std::map<std::string, std::string> &headers =
                         std::map<std::string, std::string>(),
                     const std::map<std::string, std::string> &args =
                         std::map<std::string, std::string>(),
                     bool pass_first = true) {
    Ctx c;
    c.method = HTTP_POST;
    c.path = path;
    c.req_headers = headers;
    c.args = args;
    cur() = c;
    auto uh = uploads().find(Key(path, HTTP_POST));
    auto send_pass_field = [&]() {
      auto fp = args.find("pass");
      if (fp != args.end()) {
        // Form-field part (empty filename), as the browser sends it.
        cur().upload.status = UPLOAD_FILE_WRITE;
        cur().upload.filename = String("");
        cur().upload.name = String("pass");
        cur().upload.buf = (const uint8_t *)fp->second.data();
        cur().upload.currentSize = fp->second.size();
        cur().upload.totalSize = fp->second.size();
        uh->second();
      }
    };
    if (uh != uploads().end() && uh->second) {
      if (pass_first) send_pass_field();
      cur().upload.status = UPLOAD_FILE_START;
      cur().upload.filename = String(filename);
      cur().upload.buf = nullptr;
      cur().upload.currentSize = 0;
      cur().upload.totalSize = data.size();
      uh->second();
      for (size_t off = 0; off < data.size(); off += 512) {
        size_t n = data.size() - off;
        if (n > 512) n = 512;
        cur().upload.status = UPLOAD_FILE_WRITE;
        cur().upload.buf = (const uint8_t *)(data.data() + off);
        cur().upload.currentSize = n;
        uh->second();
      }
      cur().upload.status = UPLOAD_FILE_END;
      cur().upload.buf = nullptr;
      cur().upload.currentSize = 0;
      uh->second();
      if (!pass_first) send_pass_field();
    }
    auto &rt = routes();
    auto it = rt.find(Key(path, HTTP_POST));
    if (it == rt.end()) it = rt.find(Key(path, HTTP_ANY));
    if (it != rt.end()) {
      it->second();
    } else if (notFound()) {
      notFound()();
    } else {
      cur().resp.code = 404;
    }
    Resp r = cur().resp;
    cur() = Ctx();
    return r;
  }

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
  static std::map<Key, HandlerFn> &routes() {
    static std::map<Key, HandlerFn> m;
    return m;
  }
  static std::map<Key, HandlerFn> &uploads() {
    static std::map<Key, HandlerFn> m;
    return m;
  }
  static HandlerFn &notFound() {
    static HandlerFn h = nullptr;
    return h;
  }
  static Ctx &cur() {
    static Ctx c;
    return c;
  }
  static void respond(int code, const std::string &type, const std::string &body) {
    cur().resp.code = code;
    cur().resp.type = type;
    cur().resp.body = body;
  }
};
