#!/usr/bin/env python3
"""Refresh the SIM_NOT_PORTED lists of partially ported models.

A concrete model with the marker line (MARKER below) followed by
SIM_NOT_PORTED(...) lines is a deliberate partial port
(e.g. only a relic's IsAllowed). When the member tables grow (a C++ type is
ported and more members get generated), such a class must list the new
members it overrides in C# but does not declare. This tool rewrites the
SIM_NOT_PORTED block of those headers only; fully ported classes are still
checked strictly by tools/gen/gen_dispatch.py.

Usage: tools/port/refresh_not_ported.py
"""
import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "gen"))
import gen_dispatch as gd  # noqa: E402

ROOT = gd.ROOT
# The line a partial port puts before its SIM_NOT_PORTED list.
MARKER = "    // Members left to Unported (tools/port/refresh_not_ported.py):"


def main():
    import json
    data = json.load(open(gd.CONTENT, encoding="utf-8"))
    overrides = {}
    for m in data["models"]:
        if m["kind"] in gd.KINDS:
            overrides[m["type"].split(".")[-1]] = (m["kind"], {o.split(" ")[0] for o in m.get("overrides", [])})
    bases = ["AbstractModel"] + list(dict.fromkeys(v[3] for v in gd.KINDS.values()
                                                   if os.path.exists(os.path.join(gd.MODELS, v[3] + ".def"))))
    members = {b: [mem["name"] for mem in gd.read_members(b)] for b in bases}
    changed = 0
    for kind, (_, _, folder, base) in gd.KINDS.items():
        d = os.path.join(gd.MODELS, folder)
        if not os.path.isdir(d):
            continue
        for fn in sorted(os.listdir(d)):
            path = os.path.join(d, fn)
            text = open(path, encoding="utf-8").read()
            if MARKER not in text:
                continue
            m = re.search(r"^struct (\w+)", text, re.M)
            cls = m.group(1)
            if cls not in overrides:
                continue
            ov = overrides[cls][1]
            want = []
            for b in ("AbstractModel", base):
                for name in members.get(b, []):
                    if ("%s.%s" % (b, name) in ov or "%s.get_%s" % (b, name) in ov) and not gd.declares(text, cls, name):
                        want.append(name)
            want = sorted(set(want))
            block = "".join("    SIM_NOT_PORTED(%s)\n" % n for n in want)
            new = re.sub(r"(%s\n)(    SIM_NOT_PORTED\(\w+\)\n)*" % re.escape(MARKER), lambda m: m.group(1) + block, text, count=1)
            if new != text:
                open(path, "w", encoding="utf-8").write(new)
                changed += 1
    print("refreshed %d partial ports" % changed)


if __name__ == "__main__":
    main()
