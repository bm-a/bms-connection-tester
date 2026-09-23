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

    # ---- dashboard variants (v2.7): three PAGE_DASH blocks (classic/lite/full),
    # one per WEB_UI_VARIANT branch. Each block's JS must only fetch routed
    # endpoints and only touch ids present in its own HTML (lite is a strict
    # subset by construction — the same rules prove it).
    blocks = []
    for m in re.finditer(r'PAGE_DASH\[\] PROGMEM = R"HTML\((.*?)\)HTML"',
                         src, re.S):
        blocks.append(m.group(1))
    print(f"dash variants: {len(blocks)}")
    if len(blocks) != 3:
        fail(f"expected 3 PAGE_DASH variants (classic/lite/full), found {len(blocks)}")
        blocks = blocks[:1] if blocks else []

    def check_block(page, tag):
        if "<script>" not in page or "</script>" not in page:
            fail(f"{tag}: no inline script block")
            return
        js = page.split("<script>", 1)[1].split("</script>", 1)[0]
        html = page.split("<script>", 1)[0]
        for ep in sorted(set(re.findall(r"fetch\(['\"]([^'\"]+)['\"]", js))):
            ok = any(p == ep for (p, _) in routes)
            print(f"[{tag}] endpoint {ep}: {'OK' if ok else 'MISSING ROUTE'}")
            if not ok:
                fail(f"[{tag}] JS fetches {ep} with no server.on route")
        html_ids = set(re.findall(r"id=([A-Za-z_]+)", html))
        for eid in sorted(set(re.findall(r"getElementById\(['\"]([^'\"]+)['\"]", js))):
            if eid == "lbl":
                ok = "labels" in html_ids
            elif eid == "bench_r":
                # Dynamic rect ids bench_r0-7 (the id= regex stops at digits,
                # so search the raw page for the first rect id).
                ok = "bench_r0" in page
                print(f"[{tag}] element #bench_rN: {'OK (svg rects)' if ok else 'MISSING'}")
                if not ok:
                    fail(f"[{tag}] JS touches #bench_rN with no bench_r0 rect")
                continue
            else:
                ok = eid in html_ids
            print(f"[{tag}] element #{eid}: {'OK' if ok else 'MISSING id='}")
            if not ok:
                fail(f"[{tag}] JS uses #{eid} with no id= in HTML")

    for i, page in enumerate(blocks):
        check_block(page, ("classic", "lite", "full")[i] if len(blocks) == 3 else f"v{i}")

    # Login wall is gone since v2.3.1 (WPA2 is the gate); sensitive actions
    # carry the admin password per request. Manual firmware upload page stays.
    for token in ['href=/update', 'action=/update']:
        if token not in src:
            fail(f"missing {token}")
    for dead in ['action=/login', 'href=/logout', 'PAGE_LOGIN',
                 'handle_login', 'handle_logout']:
        if dead in src:
            fail(f"login remnant still present: {dead}")

    # v2.4 surface: new endpoints routed, per-mode rows + cards present,
    # progress endpoint referenced by the /update page. v2.5: portal
    # landing for OS probes / mini-browsers.
    for route in ['"/api/sta"', '"/api/cmd"', '"/api/backup"',
                   '"/api/restore"', '"/api/uprog"', '"/api/meter"']:
        if route not in src:
            fail(f"missing route {route}")
    for token in ["hotspot-detect.html", "generate_204",
                  "CaptiveNetworkSupport", "Open Dashboard", "bmstester.local",
                  "handle_portal", "collectHeaders"]:
        if token not in src:
            fail(f"missing portal surface: {token}")
    for rid in ["row_step", "row_seqonly", "row_chaseonly", "row_allonly",
                "row_dir", "info_fw", "info_mem", "info_net", "cmdout",
                "sta_test_msg", "restoremsg", "spoofsave", "otaurlmsg",
                "otainstall", "restorefile", "trigmsg", "sinv",
                "dotG", "dotR", "meters", "metermsg"]:
        if f"id={rid}" not in src:
            fail(f"missing dashboard element id={rid}")
    if "showMode()" not in src:
        fail("per-mode menu needs showMode()")
    # Single-end rule: exactly one Update.end(true) in web_ui.cpp.
    if src.count("Update.end(true)") != 1:
        fail("expected exactly one Update.end(true) (single-end rule)")
    if "UPDATE_SIZE_UNKNOWN" in src:
        fail("UPDATE_SIZE_UNKNOWN banned (explicit budget only)")

    # 2. element ids (per variant block, checked above in check_block).

    # 3. state keys: union of all variant scripts vs handle_state emission.
    state_fn = src.split("static void handle_state()", 1)[1].split(
        "static void handle_relay()", 1)[0]
    emitted = set(re.findall(r'"\\?"?([a-z_]+)\\?"?:', state_fn))
    # cfg keys are built as \",\"key\": so also catch them
    emitted |= set(re.findall(r'\\\\",\\\\"([a-z_]+)', state_fn))
    js_all = "\n".join(
        p.split("<script>", 1)[1].split("</script>", 1)[0]
        for p in blocks if "<script>" in p)
    js_keys = set()
    # Form fields live in fillForm() (NOT the 1 s status tick anymore).
    for fill_fn in re.findall(r"function fillForm\(s\)(.*?)async function loadForm\(\)",
                             js_all, re.S):
        for m in re.finditer(r"for\(let k of \[(.*?)\]\)", fill_fn):
            js_keys |= set(re.findall(r"'([a-z_]+)'", m.group(1)))
    # top-level flags used directly
    for k in ("link", "running", "spoof", "relays", "cycles", "acts", "stage",
              "fw"):
        if f"s.{k}" in js_all or f"s.cfg.{k}" in js_all:
            js_keys.add(k)
    # v2.4: refresh() reads info/STA fields straight off s.cfg (not via
    # fillForm), so scrape those uses too.
    for m in re.finditer(r"s\.cfg\.([a-z_]+)", js_all):
        js_keys.add(m.group(1))
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
                       "s2v", "s2a", "s2c", "s2soc", "s2sec", "sena", "sinv",
                       "spin"],
        "/api/admin": ["cmd", "pass", "ap_ssid", "ap_pass", "ap_ch",
                       "a_pass", "sta_en", "sta_ssid", "sta_pass", "auto"],
        "/api/ota": ["cmd", "pass", "ota_auto", "ota_url", "ota_int_h"],
        "/api/sta": ["cmd", "pass", "ssid", "sta_pass"],
        "/api/cmd": ["cmd", "pass"],
        "/api/restore": ["pass", "backup", "nrel", "sv", "ap_ssid",
                         "ota_url"],
        "/api/meter": ["cmd"],
    }
    handlers = {
        "/api/relay": "handle_relay", "/api/seq": "handle_seq",
        "/api/config": "handle_config", "/api/spoof": "handle_spoof",
        "/api/admin": "handle_admin", "/api/ota": "handle_ota",
        "/api/sta": "handle_sta", "/api/cmd": "handle_cmd",
        "/api/restore": "handle_restore", "/api/meter": "handle_meter",
    }
    for ep, keys in posts.items():
        hname = handlers[ep]
        body = src.split(f"static void {hname}()", 1)[1].split(
            "\n}\n", 1)[0]
        if ep in ("/api/restore", "/api/config", "/api/spoof"):
            # These reuse the shared appliers: keys land in THEIR bodies.
            for helper in ("apply_cfg_keys", "apply_spoof_keys"):
                body += src.split(f"static bool {helper}", 1)[1].split(
                    "\n}\n", 1)[0] if f"static bool {helper}" in src else ""
                body += src.split(f"static void {helper}", 1)[1].split(
                    "\n}\n", 1)[0] if f"static void {helper}" in src else ""
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
