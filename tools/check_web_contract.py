#!/usr/bin/env python3
"""Web contract check: dashboard JS <-> web_ui.cpp consistency.
Fails (exit 1) on any mismatch so CI catches website/API drift:
 1. Every fetch() endpoint in the dashboard JS has a server.on() route.
 2. Every getElementById() has a matching id= in the served HTML.
 3. Every cfg key the JS reads from /api/state is emitted by handle_state().
 4. Every key the JS POSTs is consumed (has/jnum/jstr) by that endpoint's handler.
Usage: python3 tools/check_web_contract.py
"""
import re
import sys

SRC = "src/web_ui.cpp"


def main() -> int:
    src = open(SRC).read()
    fails = []

    def fail(msg):
        fails.append(msg)
        print("CONTRACT-FAIL:", msg)

    # ---- routes ----
    routes = {}
    for m in re.finditer(r'server\.on\("([^"]+)"(?:,\s*HTTP_(GET|POST))?,\s*(\w+)', src):
        path, method, handler = m.group(1), m.group(2) or "ANY", m.group(3)
        routes[(path, method)] = handler
    print(f"routes: {sorted(p for p, _ in routes)}")

    # ---- dashboard JS + HTML ----
    js = src.split("<script>", 1)[1].split("</script>", 1)[0]
    html = src.split("PAGE_DASH[] PROGMEM", 1)[1].split("<script>", 1)[0]

    # 1. endpoints
    for ep in sorted(set(re.findall(r"fetch\(['\"]([^'\"]+)['\"]", js))):
        ok = any(p == ep for (p, _) in routes)
        print(f"endpoint {ep}: {'OK' if ok else 'MISSING ROUTE'}")
        if not ok:
            fail(f"JS fetches {ep} with no server.on route")

    # Login wall is gone since v2.3.1 (WPA2 is the gate); sensitive actions
    # carry the admin password per request. Manual firmware upload page stays.
    for token in ['href=/update', 'action=/update']:
        if token not in src:
            fail(f"missing {token}")
    for dead in ['action=/login', 'href=/logout', 'PAGE_LOGIN',
                 'handle_login', 'handle_logout']:
        if dead in src:
            fail(f"login remnant still present: {dead}")

    # 2. element ids
    html_ids = set(re.findall(r"id=([A-Za-z_]+)", html))
    for eid in sorted(set(re.findall(r"getElementById\(['\"]([^'\"]+)['\"]", js))):
        if eid == "lbl":
            # Dynamic tile ids lbl0-7 (built in JS loops, inputs in #labels).
            ok = "labels" in html_ids
            print(f"element #lblN: {'OK (dynamic)' if ok else 'MISSING'}")
            if not ok:
                fail("JS builds #lblN inputs with no id=labels container")
            continue
        ok = eid in html_ids
        print(f"element #{eid}: {'OK' if ok else 'MISSING id='}")
        if not ok:
            fail(f"JS uses #{eid} with no id= in HTML")

    # 3. state keys: JS list literal vs handle_state emission
    state_fn = src.split("static void handle_state()", 1)[1].split(
        "static void handle_relay()", 1)[0]
    emitted = set(re.findall(r'"\\?"?([a-z_]+)\\?"?:', state_fn))
    # cfg keys are built as \",\"key\": so also catch them
    emitted |= set(re.findall(r'\\\\",\\\\"([a-z_]+)', state_fn))
    js_keys = set()
    # Form fields live in fillForm() (NOT the 1 s status tick anymore).
    fill_fn = js.split("function fillForm(s)", 1)[1].split(
        "async function loadForm()", 1)[0]
    for m in re.finditer(r"for\(let k of \[(.*?)\]\)", fill_fn):
        js_keys |= set(re.findall(r"'([a-z_]+)'", m.group(1)))
    # top-level flags used directly
    for k in ("link", "running", "spoof", "relays", "cycles", "acts", "stage",
              "fw"):
        if f"s.{k}" in js or f"s.cfg.{k}" in js:
            js_keys.add(k)
    for k in sorted(js_keys):
        # relays/link/etc are top-level; cfg.* live under cfg:{...}
        # lbl0-7 are emitted by a builder loop (",\"lbl" + i), not literally.
        if re.fullmatch(r"lbl[0-7]", k):
            ok = "'\"lbl\"'" in state_fn or '",\\"lbl"' in state_fn
            print(f"state key {k}: {'OK (builder loop)' if ok else 'NOT EMITTED'}")
            if not ok:
                fail(f"JS reads state key '{k}' not emitted by handle_state()")
            continue
        ok = (k in emitted) or (f'"{k}"' in state_fn) or (f"\\{k}" in state_fn)
        print(f"state key {k}: {'OK' if ok else 'NOT EMITTED'}")
        if not ok:
            fail(f"JS reads state key '{k}' not emitted by handle_state()")

    # 4. POST keys consumed per endpoint
    posts = {
        "/api/relay": ["i", "on"],
        "/api/seq": ["cmd"],
        "/api/config": ["rmode", "nrel", "step", "hseq", "swp", "hall",
                        "bmode", "alow", "dir", "loop", "cpause", "clim",
                        "stag", "lbl0", "lbl1", "lbl2", "lbl3", "lbl4",
                        "lbl5", "lbl6", "lbl7"],
        "/api/spoof": ["cmd", "sv", "sa", "sc", "ssoc", "ssec",
                       "s2v", "s2a", "s2c", "s2soc", "s2sec", "sena", "spin"],
        "/api/admin": ["cmd", "pass", "ap_ssid", "ap_pass", "ap_ch",
                       "a_pass", "sta_en", "sta_ssid", "sta_pass", "auto"],
        "/api/ota": ["cmd", "pass", "ota_auto"],
    }
    handlers = {
        "/api/relay": "handle_relay", "/api/seq": "handle_seq",
        "/api/config": "handle_config", "/api/spoof": "handle_spoof",
        "/api/admin": "handle_admin", "/api/ota": "handle_ota",
    }
    for ep, keys in posts.items():
        hname = handlers[ep]
        body = src.split(f"static void {hname}()", 1)[1].split(
            "\n}\n", 1)[0]
        for k in keys:
            if re.fullmatch(r"lbl[0-7]", k):
                # Dynamic keys, consumed via the snprintf(k,"lbl%u") loop.
                ok = '"lbl%u"' in body
                print(f"POST {ep} key {k}: {'OK (builder loop)' if ok else 'NOT CONSUMED'}")
                if not ok:
                    fail(f"{ep} JS posts '{k}' but {hname} never reads it")
                continue
            ok = (f'"{k}"' in body)
            print(f"POST {ep} key {k}: {'OK' if ok else 'NOT CONSUMED'}")
            if not ok:
                fail(f"{ep} JS posts '{k}' but {hname} never reads it")

    print("WEB-CONTRACT " + ("PASS" if not fails else f"FAIL ({len(fails)})"))
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
