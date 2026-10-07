#!/usr/bin/env python3
"""Porting helper: translate Hooks/Hook.cs into a first draft of game/Hooks/Hook.cpp.

This is not a generator of committed output: it produces a draft that is
reviewed, fixed by hand and then maintained as normal source. It handles the
regular shapes of Hook.cs (iterate listeners, push/pop the choice context,
accumulate modifications, first veto wins) statement by statement; any body
with a statement it does not understand becomes a stub that throws Unported
and is marked `PORT-TODO`, to be ported by hand.

Usage: tools/port/port_hook.py > draft.cpp
"""
import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "gen"))
import gen_hook_header as ghh  # noqa: E402
import gen_members as gm  # noqa: E402

SRC = os.path.join(gm.SRC, "Hooks", "Hook.cs")


def declared_signatures():
    """name -> C++ declaration line from the generated Hook.h."""
    text = open(os.path.join(gm.ROOT, "game", "Hooks", "Hook.h"), encoding="utf-8").read()
    body = text.split("namespace Hook {", 1)[1].split("}  // namespace Hook", 1)[0]
    sigs = {}
    for line in body.splitlines():
        line = line.strip()
        m = re.match(r"(.+?) (\w+)\((.*)\);$", line)
        if m:
            sigs[m.group(2)] = (m.group(1), m.group(3))
    return sigs


ITER = [
    (r"IterateCombatHookListeners\(combatState\)", "IterateCombatHookListeners(combatState)"),
    (r"combatState\.IterateHookListeners\(\)", "IterateHookListenersOf(combatState)"),
    (r"runState\.IterateHookListeners\(combatState\)", "IterateRunHookListeners(runState, combatState)"),
    (r"runState\.IterateHookListeners\(null\)", "IterateRunHookListeners(runState, CombatState())"),
]

SKIP_LINES = [
    r"\w+\.InvokeExecutionFinished\(\);",
    r"List<Task> \w+ = new List<Task>\(\);",
    r"\w+\.Add\(\w+\.WaitForCompletion\(\)\);",
    r"await Task\.WhenAll\(\w+\);",
    r"ulong\? netId = LocalContext\.NetId;",
]

TYPE_MAP = {"decimal": "Dec", "int": "int32_t", "bool": "bool", "float": "float"}


def expr(e):
    """Translate a C# expression (the small subset Hook.cs uses)."""
    e = e.strip()
    e = re.sub(r"\(int\)(\w+)", r"\1.to_int()", e)
    e = re.sub(r"\b(\d+)m\b", r"Dec(\1)", e)
    e = e.replace("decimal.MaxValue", "Dec::max_value()")
    e = e.replace("null", "{}")
    e = re.sub(r"(\w+)\.Contains\(", r"list_contains(\1, ", e)
    e = e.replace("!= {}", "")  # `x != null` -> truthiness handled below
    return e


def translate_body(body):
    lines = [l for l in body.split("\n")]
    out = []
    depth = 1
    i = 0
    while i < len(lines):
        raw = lines[i]
        l = raw.strip()
        i += 1
        if not l:
            continue
        ind = "    " * depth
        if l == "{":
            out.append("    " * (depth - 1) + "{") if False else None
            continue
        if l == "}":
            depth -= 1
            out.append("    " * depth + "}")
            continue
        if any(re.fullmatch(p, l) for p in SKIP_LINES):
            continue
        if l == "if (!netId.HasValue)":
            # the next three lines are `{ return; }`
            i += 3
            continue
        m = re.fullmatch(r"foreach \(AbstractModel (\w+) in (.+)\)", l)
        if m:
            src = m.group(2)
            matched = False
            for pat, rep in ITER:
                if re.fullmatch(pat, src):
                    src, matched = rep, True
                    break
            if not matched:
                translate_body.failed = l
                return None
            out.append(ind + "for (AbstractModel %s : %s) {" % (m.group(1), src))
            depth += 1
            i += 1  # skip "{"
            continue
        m = re.fullmatch(r"HookPlayerChoiceContext (\w+) = new HookPlayerChoiceContext\(.*\);", l)
        if m:
            out.append(ind + "PlayerChoiceContext %s;" % m.group(1))
            continue
        m = re.fullmatch(r"Task (\w+) = (\w+)\.(\w+)\((.*)\);", l)
        if m and i < len(lines) and re.fullmatch(r"await \w+\.AssignTaskAndWaitForPauseOrCompletion\(%s\);" % m.group(1),
                                                 lines[i].strip()):
            out.append(ind + "%s.%s(%s);" % (m.group(2), m.group(3), expr(m.group(4))))
            i += 1
            continue
        m = re.fullmatch(r"await (\w+)\.(\w+)\((.*)\);", l)
        if m:
            out.append(ind + "%s.%s(%s);" % (m.group(1), m.group(2), expr(m.group(3))))
            continue
        m = re.fullmatch(r"(\w+)\.(PushModel|PopModel)\((\w+)\);", l)
        if m:
            out.append(ind + l)
            continue
        m = re.fullmatch(r"(decimal|int|bool|float) (\w+) = (.+);", l)
        if m:
            out.append(ind + "%s %s = %s;" % (TYPE_MAP[m.group(1)], m.group(2), expr(m.group(3))))
            continue
        m = re.fullmatch(r"List<AbstractModel> (\w+) = new List<AbstractModel>\(\);", l)
        if m:
            out.append(ind + "ModelList %s;" % m.group(1))
            continue
        m = re.fullmatch(r"(\w+)\.Add\((\w+)\);", l)
        if m:
            out.append(ind + "%s.push_back(%s);" % (m.group(1), m.group(2)))
            continue
        m = re.fullmatch(r"(\w+) (=|\+=|\*=|-=) (.+);", l)
        if m:
            out.append(ind + "%s %s %s;" % (m.group(1), m.group(2), expr(m.group(3))))
            continue
        m = re.fullmatch(r"if \((.+)\)", l)
        if m:
            out.append(ind + "if (%s) {" % expr(m.group(1)))
            depth += 1
            i += 1  # skip "{"
            continue
        if l == "else":
            out[-1] = out[-1] + " else {"
            depth += 1
            i += 1
            continue
        m = re.fullmatch(r"return (.*);", l)
        if m:
            out.append(ind + "return %s;" % expr(m.group(1)))
            continue
        if l == "return;":
            out.append(ind + "return;")
            continue
        translate_body.failed = l
        return None
    return out


def main():
    src = open(SRC, encoding="utf-8").read()
    sigs = declared_signatures()
    funcs = re.findall(r"\n\tpublic static (?:async )?[^\n]*? (\w+)\(([^\n]*)\)\n\t\{\n(.*?)\n\t\}\n", src, re.S)
    done, todo = 0, []
    print("// Hooks/Hook.cs")
    print('#include "game/Hooks/Hook.h"')
    print("")
    print("namespace sts2::Hook {")
    print("")
    for name, params, body in funcs:
        if name not in sigs:
            continue
        ret, cparams = sigs[name]
        tr = translate_body(body)
        print("// Hooks/Hook.cs: %s" % name)
        print("%s %s(%s) {" % (ret, name, cparams))
        if tr is None:
            todo.append(name)
            sys.stderr.write("  %s: %s\n" % (name, getattr(translate_body, "failed", "?")))
            print("    // PORT-TODO: port by hand")
            print('    throw Unported("Hook::%s is not ported");' % name)
        else:
            done += 1
            for line in tr:
                print(line)
        print("}")
        print("")
    print("}  // namespace sts2::Hook")
    sys.stderr.write("translated %d, by hand %d: %s\n" % (done, len(todo), " ".join(todo)))


if __name__ == "__main__":
    main()
