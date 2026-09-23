#!/usr/bin/env python3
"""Build + merge + verify + upload uniform one-file flash images for releases.

Replaces the old per-version manual loop (worktree, pio run, merge_bin,
magic check, gh upload, repeat 10x) with one command. Everything per tag is
built from THAT TAG's own source in an isolated worktree — bootloaders and
partition tables are never mixed across versions.

Naming (uniform, Tasmota-style): bms-tester-8mb.bin (+ bms-tester-n16r8.bin
where the tag has the s3-n16r8 env). Flash: write-flash 0x0 <file>.
Plus the OTA-named raw app bins the box actually pulls (src/ota.h):
firmware.bin (8MB env) + n16r8-firmware.bin (n16r8 env) — without these,
box Check sees the release but Install 404s (exactly the v2.7 gap).

Runs INSIDE proot-debian (needs pio + esptool on PATH there):
  proot-distro login debian -- sh -c 'export PATH=/root/.local/bin:$PATH;
    python3 /data/data/com.termux/files/home/bms-connection-tester/tools/merge_releases.py v1.2 v2.0 --upload'

Usage:
  merge_releases.py [--upload] [--workroot DIR] [--keep] TAG [TAG ...]
  --upload    gh release upload the merged files (else just build+verify)
  --workroot  where to create tag worktrees (default: ../tmp/opencode/relwork)
  --keep      keep worktrees + .pio after (default: removed)
"""
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
MERGED_8MB = "bms-tester-8mb.bin"
MERGED_N16 = "bms-tester-n16r8.bin"
OTA_8MB = "firmware.bin"        # must match OTA_ASSET_8MB in src/ota.h
OTA_N16 = "n16r8-firmware.bin"  # must match OTA_ASSET_N16R8 in src/ota.h
OFFSETS = [("0x0", "bootloader.bin"), ("0x8000", "partitions.bin"),
           ("0x10000", "firmware.bin")]

total_steps = 0
done_steps = 0


def bar(label):
    pct = int(done_steps * 100 / max(total_steps, 1))
    n = pct // 2
    sys.stdout.write("\r[%s] [%s%s] %3d%%" % (label, "#" * n, " " * (50 - n), pct))
    sys.stdout.flush()


def step(label):
    global done_steps
    done_steps += 1
    bar(label)


def run(cmd, cwd=None):
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError("FAILED: %s\n%s" % (" ".join(cmd), r.stderr[-2000:]))
    return r.stdout


def envs_of(workdir):
    """PIO envs declared by platformio.ini (8MB always, n16r8 if present)."""
    ini = open(os.path.join(workdir, "platformio.ini")).read()
    envs = re.findall(r"\[env:([^\]]+)\]", ini)
    out = [e for e in ("esp32-s3-devkitc-1", "s3-n16r8") if e in envs]
    if "esp32-s3-devkitc-1" not in out:
        raise RuntimeError("tag has no esp32-s3-devkitc-1 env")
    return out


def fw_version(workdir):
    for f in ("src/bms_protocol.h", "src/main.cpp"):
        p = os.path.join(workdir, f)
        if os.path.exists(p):
            m = re.search(r'FW_VERSION\s+"([^"]+)"',
                          open(p).read())
            if m:
                return m.group(1)
    return None


def merge(esptool, bindir, outfile):
    cmd = [esptool, "--chip", "esp32s3", "merge_bin", "-o", outfile]
    if esptool.endswith(".py"):  # package script, not on PATH as executable
        cmd = [sys.executable] + cmd
    for addr, name in OFFSETS:
        cmd += [addr, os.path.join(bindir, name)]
    run(cmd)


def verify(path, version):
    d = open(path, "rb").read()
    assert d[0] == 0xE9, "bootloader magic @0x0"
    assert d[0x8000] | (d[0x8001] << 8) == 0x50AA, "partition magic @0x8000"
    assert d[0x10000] == 0xE9, "app magic @0x10000"
    if version:
        assert version.encode() in d, "version %s byte-present" % version
    return len(d)


def one_tag(tag, args, esptool):
    global total_steps, done_steps
    t0 = time.time()
    wt = os.path.join(args.workroot, "wt-" + tag)
    if os.path.exists(wt):
        shutil.rmtree(wt)
    run(["git", "-C", REPO, "worktree", "add", wt, tag])
    try:
        envs = envs_of(wt)
        npairs = 1 + (1 if "s3-n16r8" in envs else 0)
        total_steps += len(envs) + npairs + (1 if args.upload else 0)
        bar(tag)
        bindirs = {}
        for e in envs:  # [n/N] progress per env build
            step(tag)
            run(["pio", "run", "-d", wt, "-e", e])
            bindirs[e] = os.path.join(wt, ".pio", "build", e)
        outdir = os.path.join(args.workroot, "out-" + tag)
        os.makedirs(outdir, exist_ok=True)
        ver = fw_version(wt)
        merged = []
        pairs = [("esp32-s3-devkitc-1", MERGED_8MB, OTA_8MB)]
        if "s3-n16r8" in bindirs:
            pairs.append(("s3-n16r8", MERGED_N16, OTA_N16))
        for e, name, ota_name in pairs:
            step(tag)
            out = os.path.join(outdir, name)
            merge(esptool, bindirs[e], out)
            verify(out, ver)
            merged.append(out)
            raw_src = os.path.join(bindirs[e], "firmware.bin")
            raw_dst = os.path.join(outdir, ota_name)
            shutil.copyfile(raw_src, raw_dst)
            raw = open(raw_dst, "rb").read()
            assert raw[0] == 0xE9, "app magic in %s" % ota_name
            if ver:
                assert ver.encode() in raw, \
                    "version %s byte-present in %s" % (ver, ota_name)
        if args.upload:
            step(tag)
            run(["gh", "release", "upload", tag] + merged +
                [os.path.join(outdir, o) for _, _, o in pairs])
        secs = int(time.time() - t0)
        sys.stdout.write("\n[%s] done in %dm%02ds: %s\n" %
                         (tag, secs // 60, secs % 60,
                          ", ".join("%s (%d B)" % (os.path.basename(m),
                                                  os.path.getsize(m))
                                    for m in merged)))
    finally:
        run(["git", "-C", REPO, "worktree", "remove", "--force", wt])
        if not args.keep:
            shutil.rmtree(os.path.join(args.workroot, "out-" + tag),
                          ignore_errors=True)


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("tags", nargs="+")
    ap.add_argument("--upload", action="store_true")
    ap.add_argument("--workroot",
                    default="/data/data/com.termux/files/home/tmp/opencode/relwork")
    ap.add_argument("--keep", action="store_true")
    args = ap.parse_args()
    esptool = shutil.which("esptool.py") or shutil.which("esptool") or \
        os.path.expanduser("~/.platformio/packages/tool-esptoolpy/esptool.py")
    os.makedirs(args.workroot, exist_ok=True)
    global total_steps
    for tag in args.tags:  # rough total for the bar (envs counted per tag)
        total_steps += 5
    for tag in args.tags:
        one_tag(tag, args, esptool)


if __name__ == "__main__":
    main()
