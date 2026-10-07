#!/usr/bin/env python3
"""Generate a source-first Silent model, override, and branch inventory.

The authoritative model list comes from data/content.json, which was exported
from STS2 v0.111.0. Source files are read from STS2_SRC or the repository's
standard sibling checkout. The generated report is an audit denominator only:
having a C++ header or IsUsable case does not establish behavioral support.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import sys
from typing import Any

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "gen"))
import gen_dispatch as gd  # noqa: E402

MODEL_KINDS = {
    "CardModel": "Card",
    "PowerModel": "Power",
    "RelicModel": "Relic",
    "PotionModel": "Potion",
    "AfflictionModel": "Affliction",
    "EnchantmentModel": "Enchantment",
    "OrbModel": "Orb",
}
GENERIC_TYPE_RE = re.compile(r"<\s*([A-Za-z_][A-Za-z_0-9]*)\s*>")
STATIC_CREATE_RE = re.compile(r"\b([A-Za-z_][A-Za-z_0-9]*)\s*\.\s*Create[A-Za-z_0-9]*\b")
FLOW_RE = re.compile(r"\b(if|switch|while|for|foreach|catch)\s*\(")
CASE_RE = re.compile(r"\b(case\s+[^\r\n:]+|default)\s*:")
ELSE_RE = re.compile(r"\belse\b(?!\s+if\b)")
TERNARY_RE = re.compile(r"(?<![\w?.])\?(?![?.])")
SHORT_CIRCUIT_RE = re.compile(r"&&|\|\||\?\?(?!=)")
DECL_RE = re.compile(
    r"(?m)^[ \t]*(?:public|protected|private|internal)\s+"
    r"(?:(?:override|virtual|static|async|sealed|abstract|new|readonly|extern|partial)\s+)*"
    r"[A-Za-z_][\w.<>?\[\], ]*?\s+(?P<name>[A-Za-z_]\w*)\s*(?P<next>\(|=>|\{)"
)


def default_source() -> Path:
    configured = os.environ.get("STS2_SRC")
    if configured:
        return Path(configured).resolve()
    return (ROOT.parents[1] / "Slay the Spire 2" / "src" / "Core").resolve()


def mask_comments_and_literals(text: str) -> str:
    """Mask C# comments and string/char literals while preserving offsets."""
    out = list(text)
    i = 0
    n = len(text)
    state = "code"
    verbatim = False
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if state == "code":
            if c == "/" and nxt == "/":
                out[i] = out[i + 1] = " "
                i += 2
                state = "line"
                continue
            if c == "/" and nxt == "*":
                out[i] = out[i + 1] = " "
                i += 2
                state = "block"
                continue
            if c == "@" and nxt == '"':
                out[i] = out[i + 1] = " "
                i += 2
                state = "string"
                verbatim = True
                continue
            if c == '"':
                out[i] = " "
                i += 1
                state = "string"
                verbatim = False
                continue
            if c == "'":
                out[i] = " "
                i += 1
                state = "char"
                verbatim = False
                continue
            i += 1
            continue
        if state == "line":
            if c == "\n":
                state = "code"
            else:
                out[i] = " "
            i += 1
            continue
        if state == "block":
            if c == "*" and nxt == "/":
                out[i] = out[i + 1] = " "
                i += 2
                state = "code"
            else:
                if c != "\n":
                    out[i] = " "
                i += 1
            continue
        if state in ("string", "char"):
            terminator = '"' if state == "string" else "'"
            if verbatim and c == terminator and nxt == terminator:
                out[i] = out[i + 1] = " "
                i += 2
            elif not verbatim and c == "\\":
                out[i] = " "
                if i + 1 < n and text[i + 1] != "\n":
                    out[i + 1] = " "
                i += 2
            elif c == terminator:
                out[i] = " "
                i += 1
                state = "code"
            else:
                if c != "\n":
                    out[i] = " "
                i += 1
    return "".join(out)


def line_number(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def balanced_end(text: str, open_at: int, left: str, right: str) -> int:
    depth = 0
    for i in range(open_at, len(text)):
        if text[i] == left:
            depth += 1
        elif text[i] == right:
            depth -= 1
            if depth == 0:
                return i
    return len(text)


def normalized(text: str) -> str:
    return " ".join(text.split())


def member_spans(source_text: str) -> list[tuple[int, int, str]]:
    masked = mask_comments_and_literals(source_text)
    spans = []
    for match in DECL_RE.finditer(masked):
        name = match.group("name")
        marker = match.group("next")
        start = match.start()
        if marker == "(":
            open_at = masked.find("(", match.start())
            close_at = balanced_end(masked, open_at, "(", ")")
            arrow = masked.find("=>", close_at + 1, min(len(masked), close_at + 1000))
            brace = masked.find("{", close_at + 1, min(len(masked), close_at + 1000))
            semi = masked.find(";", close_at + 1, min(len(masked), close_at + 1000))
            if arrow >= 0 and (brace < 0 or arrow < brace) and (semi < 0 or arrow < semi):
                end = masked.find(";", arrow)
                end = end + 1 if end >= 0 else masked.find("\n", arrow)
                end = end if end >= 0 else len(masked)
            elif brace >= 0 and (semi < 0 or brace < semi):
                end = balanced_end(masked, brace, "{", "}") + 1
            else:
                end = close_at + 1
        elif marker == "=>":
            end = masked.find(";", match.start())
            end = end + 1 if end >= 0 else masked.find("\n", match.start())
            end = end if end >= 0 else len(masked)
        else:
            brace = masked.find("{", match.start())
            end = balanced_end(masked, brace, "{", "}") + 1 if brace >= 0 else match.end()
        spans.append((start, end, name))
    return spans


def source_branches(source_text: str) -> list[dict[str, Any]]:
    masked = mask_comments_and_literals(source_text)
    found: dict[tuple[int, str, str], dict[str, Any]] = {}

    for match in FLOW_RE.finditer(masked):
        open_at = masked.find("(", match.start())
        close_at = balanced_end(masked, open_at, "(", ")")
        condition = normalized(source_text[open_at + 1 : close_at])
        kind = match.group(1)
        if kind == "foreach":
            label = f"foreach ({condition})"
        elif kind == "for":
            label = f"for ({condition})"
        else:
            label = f"{kind} ({condition})"
        key = (match.start(), "flow", label)
        label = normalized(source_text[match.start() : close_at + 1])
        found[key] = {"line": line_number(source_text, match.start()), "offset": match.start(), "kind": "flow", "condition": label}

    for regex, kind in ((CASE_RE, "switch arm"), (ELSE_RE, "else")):
        for match in regex.finditer(masked):
            label = normalized(source_text[match.start() : match.end()])
            key = (match.start(), kind, label)
            found[key] = {"line": line_number(source_text, match.start()), "offset": match.start(), "kind": kind, "condition": label}

    # Include C# conditional expressions and short-circuit/null-coalescing
    # branches; report the containing source line for context.
    for regex, kind in ((TERNARY_RE, "ternary"), (SHORT_CIRCUIT_RE, "short-circuit")):
        for match in regex.finditer(masked):
            start = masked.rfind("\n", 0, match.start()) + 1
            end = masked.find("\n", match.end())
            if end < 0:
                end = len(masked)
            snippet = normalized(source_text[start:end])
            if not snippet:
                continue
            key = (match.start(), kind, snippet)
            found[key] = {"line": line_number(source_text, match.start()), "offset": match.start(), "kind": kind, "condition": snippet}

    spans = member_spans(source_text)
    for row in found.values():
        owners = [(end - start, name) for start, end, name in spans if start <= row["offset"] < end]
        row["member"] = min(owners)[1] if owners else "<type scope>"
        row.pop("offset")
    return sorted(found.values(), key=lambda row: (row["line"], row["kind"], row["condition"]))


def model_cpp_presence(model: dict[str, Any], source_to_cpp: dict[str, str], cpp_classes: dict[str, str]) -> dict[str, Any]:
    source_class = model["type"].split(".")[-1]
    cpp_class = source_to_cpp.get(source_class, source_class)
    return {"source_class": source_class, "cpp_class": cpp_class, "header": cpp_classes.get(cpp_class)}


def dispatch_ids() -> dict[str, set[str]]:
    path = ROOT / "game" / "generated" / "Dispatch.cpp"
    text = path.read_text(encoding="utf-8")
    marker = "bool IsUsable(ModelKind kind, uint16_t id) {"
    if marker not in text:
        raise ValueError("IsUsable definition not found")
    text = text.split(marker, 1)[1]
    result: dict[str, set[str]] = {}
    for model_kind, (dispatch_kind, enum_name, _folder, _handle) in gd.KINDS.items():
        if dispatch_kind in result:
            continue
        block = re.search(
            r"case ModelKind::" + re.escape(dispatch_kind) + r":(.*?)(?=\n    case ModelKind::|\n    default:)",
            text,
            re.S,
        )
        if not block:
            raise ValueError(f"{dispatch_kind} dispatch block not found")
        result[dispatch_kind] = set(re.findall(r"case " + re.escape(enum_name) + r"::(\w+):", block.group(1)))
    return result


def build_inventory(source_root: Path) -> dict[str, Any]:
    data = json.loads((ROOT / "data" / "content.json").read_text(encoding="utf-8"))
    models = [m for m in data["models"] if not m.get("mock") and not m["id"].split(".", 1)[-1].startswith("MOCK_")]
    by_id = {m["id"]: m for m in models}
    by_kind_class: dict[tuple[str, str], dict[str, Any]] = {}
    by_class_name: dict[str, list[dict[str, Any]]] = {}
    by_kind_class_name: dict[tuple[str, str], dict[str, Any]] = {}
    for model in models:
        cls = model["type"].split(".")[-1]
        by_kind_class[(model["kind"], cls)] = model
        by_kind_class_name[(model["kind"], cls.lower())] = model
        if model["kind"] in MODEL_KINDS:
            by_class_name.setdefault(cls, []).append(model)

    pool_specs = [
        ("CARD_POOL.SILENT_CARD_POOL", "card pool", "AllCardIds"),
        ("RELIC_POOL.SILENT_RELIC_POOL", "relic pool", "AllRelicIds"),
        ("POTION_POOL.SILENT_POTION_POOL", "potion pool", "AllPotionIds"),
    ]
    root_ids: list[tuple[str, str]] = []
    pools = []
    for pool_id, category, field in pool_specs:
        pool = by_id.get(pool_id)
        if pool is None:
            raise ValueError(f"Missing pool {pool_id} in content.json")
        ids = pool.get("props", {}).get(field)
        if not isinstance(ids, list) or not all(isinstance(x, str) for x in ids):
            raise ValueError(f"{pool_id} has no valid {field}")
        pools.append({"id": pool_id, "category": category, "members": ids})
        root_ids.extend((member, category) for member in ids)
    root_ids.append(("CARD_POOL.SILENT_CARD_POOL", "card pool"))
    root_ids.append(("RELIC_POOL.SILENT_RELIC_POOL", "relic pool"))
    root_ids.append(("POTION_POOL.SILENT_POTION_POOL", "potion pool"))

    refs_by_model: dict[str, list[str]] = {}
    unresolved: list[dict[str, str]] = []
    branches_by_source: dict[str, list[dict[str, Any]]] = {}
    source_texts: dict[str, str] = {}
    class_source_paths: dict[str, list[str]] = {}
    for path in sorted((source_root / "Models").rglob("*.cs")):
        body = path.read_text(encoding="utf-8")
        masked_body = mask_comments_and_literals(body)
        rel = str(path.relative_to(source_root)).replace(os.sep, "/")
        for class_name in re.findall(r"\b(?:class|struct|record)\s+(\w+)", masked_body):
            class_source_paths.setdefault(class_name, []).append(rel)
    for model in models:
        rel = model.get("source")
        if not rel:
            continue
        path = source_root / rel
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8")
        source_texts[rel] = text
        branches_by_source[rel] = source_branches(text)
        refs: list[str] = []
        clean = mask_comments_and_literals(text)
        ref_names = {m.group(1) for m in GENERIC_TYPE_RE.finditer(clean)}
        ref_names.update(m.group(1) for m in STATIC_CREATE_RE.finditer(clean))
        namespaces = set(re.findall(r"\busing\s+MegaCrit\.Sts2\.Core\.Models\.(Cards|Powers|Relics|Potions|Afflictions|Enchantments|Orbs)\s*;", clean))
        kind_by_namespace = {
            "Cards": "CardModel", "Powers": "PowerModel", "Relics": "RelicModel",
            "Potions": "PotionModel", "Afflictions": "AfflictionModel",
            "Enchantments": "EnchantmentModel", "Orbs": "OrbModel",
        }
        visible_kinds = {kind_by_namespace[name] for name in namespaces if name in kind_by_namespace}
        for class_name in sorted(ref_names):
            candidates = by_class_name.get(class_name, [])
            if not candidates:
                continue
            narrowed = [candidate for candidate in candidates if candidate["kind"] in visible_kinds]
            if len(narrowed) == 1:
                refs.append(narrowed[0]["id"])
            elif len(candidates) == 1:
                refs.append(candidates[0]["id"])
            else:
                unresolved.append({"source_model": model["id"], "source": rel, "kind": "ambiguous", "class": class_name})
        refs_by_model[model["id"]] = sorted(set(refs))

    closure: dict[str, str] = {}
    queue = list(root_ids)
    while queue:
        model_id, origin = queue.pop(0)
        if model_id in closure:
            continue
        model = by_id.get(model_id)
        if not model:
            raise ValueError(f"Pool or model reference {model_id} is not an exported model")
        closure[model_id] = origin
        for ref in refs_by_model.get(model_id, []):
            queue.append((ref, f"generated/reference from {model_id}"))

    source_to_cpp = gd.SOURCE_NAMES
    cpp_classes: dict[str, str] = {}
    model_dirs = ROOT / "game" / "Models"
    for header in sorted(model_dirs.rglob("*.h")):
        body = header.read_text(encoding="utf-8")
        for cls in re.findall(r"^(?:struct|class) (\w+)\s*(?:final\s*)?:", body, re.M):
            cpp_classes.setdefault(cls, str(header.relative_to(ROOT)))

    usable = dispatch_ids()
    entries = []
    for model_id, origin in sorted(closure.items(), key=lambda pair: (by_id[pair[0]]["kind"], pair[0])):
        model = by_id[model_id]
        rel = model.get("source", "")
        source_path = source_root / rel if rel else None
        source_text = source_texts.get(rel, "")
        presence = model_cpp_presence(model, source_to_cpp, cpp_classes)
        own_refs = refs_by_model.get(model_id, [])
        overrides = model.get("overrides", [])
        branch_rows = branches_by_source.get(rel, [])
        # Include conditional sites in the actual base method when the
        # exporter says this model inherits an override via that base class.
        for override in overrides:
            via = re.search(r"\(via\s+(\w+)\)", override)
            if not via:
                continue
            base_name = via.group(1)
            member = override.split(".", 1)[1].split(" ", 1)[0]
            if member.startswith("get_"):
                member = member[4:]
            candidates = class_source_paths.get(base_name, [])
            if len(candidates) != 1:
                unresolved.append({"source_model": model_id, "source": rel, "kind": "base-source", "class": base_name})
                continue
            base_rel = candidates[0]
            if base_rel not in branches_by_source:
                base_text = (source_root / base_rel).read_text(encoding="utf-8")
                source_texts[base_rel] = base_text
                branches_by_source[base_rel] = source_branches(base_text)
            for branch in branches_by_source[base_rel]:
                if branch["member"] == member:
                    inherited = dict(branch)
                    inherited["source"] = base_rel
                    inherited["via"] = base_name
                    branch_rows = branch_rows + [inherited]
        branch_rows = sorted(branch_rows, key=lambda item: (item.get("source", rel), item["line"], item["kind"], item["condition"]))
        stub_markers = []
        if presence["header"]:
            header_text = (ROOT / presence["header"]).read_text(encoding="utf-8")
            for marker in ("SIM_NOT_PORTED", "throw_unported", "throw Unported", "Unported("):
                if marker in header_text:
                    stub_markers.append(marker)
        dispatch_kind, _id_enum, _folder, _handle = gd.KINDS.get(model["kind"], ("", "", "", ""))
        enum_name = gd.id_enum_name(model_id)
        entries.append({
            "id": model_id,
            "kind": model["kind"],
            "type": model["type"],
            "origin": origin,
            "source": rel,
            "source_exists": bool(source_path and source_path.is_file()),
            "overrides": overrides,
            "branches": branch_rows,
            "cpp_class": presence["cpp_class"],
            "cpp_header": presence["header"],
            "runtime_representation": (
                "content tables / generic pool queries" if model["kind"] in {"CardPoolModel", "RelicPoolModel", "PotionPoolModel"}
                else "concrete C++ model" if presence["header"] else "missing concrete C++ model"
            ),
            "is_usable": enum_name in usable.get(dispatch_kind, set()) if dispatch_kind else None,
            "unported_markers": stub_markers,
            "model_refs": own_refs,
        })
    return {
        "source_root": str(source_root),
        "pools": pools,
        "entries": entries,
        "unresolved_refs": unresolved,
        "notes": [
            "The pool denominator and model override arrays come from data/content.json exported from STS2 v0.111.0.",
            "Branch scan records explicit if/switch/loop/catch, switch arms, else, ternary and short-circuit/null-coalescing sites in each model source file.",
            "Known model types in generic arguments and static Create calls create the recursive dependency closure, including generated cards and powers.",
            "For each inherited override marked '(via X)', branch sites inside X's matching method/property are included as well.",
            "C++ header, IsUsable and absence of obvious Unported markers are triage hints only; source review and Oracle behavior checks are still required.",
        ],
    }


def render_markdown(report: dict[str, Any]) -> str:
    entries = report["entries"]
    by_kind: dict[str, list[dict[str, Any]]] = {}
    for entry in entries:
        by_kind.setdefault(entry["kind"], []).append(entry)
    out = [
        "# Silent v0.111.0 源码模型、override 与分支清单",
        "",
        "源码根目录：`STS2_SRC`（默认 `../Slay the Spire 2/src/Core`）。池成员/override 来自仓库的 v0.111.0 `data/content.json`；请用 `python3 tools/coverage/silent_source_inventory.py --check` 重生校验。",
        "",
        "> 本文是源码审计分母，不把头文件、`IsUsable` 或无明显 stub 当成行为完成。每个模型的所有 override 与 C# 控制流检查点均列出；生成依赖来自源码中的模型类型引用闭包，继承 override 会追到实际声明源。逐项完成状态见 `docs/reports/silent-v0110/final-report.md`。",
        "",
        "## 池分母",
        "",
        "| 原版池 | 成员数 | 成员 |",
        "|---|---:|---|",
    ]
    for pool in report["pools"]:
        members = ", ".join(f"`{x}`" for x in pool["members"])
        out.append(f"| `{pool['id']}` | {len(pool['members'])} | {members} |")
    out.extend(["", "## 模型逐项清单", ""])
    for kind in sorted(by_kind):
        rows = by_kind[kind]
        out.extend([f"### {kind}（{len(rows)}）", ""])
        for row in rows:
            state = "header present" if row["cpp_header"] else row["runtime_representation"]
            if row["unported_markers"]:
                state += "; stub marker: " + ", ".join(row["unported_markers"])
            state += "; `IsUsable`=" + ("N/A" if row["is_usable"] is None else "yes" if row["is_usable"] else "no")
            out.append(f"#### `{row['id']}`")
            out.append("")
            out.append(f"- 类别/可达来源：`{row['origin']}`；源：`{row['source'] or 'missing source mapping'}`；C++：`{row['cpp_header'] or row['runtime_representation']}`（{state}）")
            if row["overrides"]:
                out.append(f"- C# override 清单（{len(row['overrides'])}）：")
                for override in row["overrides"]:
                    out.append(f"  - `{override}`")
            else:
                out.append("- C# override 清单：无显式 override")
            out.append(f"- 生成/模型依赖：" + (", ".join(f"`{x}`" for x in row["model_refs"]) if row["model_refs"] else "（无已识别的模型类型引用）"))
            if row["branches"]:
                out.append(f"- 源码分支/控制流检查点（{len(row['branches'])}）：")
                for branch in row["branches"]:
                    condition = branch["condition"].replace("|", "\\|").replace("`", "\\`")
                    origin = branch.get("source", row["source"])
                    via = f"（继承自 `{branch['via']}`）" if branch.get("via") else ""
                    out.append(f"  - `{origin}:{branch['line']}` **{branch['kind']} / {branch['member']}** `{condition}`{via}")
            else:
                out.append("- 源码分支/控制流检查点：0")
            out.append("")
    out.extend(["## 缺失的 C# 源文件", ""])
    missing_sources = [row for row in entries if not row["source_exists"]]
    out.append("无。" if not missing_sources else "\n".join(f"- `{x['id']}`: `{x['source']}`" for x in missing_sources))
    out.extend(["", "## 未解析的源码或依赖映射", ""])
    unresolved = report["unresolved_refs"]
    out.append("无。" if not unresolved else "\n".join(f"- `{x['source_model']}` `{x['source']} -> {x['kind']}.{x['class']}`" for x in unresolved))
    out.extend(["", "## 计数与口径", "", f"- 闭包模型：{len(entries)}。", f"- 缺具体 C++ 模型头文件（不含表驱动的池模型）：{sum(x['kind'] in MODEL_KINDS and not x['cpp_header'] for x in entries)}。", f"- 缺 `IsUsable`（不含表驱动的池模型）：{sum(x['is_usable'] is False for x in entries)}。", f"- 有明显 Unported stub 标记的现存头文件：{sum(bool(x['unported_markers']) for x in entries)}。", f"- 源码控制流检查点（含继承 override）：{sum(len(x['branches']) for x in entries)}。", "- 依赖闭包由已知模型类型的泛型参数与静态 `Create*` 引用构造；源审计还要核查非泛型工厂与运行时 Hook。", "- CardPoolModel/RelicPoolModel/PotionPoolModel 在 Env 中无独立具体类；其池成员通过导出的内容表，需逐项核对全解锁路径与池源过滤条件。", ""])
    return "\n".join(out)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=default_source(), help="v0.111.0 src/Core root")
    parser.add_argument("--out", type=Path, default=ROOT / "docs/audits/silent-source-inventory.md")
    parser.add_argument("--check", action="store_true", help="fail if the generated report differs from --out")
    parser.add_argument("--json", action="store_true", help="emit the full inventory JSON to stdout")
    args = parser.parse_args()
    if not args.source.is_dir():
        parser.error(f"source directory does not exist: {args.source}")
    report = build_inventory(args.source.resolve())
    rendered = render_markdown(report)
    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
    elif args.check:
        existing = args.out.read_text(encoding="utf-8") if args.out.exists() else ""
        if existing != rendered:
            print(f"stale source inventory: {args.out}", file=sys.stderr)
            return 1
        print(f"source inventory current: {args.out}")
    else:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(rendered, encoding="utf-8")
        print(f"wrote {args.out}")
        print(f"{len(report['entries'])} models; {sum(len(x['branches']) for x in report['entries'])} source control-flow sites; {len(report['unresolved_refs'])} unresolved model references")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
