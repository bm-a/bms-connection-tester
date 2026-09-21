// Host stub: WebServer (test_web ONLY). Registers routes like the real
// server; the static request() driver injects a fake HTTP exchange so tests
// execute the real handlers from web_ui.cpp and inspect status/headers/body.
#pragma once
#include "Arduino.h"

enum { HTTP_ANY = 0, HTTP_GET = 1, HTTP_POST = 2 };
typedef void (*HandlerFn)();

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
  void onNotFound(HandlerFn h) { notFound() = h; }
  void begin() {}
  void handleClient() {}

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
    Resp resp;
  };
  static std::map<Key, HandlerFn> &routes() {
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
