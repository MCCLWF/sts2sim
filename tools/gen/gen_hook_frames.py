#!/usr/bin/env python3
"""Add observational scopes to existing Hook bodies without rewriting logic.

Public Hook declarations must each have exactly one matching definition. The
damage helper is included as an internal causal frame. Re-running is idempotent;
--check validates generated scopes without modifying the source.
"""
import argparse
from pathlib import Path
import re
from execution_signatures import resolve, capture_arguments

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "game/Hooks/Hook.cpp"
PATTERN = re.compile(r"^([^\n{};]+?)\s+(\w+)\(([^{};]*?)\)\s*\{", re.MULTILINE)
MARKER = re.compile(r'^    HookExecutionScope hook_execution_observation\([^\n]*\);\n', re.MULTILINE)
BEGIN = "    // STS2_EXECUTION_BODY_BEGIN\n"
END = "    // STS2_EXECUTION_BODY_END\n"

def body_end(source, start):
    """Find the real function brace, ignoring comments and quoted literals."""
    depth, pos = 1, start
    while pos < len(source):
        if source.startswith("//", pos):
            pos = source.index("\n", pos) + 1
        elif source.startswith("/*", pos):
            pos = source.index("*/", pos + 2) + 2
        elif source[pos] in "\"'":
            quote = source[pos]
            pos += 1
            while pos < len(source):
                if source[pos] == "\\": pos += 2
                elif source[pos] == quote:
                    pos += 1
                    break
                else: pos += 1
        else:
            if source[pos] == "{": depth += 1
            elif source[pos] == "}":
                depth -= 1
                if not depth: return pos
            pos += 1
    raise ValueError("Unclosed Hook function body")

def generated():
    source = SOURCE.read_text()
    header = (ROOT / "game/Hooks/Hook.h").read_text()
    public = set(re.findall(r'^\S[^\n]*?\s+(\w+)\([^\n]*\);$', header, re.MULTILINE))
    wanted = public | {"ModifyDamageInternal"}
    seen = {}

    replacements = []
    for match in PATTERN.finditer(source):
        name = match[2]
        if name not in wanted:
            continue
        seen[name] = seen.get(name, 0) + 1
        params = match[3]
        # Hook signatures have no default expressions or function-pointer args.
        # Split template arguments correctly, then retain parameter names only.
        args, part, depth = [], "", 0
        for char in params + ",":
            if char == "," and depth == 0:
                if part.strip():
                    arg = re.search(r'(\w+)\s*$', part)
                    if arg is None:
                        raise ValueError(f"Cannot resolve Hook argument: {name}: {part}")
                    args.append(arg[1])
                part = ""
            else:
                part += char
                if char in "<([": depth += 1
                elif char in ">)]": depth -= 1
        invocation = ", ".join([f'"Hook.{name}"', *args])
        end = body_end(source, match.end())
        body = source[match.end():end]
        if BEGIN in body:
            if body.count(BEGIN) != 1 or body.count(END) != 1:
                raise ValueError(f"Invalid generated body markers: {name}")
            body = body.split(BEGIN, 1)[1].split(END, 1)[0]
        else:
            body = MARKER.sub("", body)
        binding = resolve("Hook", name, args)
        wrapped = (f"\n    HookExecutionScope hook_execution_observation({invocation});\n"
                   f'    return hook_execution_observation.Invoke("{binding["returns"]}", [&]() -> {match[1].strip()} {{\n'
                   + BEGIN + body + END
                   + f'    }}, [&]() {{ return std::vector<ExecutionArgument>{capture_arguments(binding)}; }},\n'
                   + f'       [&]() {{ return std::vector<ExecutionArgument>{capture_arguments(binding, after=True)}; }});\n')
        replacements.append((match.end(), end, wrapped))

    output = source
    for start, end, wrapped in reversed(replacements):
        output = output[:start] + wrapped + output[end:]
    missing = [name for name in sorted(wanted) if seen.get(name) != 1]
    if missing:
        raise ValueError(f"Hook scope coverage is not exact: {missing}")
    return output, len(seen)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    output, count = generated()
    if args.check:
        if output != SOURCE.read_text():
            raise SystemExit("Hook scopes changed; run tools/gen/gen_hook_frames.py")
    else:
        SOURCE.write_text(output)
    print(f"Hook execution scopes: {count} definitions")
