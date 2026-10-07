#!/usr/bin/env python3
"""Generate C++ content tables from data/content.json (tools/Sts2Exporter).

Outputs into content/generated/:
  enums.h    game enums (CardType, CardKeyword, ValueProp, ...)
  ids.h      ModelKind and one dense id enum per content kind, plus names
  members.h  every overridable framework member (hooks and behaviours) and
             MemberSet, the per-model override bitset
  tables.h   static data structs and accessors
  tables.cpp the data

Usage: tools/gen/gen_content.py [data/content.json] [content/generated]
"""
import json
import os
import re
import sys
from collections import OrderedDict

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "data", "content.json")
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "content", "generated")

# ModelKind -> (framework classes, id category, C++ id enum name)
KINDS = OrderedDict([
    ("Card", (["CardModel"], "CARD", "CardId")),
    ("Relic", (["RelicModel"], "RELIC", "RelicId")),
    ("Power", (["PowerModel"], "POWER", "PowerId")),
    ("Potion", (["PotionModel"], "POTION", "PotionId")),
    ("Monster", (["MonsterModel"], "MONSTER", "MonsterId")),
    ("Encounter", (["EncounterModel"], "ENCOUNTER", "EncounterId")),
    ("Event", (["EventModel", "AncientEventModel"], "EVENT", "EventId")),
    ("Enchantment", (["EnchantmentModel"], "ENCHANTMENT", "EnchantmentId")),
    ("Affliction", (["AfflictionModel"], "AFFLICTION", "AfflictionId")),
    ("Orb", (["OrbModel"], "ORB", "OrbId")),
    ("Character", (["CharacterModel"], "CHARACTER", "CharacterId")),
    ("Act", (["ActModel"], "ACT", "ActId")),
    ("CardPool", (["CardPoolModel"], "CARD_POOL", "CardPoolId")),
    ("RelicPool", (["RelicPoolModel"], "RELIC_POOL", "RelicPoolId")),
    ("PotionPool", (["PotionPoolModel"], "POTION_POOL", "PotionPoolId")),
    ("Modifier", (["ModifierModel"], "MODIFIER", "ModifierId")),
])

# Identifiers that collide with common C/POSIX macros.
MACRO_NAMES = {
    "EOF", "NULL", "TRUE", "FALSE", "DOMAIN", "OVERFLOW", "UNDERFLOW", "SING", "TLOSS", "PLOSS", "INFINITY",
    "NAN", "EDOM", "ERANGE", "EILSEQ", "HUGE_VAL", "BUFSIZ", "CHAR_BIT", "RAND_MAX", "EXIT_SUCCESS",
    "EXIT_FAILURE", "unix", "linux", "SIGABRT", "SIGSEGV", "SIGINT", "SIGTERM", "SIGKILL", "SIGALRM",
    "stdin", "stdout", "stderr", "errno", "assert", "NONE_", "Count",
}


def ident(name):
    s = re.sub(r"[^A-Za-z0-9_]", "_", name)
    if not s or s[0].isdigit():
        s = "_" + s
    if s in MACRO_NAMES:
        s += "_"
    return s


def cstr(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def dec_lit(text):
    """C# decimal.ToString() -> Dec::lit(mantissa, scale)."""
    text = str(text)
    m = re.fullmatch(r"(-?)(\d+)(?:\.(\d+))?", text)
    if not m:
        raise ValueError("bad decimal " + text)
    sign, whole, frac = m.group(1), m.group(2), m.group(3) or ""
    mant = int(whole + frac)
    if mant >= 2**63:
        raise ValueError("decimal literal too large: " + text)
    return "Dec::lit(%s%d, %d)" % (sign, mant, len(frac))


class Gen:
    def __init__(self, data):
        self.data = data
        self.enums = data["enums"]
        # Test content: Mocks namespaces, plus a few MOCK_* models outside them.
        self.models = [m for m in data["models"]
                       if not m.get("mock") and not m["id"].split(".", 1)[1].startswith("MOCK_")]
        self.by_kind = OrderedDict((k, []) for k in KINDS)
        kind_of_class = {c: k for k, (classes, _, _) in KINDS.items() for c in classes}
        for m in self.models:
            k = kind_of_class.get(m["kind"])
            if k:
                self.by_kind[k].append(m)
        # id string -> (kind, dense index); index 0 is NONE.
        self.index = {}
        for k, ms in self.by_kind.items():
            for i, m in enumerate(ms, start=1):
                self.index[m["id"]] = (k, i)
        self.members = self.build_members()

    # ------------------------------------------------------------ helpers

    def ref(self, idstr, kind):
        if idstr is None:
            return "%s::NONE" % KINDS[kind][2]
        k, _ = self.index[idstr]
        if k != kind:
            raise ValueError("%s is %s, expected %s" % (idstr, k, kind))
        return "%s::%s" % (KINDS[kind][2], ident(idstr.split(".", 1)[1]))

    def enum_val(self, enum, name):
        for v in self.enums[enum]["values"]:
            if v["name"] == name:
                return v["value"]
        raise KeyError("%s.%s" % (enum, name))

    def enum_ref(self, enum, name):
        if name is None:
            return "%s{}" % enum
        if self.enums[enum]["flags"]:
            total = 0
            for part in [p.strip() for p in str(name).split(",")]:
                total |= self.enum_val(enum, part)
            return "static_cast<%s>(%d)" % (enum, total)
        self.enum_val(enum, name)
        return "%s::%s" % (enum, ident(name))

    def bitmask(self, enum, names):
        mask = 0
        for n in names or []:
            v = self.enum_val(enum, n)
            if v >= 64:
                raise ValueError("%s.%s does not fit a 64-bit mask" % (enum, n))
            mask |= 1 << v
        return "0x%xULL" % mask

    def build_members(self):
        members = []
        for cls, arr in self.data["virtuals"].items():
            seen = {}
            for v in arr:
                base = cls + "_" + v["name"]
                seen[base] = seen.get(base, 0) + 1
                name = base if seen[base] == 1 else "%s_%d" % (base, seen[base])
                members.append((name, cls + "." + v["name"], seen[base] - 1, v))
        return members

    def member_set(self, overrides):
        index = {}
        for i, (name, key, dup, _) in enumerate(self.members):
            index.setdefault(key, []).append(i)
        words = [0] * self.member_words()
        for o in overrides:
            key = o.split(" (via ")[0]
            for i in index.get(key, []):  # overloads share a name; mark all of them
                words[i // 64] |= 1 << (i % 64)
        return "MemberSet{{%s}}" % ", ".join("0x%xULL" % w for w in words)

    def member_words(self):
        return (len(self.members) + 63) // 64

    # ------------------------------------------------------------ files

    def enums_h(self):
        out = [HEADER, "#pragma once", "", "#include <cstdint>", "#include <string_view>", "", "namespace sts2 {", ""]
        cpp_types = {"Int32": "int32_t", "UInt32": "uint32_t", "Int16": "int16_t", "UInt16": "uint16_t",
                     "Byte": "uint8_t", "SByte": "int8_t", "Int64": "int64_t", "UInt64": "uint64_t"}
        for name, e in sorted(self.enums.items()):
            cname = ident(name.split(".")[-1]) if "." not in name else ident(name.replace(".", "_"))
            out.append("// %s.%s%s" % (e["namespace"], name.split(".")[-1], " [Flags]" if e["flags"] else ""))
            # Plain enums get the smallest type that holds their values, so
            # simulation state stays compact; [Flags] enums keep the C# type.
            ut = cpp_types[e["underlying"]]
            vals = [v["value"] for v in e["values"]] or [0]
            if not e["flags"]:
                if min(vals) >= 0 and max(vals) <= 255:
                    ut = "uint8_t"
                elif min(vals) >= -128 and max(vals) <= 127:
                    ut = "int8_t"
                elif min(vals) >= -32768 and max(vals) <= 32767:
                    ut = "int16_t"
            out.append("enum class %s : %s {" % (cname, ut))
            for v in e["values"]:
                out.append("    %s = %d," % (ident(v["name"]), v["value"]))
            out.append("};")
            out.append("constexpr std::string_view name(%s v) {" % cname)
            out.append("    switch (v) {")
            seen_vals = set()
            for v in e["values"]:
                if v["value"] in seen_vals:
                    continue
                seen_vals.add(v["value"])
                out.append("        case %s::%s: return %s;" % (cname, ident(v["name"]), cstr(v["name"])))
            out.append("    }")
            out.append('    return "";')
            out.append("}")
            if e["flags"]:
                out.append("constexpr %s operator|(%s a, %s b) { return static_cast<%s>(static_cast<%s>(a) | static_cast<%s>(b)); }"
                           % (cname, cname, cname, cname, ut, ut))
                out.append("constexpr %s operator&(%s a, %s b) { return static_cast<%s>(static_cast<%s>(a) & static_cast<%s>(b)); }"
                           % (cname, cname, cname, cname, ut, ut))
                out.append("constexpr %s operator~(%s a) { return static_cast<%s>(~static_cast<%s>(a)); }"
                           % (cname, cname, cname, ut))
                out.append("constexpr bool has_flag(%s v, %s f) { return (static_cast<%s>(v) & static_cast<%s>(f)) == static_cast<%s>(f); }"
                           % (cname, cname, ut, ut, ut))
            out.append("")
        out.append("}  // namespace sts2")
        return "\n".join(out) + "\n"

    def ids_h(self):
        out = [HEADER, "#pragma once", "", "#include <array>", "#include <cstdint>", "#include <string_view>", "",
               "namespace sts2 {", "",
               "// Model id hash of the exported game build (ModelIdSerializationCache.Hash).",
               "inline constexpr uint32_t kModelIdHash = %du;" % self.data["meta"]["model_id_hash"], "",
               "enum class ModelKind : uint8_t {"]
        for k in KINDS:
            out.append("    %s," % k)
        out += ["    Count", "};", ""]
        for k, ms in self.by_kind.items():
            enum = KINDS[k][2]
            out.append("// %d %s models, in ModelDb registration order." % (len(ms), k))
            out.append("enum class %s : uint16_t {" % enum)
            out.append("    NONE = 0,")
            for m in ms:
                out.append("    %s," % ident(m["id"].split(".", 1)[1]))
            out.append("    Count")
            out.append("};")
            out.append("inline constexpr std::array<std::string_view, %d> k%sNames = {" % (len(ms) + 1, enum))
            out.append('    "NONE",')
            for m in ms:
                out.append("    %s," % cstr(m["id"].split(".", 1)[1]))
            out.append("};")
            # Ordinal rank of the entry string, for ModelId.CompareTo (StableShuffle).
            ranked = sorted(range(len(ms)), key=lambda i: ms[i]["id"].split(".", 1)[1].encode("utf-16-be"))
            rank = [0] * (len(ms) + 1)
            for r, i in enumerate(ranked, start=1):
                rank[i + 1] = r
            out.append("// Rank of each entry under ordinal (UTF-16) string comparison; NONE sorts first.")
            out.append("inline constexpr std::array<uint16_t, %d> k%sOrdinalRank = {%s};"
                       % (len(ms) + 1, enum, ", ".join(str(r) for r in rank)))
            out.append("inline std::string_view name(%s id) { return k%sNames[static_cast<size_t>(id)]; }" % (enum, enum))
            out.append("")
        out.append("}  // namespace sts2")
        return "\n".join(out) + "\n"

    def members_h(self):
        out = [HEADER, "#pragma once", "", "#include <array>", "#include <cstdint>", "#include <string_view>", "",
               "namespace sts2 {", "",
               "// Every overridable member introduced by the game's framework model classes",
               "// (AbstractModel = the hook universe, then CardModel, RelicModel, ...), in",
               "// declaration order. Overloads get a numeric suffix.",
               "enum class Member : uint16_t {"]
        for name, key, dup, v in self.members:
            params = ", ".join(v["params"])
            out.append("    %s,  // %s %s(%s)" % (ident(name), v["returns"], v["name"], params))
        out += ["    Count", "};", "",
                "inline constexpr size_t kMemberWords = %d;" % self.member_words(), "",
                "// Which framework members a content class overrides.",
                "struct MemberSet {",
                "    std::array<uint64_t, kMemberWords> w{};",
                "    constexpr bool has(Member m) const {",
                "        auto i = static_cast<size_t>(m);",
                "        return (w[i / 64] >> (i % 64)) & 1;",
                "    }",
                "};", "",
                "inline constexpr std::array<std::string_view, %d> kMemberNames = {" % len(self.members)]
        for name, key, dup, v in self.members:
            out.append("    %s," % cstr(name))
        out += ["};", "", "}  // namespace sts2"]
        return "\n".join(out) + "\n"

    # ------------------------------------------------------------ tables

    def var_types(self):
        names = set()
        for m in self.models:
            for v in m.get("vars", []):
                names.add(v["type"].split("<")[0])
        return sorted(names)

    def var_defs(self, m, arrays, label):
        vars_ = m.get("vars", [])
        if not vars_:
            return "{}"
        rows = []
        for v in vars_:
            base_type = v["type"].split("<")[0]
            power = "PowerId::NONE"
            g = re.match(r"PowerVar<(\w+)>", v["type"])
            if g:
                pid = "POWER." + re.sub(r"(?<!^)(?=[A-Z])", "_", g.group(1)).upper()
                if pid in self.index:
                    power = self.ref(pid, "Power")
            props = (v.get("props") or {}).get("Props")
            rows.append("{%s, VarType::%s, %s, %s, %s}" % (
                cstr(v["name"]), base_type, power,
                self.enum_ref("ValueProp", props) if props else "ValueProp{}", dec_lit(v["base"])))
        arrays.append("const VarDef %s[] = {%s};" % (label, ", ".join(rows)))
        return "std::span<const VarDef>(%s)" % label

    def id_span(self, arrays, label, ids, kind):
        ids = ids or []
        if not ids:
            return "{}"
        arrays.append("const %s %s[] = {%s};" % (KINDS[kind][2], label, ", ".join(self.ref(i, kind) for i in ids)))
        return "std::span<const %s>(%s)" % (KINDS[kind][2], label)

    def card_levels(self, m, arrays, label):
        """Card state at each upgrade level, derived from the exporter's diffs."""
        p = m["props"]
        var_names = [v["name"] for v in m.get("vars", [])]
        values = {v["name"]: v["base"] for v in m.get("vars", [])}
        cost = p["EnergyCost"]["_base"]
        keywords = list(p.get("Keywords") or [])
        levels = [(cost, list(keywords), [values[n] for n in var_names])]
        for u in m.get("upgrades", []):
            if "error" in u:
                raise ValueError("%s upgrade error: %s" % (m["id"], u["error"]))
            up = u.get("props") or {}
            if "EnergyCost" in up:
                cost = up["EnergyCost"]["_base"]
            if "Keywords" in up:
                keywords = list(up["Keywords"])
            for v in u.get("vars") or []:
                if v["name"] not in values:
                    raise ValueError("%s: upgrade adds var %s" % (m["id"], v["name"]))
                values[v["name"]] = v["base"]
            levels.append((cost, list(keywords), [values[n] for n in var_names]))
        rows = []
        for i, (c, kw, vals) in enumerate(levels):
            vlabel = "%s_v%d" % (label, i)
            if vals:
                arrays.append("const Dec %s[] = {%s};" % (vlabel, ", ".join(dec_lit(x) for x in vals)))
                vspan = "std::span<const Dec>(%s)" % vlabel
            else:
                vspan = "{}"
            rows.append("{%d, %s, %s}" % (c, self.bitmask("CardKeyword", kw), vspan))
        arrays.append("const CardLevel %s[] = {%s};" % (label, ", ".join(rows)))
        return "std::span<const CardLevel>(%s)" % label

    def asc(self, m, level, prop, default):
        diff = (m.get("ascension") or {}).get(level) or {}
        return (diff.get("props") or {}).get(prop, default)

    def tables(self):
        arrays = []
        rows = OrderedDict()

        def lab(kind, m, suffix):
            return "k%s_%s_%s" % (kind, ident(m["id"].split(".", 1)[1]), suffix)

        rows["Card"] = []
        for m in self.by_kind["Card"]:
            p = m["props"]
            rows["Card"].append("{%s, CardType::%s, CardRarity::%s, TargetType::%s, %s, %d, %d, %s, %s, %s, %s, %s, %s}" % (
                cstr(m["id"].split(".", 1)[1]), p["Type"], p["Rarity"], p["TargetType"],
                "true" if p["EnergyCost"]["CostsX"] else "false", p["CanonicalStarCost"], p["MaxUpgradeLevel"],
                self.ref(p.get("Pool"), "CardPool") if p.get("Pool") in self.index else "CardPoolId::NONE",
                self.bitmask("CardTag", p.get("Tags")),
                self.enum_ref("CardMultiplayerConstraint", p.get("MultiplayerConstraint")),
                b(p.get("CanBeGeneratedInCombat", True)),
                self.var_defs(m, arrays, lab("Card", m, "vars")),
                self.card_levels(m, arrays, lab("Card", m, "levels"))))

        rows["Relic"] = []
        for m in self.by_kind["Relic"]:
            p = m["props"]
            rows["Relic"].append("{%s, RelicRarity::%s, %s, %d, %s, %s, %s, %s, %s, %s}" % (
                cstr(m["id"].split(".", 1)[1]), p["Rarity"],
                self.ref(p.get("Pool"), "RelicPool") if p.get("Pool") in self.index else "RelicPoolId::NONE",
                p.get("MerchantCost", 0),
                b(p.get("IsStackable")), b(p.get("IsAllowedInShops")), b(p.get("IsTradable")),
                b(p.get("HasUponPickupEffect")), b(p.get("SpawnsPets")),
                self.var_defs(m, arrays, lab("Relic", m, "vars"))))

        rows["Power"] = []
        for m in self.by_kind["Power"]:
            p = m["props"]
            rows["Power"].append("{%s, PowerType::%s, PowerStackType::%s, PowerInstanceType::%s, %s, %s, %s}" % (
                cstr(m["id"].split(".", 1)[1]), p["Type"], p["StackType"], p["InstanceType"],
                b(p.get("AllowNegative")), b(p.get("ShouldScaleInMultiplayer")),
                self.var_defs(m, arrays, lab("Power", m, "vars"))))

        rows["Potion"] = []
        for m in self.by_kind["Potion"]:
            p = m["props"]
            rows["Potion"].append("{%s, PotionRarity::%s, TargetType::%s, PotionUsage::%s, %s, %s, %s}" % (
                cstr(m["id"].split(".", 1)[1]), p["Rarity"], p["TargetType"], p["Usage"],
                self.ref(p.get("Pool"), "PotionPool") if p.get("Pool") in self.index else "PotionPoolId::NONE",
                b(p.get("CanBeGeneratedInCombat")),
                self.var_defs(m, arrays, lab("Potion", m, "vars"))))

        rows["Monster"] = []
        for m in self.by_kind["Monster"]:
            p = m["props"]
            lo, hi = p.get("MinInitialHp", 0), p.get("MaxInitialHp", 0)
            rows["Monster"].append("{%s, %d, %d, %d, %d}" % (
                cstr(m["id"].split(".", 1)[1]), lo, hi,
                self.asc(m, "ToughEnemies", "MinInitialHp", lo), self.asc(m, "ToughEnemies", "MaxInitialHp", hi)))

        rows["Encounter"] = []
        for m in self.by_kind["Encounter"]:
            p = m["props"]
            glo, ghi = p.get("MinGoldReward", 0), p.get("MaxGoldReward", 0)
            slots = p.get("Slots") or []
            if slots:
                slabel = lab("Encounter", m, "slots")
                arrays.append("const std::string_view %s[] = {%s};" % (slabel, ", ".join(cstr(x) for x in slots)))
                sspan = "std::span<const std::string_view>(%s)" % slabel
            else:
                sspan = "{}"
            rows["Encounter"].append("{%s, RoomType::%s, %s, %s, %d, %d, %d, %d, %s, %s, %s}" % (
                cstr(m["id"].split(".", 1)[1]), p["RoomType"], b(p.get("IsWeak")), b(p.get("ShouldGiveRewards")),
                glo, ghi, self.asc(m, "Poverty", "MinGoldReward", glo), self.asc(m, "Poverty", "MaxGoldReward", ghi),
                self.bitmask("EncounterTag", p.get("Tags")),
                self.id_span(arrays, lab("Encounter", m, "monsters"), p.get("AllPossibleMonsters"), "Monster"),
                sspan))

        rows["Event"] = []
        for m in self.by_kind["Event"]:
            p = m["props"]
            rows["Event"].append("{%s, %s, %s, %s, EventLayoutType::%s, %s}" % (
                cstr(m["id"].split(".", 1)[1]), b(m["kind"] == "AncientEventModel"), b(p.get("IsShared")),
                b(p.get("IsDeterministic")), p.get("LayoutType", "Default"),
                self.var_defs(m, arrays, lab("Event", m, "vars"))))

        for k in ("Enchantment", "Affliction", "Orb", "Modifier"):
            rows[k] = []
            for m in self.by_kind[k]:
                p = m["props"]
                rows[k].append("{%s, %s, %s}" % (
                    cstr(m["id"].split(".", 1)[1]), b(p.get("IsStackable")),
                    self.var_defs(m, arrays, lab(k, m, "vars"))))

        rows["Character"] = []
        for m in self.by_kind["Character"]:
            p = m["props"]
            rows["Character"].append("{%s, %s, %d, %d, %d, %d, %s, %s, %s, %s, %s, %s}" % (
                cstr(m["id"].split(".", 1)[1]), b(p.get("IsPlayable")), p.get("StartingHp", 0),
                p.get("StartingGold", 0), p.get("MaxEnergy", 0), p.get("BaseOrbSlotCount", 0),
                self.ref(p.get("CardPool"), "CardPool") if p.get("CardPool") in self.index else "CardPoolId::NONE",
                self.ref(p.get("RelicPool"), "RelicPool") if p.get("RelicPool") in self.index else "RelicPoolId::NONE",
                self.ref(p.get("PotionPool"), "PotionPool") if p.get("PotionPool") in self.index else "PotionPoolId::NONE",
                self.id_span(arrays, lab("Character", m, "deck"), p.get("StartingDeck"), "Card"),
                self.id_span(arrays, lab("Character", m, "relics"), p.get("StartingRelics"), "Relic"),
                self.id_span(arrays, lab("Character", m, "potions"), p.get("StartingPotions"), "Potion")))

        rows["Act"] = []
        for m in self.by_kind["Act"]:
            p = m["props"]
            rows["Act"].append("{%s, %d, %s, %s, %s, %s, %s, %s, %s, %s, %s}" % (
                cstr(m["id"].split(".", 1)[1]), p.get("Index", -1), b(p.get("IsDefault")),
                self.id_span(arrays, lab("Act", m, "weak"), p.get("AllWeakEncounters"), "Encounter"),
                self.id_span(arrays, lab("Act", m, "regular"), p.get("AllRegularEncounters"), "Encounter"),
                self.id_span(arrays, lab("Act", m, "elite"), p.get("AllEliteEncounters"), "Encounter"),
                self.id_span(arrays, lab("Act", m, "boss"), p.get("AllBossEncounters"), "Encounter"),
                self.id_span(arrays, lab("Act", m, "boss_order"), p.get("BossDiscoveryOrder"), "Encounter"),
                self.id_span(arrays, lab("Act", m, "events"), p.get("AllEvents"), "Event"),
                self.id_span(arrays, lab("Act", m, "ancients"), p.get("AllAncients"), "Event"),
                self.id_span(arrays, lab("Act", m, "monsters"), p.get("AllMonsters"), "Monster")))

        for k, prop, member in (("CardPool", "AllCards", "Card"), ("RelicPool", "AllRelics", "Relic"),
                                ("PotionPool", "AllPotions", "Potion")):
            rows[k] = []
            for m in self.by_kind[k]:
                rows[k].append("{%s, %s}" % (
                    cstr(m["id"].split(".", 1)[1]),
                    self.id_span(arrays, lab(k, m, "items"), m["props"].get(prop), member)))

        # Generic per-model info (all kinds).
        info_rows = []
        for k, ms in self.by_kind.items():
            for i, m in enumerate(ms, start=1):
                info_rows.append("{ModelKind::%s, %d, %s, %s, %s, %s}" % (
                    k, i, cstr(m["id"]), cstr(m["type"]), cstr(m.get("source", "")), self.member_set(m["overrides"])))

        # Ascension-dependent numeric props beyond the ones folded into the rows above.
        asc_rows = []
        folded = {("Monster", "MinInitialHp"), ("Monster", "MaxInitialHp"),
                  ("Encounter", "MinGoldReward"), ("Encounter", "MaxGoldReward")}
        for k, ms in self.by_kind.items():
            for i, m in enumerate(ms, start=1):
                for level, diff in (m.get("ascension") or {}).items():
                    if level == "error":
                        continue
                    for prop, val in (diff.get("props") or {}).items():
                        if (k, prop) in folded or not isinstance(val, int) or isinstance(val, bool):
                            continue
                        asc_rows.append("{ModelKind::%s, %d, AscensionLevel::%s, %s, %d}" % (k, i, level, cstr(prop), val))
        return arrays, rows, info_rows, asc_rows

    def kind_of_category(self, idstr):
        cat = idstr.split(".", 1)[0]
        for k, (_, c, _) in KINDS.items():
            if c == cat:
                return k
        return None

    def modeldb_lists(self, arrays):
        """ModelDb's static lists as spans: (name, element C++ type, expression)."""
        out = []
        for name, value in sorted(self.data.get("modeldb", {}).items()):
            if not isinstance(value, list) or not value:
                continue
            if isinstance(value[0], list):  # ActsByIndex
                kind = self.kind_of_category(value[0][0])
                if kind is None:
                    continue
                inner = []
                for i, sub in enumerate(value):
                    inner.append(self.id_span(arrays, "kModelDb_%s_%d" % (name, i), sub, kind))
                arrays.append("const std::span<const %s> kModelDb_%s[] = {%s};" % (KINDS[kind][2], name, ", ".join(inner)))
                out.append((name, "std::span<const %s>" % KINDS[kind][2], "kModelDb_" + name))
                continue
            kind = self.kind_of_category(value[0])
            if kind is None or any(v not in self.index for v in value):
                continue
            arrays.append("const %s kModelDb_%s[] = {%s};" % (KINDS[kind][2], name, ", ".join(self.ref(v, kind) for v in value)))
            out.append((name, KINDS[kind][2], "kModelDb_" + name))
        return out

    def tables_h(self):
        vt = self.var_types()
        decls = ["std::span<const %s> %s();" % (t, snake_list(n)) for n, t, _ in self.modeldb_lists([])]
        return (TABLES_H.replace("@VARTYPES@", ",\n".join("    " + t for t in vt))
                .replace("@MODELDB@", "\n".join(decls)))

    def tables_cpp(self):
        arrays, rows, info_rows, asc_rows = self.tables()
        self.modeldb_lists_cached = self.modeldb_lists(arrays)
        out = [HEADER, '#include "content/generated/tables.h"', "", "namespace sts2 {", "", "namespace {", ""]
        out += arrays
        out.append("")
        struct_of = {"Card": "CardData", "Relic": "RelicData", "Power": "PowerData", "Potion": "PotionData",
                     "Monster": "MonsterData", "Encounter": "EncounterData", "Event": "EventData",
                     "Enchantment": "SimpleData", "Affliction": "SimpleData", "Orb": "SimpleData",
                     "Modifier": "SimpleData", "Character": "CharacterData", "Act": "ActData",
                     "CardPool": "CardPoolData", "RelicPool": "RelicPoolData", "PotionPool": "PotionPoolData"}
        for k, rs in rows.items():
            out.append("const %s k%sTable[] = {" % (struct_of[k], k))
            out.append("    {},  // NONE")
            for r in rs:
                out.append("    %s," % r)
            out.append("};")
            out.append("")
        out.append("const ModelInfo kModelInfos[] = {")
        for r in info_rows:
            out.append("    %s," % r)
        out.append("};")
        out.append("")
        out.append("const AscensionOverride kAscensionOverrides[] = {")
        for r in asc_rows:
            out.append("    %s," % r)
        out.append("};")
        out.append("")
        out.append("}  // namespace")
        out.append("")
        for k in rows:
            enum = KINDS[k][2]
            out.append("const %s& data(%s id) { return k%sTable[static_cast<size_t>(id)]; }" % (struct_of[k], enum, k))
            out.append("std::span<const %s> all_%s() { return std::span<const %s>(k%sTable).subspan(1); }"
                       % (struct_of[k], snake(k), struct_of[k], k))
        out.append("std::span<const ModelInfo> all_model_infos() { return kModelInfos; }")
        out.append("")
        out.append("namespace modeldb {")
        for n, t, arr in self.modeldb_lists_cached:
            out.append("std::span<const %s> %s() { return %s; }" % (t, snake_list(n), arr))
        out.append("}  // namespace modeldb")
        out.append("std::span<const AscensionOverride> ascension_overrides() { return kAscensionOverrides; }")
        out.append("")
        out.append("}  // namespace sts2")
        return "\n".join(out) + "\n"


def snake_list(name):
    return re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()


def b(v):
    return "true" if v else "false"


def snake(k):
    return re.sub(r"(?<!^)(?=[A-Z])", "_", k).lower() + "s"


HEADER = "// GENERATED by tools/gen/gen_content.py from data/content.json. Do not edit."

TABLES_H = HEADER + """
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "content/generated/enums.h"
#include "content/generated/ids.h"
#include "content/generated/members.h"
#include "core/dec.h"

namespace sts2 {

// DynamicVar subclass (generic argument stripped; PowerVar<T> keeps T in VarDef::power).
enum class VarType : uint8_t {
@VARTYPES@
};

struct VarDef {
    std::string_view name;
    VarType type;
    PowerId power;   // PowerVar<T>
    ValueProp props; // DamageVar/BlockVar value flags
    Dec base;
};

// Card state after N upgrades (levels[0] is the canonical card).
struct CardLevel {
    int32_t energy_cost;       // CardEnergyCost._base; -1 means unplayable
    uint64_t keywords;         // bit per CardKeyword
    std::span<const Dec> vars; // parallel to CardData::vars
};

struct CardData {
    std::string_view entry;
    CardType type;
    CardRarity rarity;
    TargetType target;
    bool costs_x;
    int32_t star_cost;  // CanonicalStarCost; -1 when the card has none
    int32_t max_upgrade_level;
    CardPoolId pool;
    uint64_t tags;  // bit per CardTag
    CardMultiplayerConstraint multiplayer;
    bool can_be_generated_in_combat;
    std::span<const VarDef> vars;
    std::span<const CardLevel> levels;
};

struct RelicData {
    std::string_view entry;
    RelicRarity rarity;
    RelicPoolId pool;
    int32_t merchant_cost;
    bool stackable;
    bool allowed_in_shops;
    bool tradable;
    bool has_upon_pickup_effect;
    bool spawns_pets;
    std::span<const VarDef> vars;
};

struct PowerData {
    std::string_view entry;
    PowerType type;
    PowerStackType stack_type;
    PowerInstanceType instance_type;
    bool allow_negative;
    bool scale_in_multiplayer;
    std::span<const VarDef> vars;
};

struct PotionData {
    std::string_view entry;
    PotionRarity rarity;
    TargetType target;
    PotionUsage usage;
    PotionPoolId pool;
    bool can_be_generated_in_combat;
    std::span<const VarDef> vars;
};

struct MonsterData {
    std::string_view entry;
    int32_t min_hp, max_hp;              // ascension below ToughEnemies
    int32_t min_hp_tough, max_hp_tough;  // AscensionLevel::ToughEnemies and above
};

struct EncounterData {
    std::string_view entry;
    RoomType room_type;
    bool is_weak;
    bool gives_rewards;
    int32_t min_gold, max_gold;                  // below Poverty
    int32_t min_gold_poverty, max_gold_poverty;  // AscensionLevel::Poverty and above
    uint64_t tags;                               // bit per EncounterTag
    std::span<const MonsterId> possible_monsters;
    std::span<const std::string_view> slots;     // Slots (monster slot names, in order)
};

struct EventData {
    std::string_view entry;
    bool is_ancient;
    bool is_shared;
    bool is_deterministic;
    EventLayoutType layout;
    std::span<const VarDef> vars;  // CanonicalVars
};

// Enchantments, afflictions, orbs, modifiers.
struct SimpleData {
    std::string_view entry;
    bool stackable;
    std::span<const VarDef> vars;
};

struct CharacterData {
    std::string_view entry;
    bool playable;
    int32_t starting_hp, starting_gold, max_energy, orb_slots;
    CardPoolId card_pool;
    RelicPoolId relic_pool;
    PotionPoolId potion_pool;
    std::span<const CardId> starting_deck;
    std::span<const RelicId> starting_relics;
    std::span<const PotionId> starting_potions;
};

struct ActData {
    std::string_view entry;
    int32_t index;
    bool is_default;
    std::span<const EncounterId> weak, regular, elite, boss, boss_discovery_order;
    std::span<const EventId> events, ancients;
    std::span<const MonsterId> monsters;
};

struct CardPoolData { std::string_view entry; std::span<const CardId> cards; };
struct RelicPoolData { std::string_view entry; std::span<const RelicId> relics; };
struct PotionPoolData { std::string_view entry; std::span<const PotionId> potions; };

// Per-model reflection info: which framework members the C# class overrides
// (the behaviour the C++ port must implement) and where its source lives.
struct ModelInfo {
    ModelKind kind;
    uint16_t index;           // dense id within the kind
    std::string_view id;      // "CARD.BASH"
    std::string_view type;    // "Cards.Bash"
    std::string_view source;  // relative to the decompiled src/Core
    MemberSet overrides;
};

// Numeric properties that change at an ascension level (beyond monster HP and
// encounter gold, which are folded into their tables).
struct AscensionOverride {
    ModelKind kind;
    uint16_t index;
    AscensionLevel level;
    std::string_view prop;
    int32_t value;
};

const CardData& data(CardId id);
const RelicData& data(RelicId id);
const PowerData& data(PowerId id);
const PotionData& data(PotionId id);
const MonsterData& data(MonsterId id);
const EncounterData& data(EncounterId id);
const EventData& data(EventId id);
const SimpleData& data(EnchantmentId id);
const SimpleData& data(AfflictionId id);
const SimpleData& data(OrbId id);
const SimpleData& data(ModifierId id);
const CharacterData& data(CharacterId id);
const ActData& data(ActId id);
const CardPoolData& data(CardPoolId id);
const RelicPoolData& data(RelicPoolId id);
const PotionPoolData& data(PotionPoolId id);

std::span<const CardData> all_cards();
std::span<const RelicData> all_relics();
std::span<const PowerData> all_powers();
std::span<const PotionData> all_potions();
std::span<const MonsterData> all_monsters();
std::span<const EncounterData> all_encounters();
std::span<const EventData> all_events();
std::span<const SimpleData> all_enchantments();
std::span<const SimpleData> all_afflictions();
std::span<const SimpleData> all_orbs();
std::span<const SimpleData> all_modifiers();
std::span<const CharacterData> all_characters();
std::span<const ActData> all_acts();
std::span<const CardPoolData> all_card_pools();
std::span<const RelicPoolData> all_relic_pools();
std::span<const PotionPoolData> all_potion_pools();
std::span<const ModelInfo> all_model_infos();
std::span<const AscensionOverride> ascension_overrides();

// ModelDb's static lists (tools/Sts2Exporter "modeldb"), in game order.
namespace modeldb {
@MODELDB@
}  // namespace modeldb

}  // namespace sts2
"""


def main():
    with open(SRC, encoding="utf-8") as f:
        data = json.load(f)
    g = Gen(data)
    os.makedirs(OUT, exist_ok=True)
    files = {
        "enums.h": g.enums_h(),
        "ids.h": g.ids_h(),
        "members.h": g.members_h(),
        "tables.h": g.tables_h(),
        "tables.cpp": g.tables_cpp(),
    }
    for name, text in files.items():
        with open(os.path.join(OUT, name), "w", encoding="utf-8") as f:
            f.write(text)
    counts = ", ".join("%s %d" % (k, len(v)) for k, v in g.by_kind.items())
    print("generated %s: %s; %d members" % (OUT, counts, len(g.members)))


if __name__ == "__main__":
    main()
