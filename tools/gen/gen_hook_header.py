#!/usr/bin/env python3
"""Generate game/Hooks/Hook.h: the declarations of every Hook.cs dispatcher.

The C# signatures are read from the decompiled Hooks/Hook.cs and translated
with the same type table as tools/gen/gen_members.py. Dispatchers whose
signature uses a type the port does not have yet are listed at the end of the
header instead. The bodies are ported by hand in game/Hooks/Hook.cpp.

Usage: tools/gen/gen_hook_header.py
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_members as gm  # noqa: E402

ROOT = gm.ROOT
OUT = os.path.join(ROOT, "game", "Hooks", "Hook.h")

EXTRA = {
    "HookPlayerChoiceContext": "sts2::PlayerChoiceContext&",
    "OrbModel": "sts2::OrbModel",
    "IEnumerable<AbstractModel>": "const sts2::ModelList&",
    "out List<AbstractModel>": "sts2::ModelList&",
    "out IEnumerable<AbstractModel>": "sts2::ModelList&",
    "out AbstractModel": "sts2::AbstractModel&",
    "ISet<CardKeyword>": "sts2::CardKeywordSet&",
    "out Decimal": "sts2::Dec&",
    "out Int32": "int32_t&",
    "out Boolean": "bool&",
}
# Return types that differ from the parameter form.
RET_EXTRA = {"IEnumerable<AbstractModel>": "sts2::ModelList"}
# AfterMapGenerated mutates a reference-type ActMap and stays guarded until the
# hook's C++ signature can preserve that reference semantics.
SKIP_NAMES = {"AfterMapGenerated"}
CS_ALIASES = {"decimal": "Decimal", "bool": "Boolean", "int": "Int32", "float": "Single", "void": "Void",
              "string": "String"}


def cs_type(t):
    t = t.strip().replace("?", "")
    out = ""
    if t.startswith("out "):
        out, t = "out ", t[4:].strip()
    t = CS_ALIASES.get(t, t)
    return out + t


def split_params(s):
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch == "<":
            depth += 1
        elif ch == ">":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def main():
    src = open(os.path.join(gm.SRC, "Hooks", "Hook.cs"), encoding="utf-8").read()
    decls, missing = [], []
    for m in re.finditer(r"^\tpublic static (async )?([\w<>?,\[\] ]+?) (\w+)\((.*?)\)\s*$", src, re.M):
        ret_cs, name, params = m.group(2).strip(), m.group(3), m.group(4)
        if name in SKIP_NAMES:
            continue
        if ret_cs.startswith("Task<"):
            ret_cs = ret_cs[5:-1]
        ret_key = cs_type(ret_cs)
        ret = RET_EXTRA.get(ret_key) or EXTRA.get(ret_key) or gm.RETURNS.get(ret_key)
        if ret_key in ("Task", "Void"):
            ret = "void"
        cpp_params, ok = [], ret is not None
        for p in split_params(params):
            p = re.sub(r"\s*=\s*[^,]+$", "", p)  # default values
            ptype, pname = p.rsplit(" ", 1)
            key = cs_type(ptype)
            t = EXTRA.get(key) or gm.TYPES.get(key)
            if t is None:
                ok = False
                break
            cpp_params.append("%s %s" % (t, pname if pname not in gm.CPP_KEYWORDS else pname + "_"))
        sig = "%s %s(%s);" % (ret, name, ", ".join(cpp_params))
        if ok:
            decls.append(sig)
        else:
            missing.append("%s %s(%s)" % (m.group(2).strip(), name, params))
    lines = ["// Hooks/Hook.cs. GENERATED declarations (tools/gen/gen_hook_header.py); the bodies",
             "// are ported in Hook.cpp.",
             "#pragma once",
             "",
             '#include "game/All.h"',
             "",
             "namespace sts2 {",
             "",
             "// IEnumerable<AbstractModel> / List<AbstractModel> of hook modifiers.",
             "using ModelList = StaticVec<AbstractModel, 32>;",
             "",
             "namespace Hook {",
             ""]
    lines += decls
    lines += ["", "}  // namespace Hook", "}  // namespace sts2"]
    if missing:
        lines += ["", "// Not declared yet (a C# type in the signature has no C++ port):"]
        lines += ["//   " + s for s in missing]
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("Hook.h: %d declared, %d waiting for types" % (len(decls), len(missing)))


if __name__ == "__main__":
    main()
