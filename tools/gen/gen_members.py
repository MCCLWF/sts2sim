#!/usr/bin/env python3
"""Generate the overridable-member tables game/Models/<Base>.def.

Source of truth: the "virtuals" section of data/content.json (every virtual
member each framework base class introduces, with its C# signature). Default
bodies are taken from the decompiled C# source of the base class and
translated when they are simple (return a literal, return a parameter, do
nothing); anything else is marked SIM_DEFAULT_IMPL and implemented by hand as
Base::Name_Default in game/Models/<Base>.cpp.

Members whose signature mentions a C# type the port does not have yet are left
out and listed at the end of the file; they become available as TYPES grows.

Usage: tools/gen/gen_members.py [content.json] [src/Core]
"""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CONTENT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "data", "content.json")
SRC = sys.argv[2] if len(sys.argv) > 2 else os.environ.get(
    "STS2_SRC", os.path.join(ROOT, "..", "Slay the Spire 2", "src", "Core"))
OUT = os.path.join(ROOT, "game", "Models")

# Framework base classes that get a member table, and their source files.
BASES = {
    "AbstractModel": "Models/AbstractModel.cs",
    "CardModel": "Models/CardModel.cs",
    "PowerModel": "Models/PowerModel.cs",
    "MonsterModel": "Models/MonsterModel.cs",
    "RelicModel": "Models/RelicModel.cs",
    "PotionModel": "Models/PotionModel.cs",
    "EncounterModel": "Models/EncounterModel.cs",
    "EventModel": "Models/EventModel.cs",
    "AfflictionModel": "Models/AfflictionModel.cs",
}

# C# type -> C++ parameter type. Types not listed are "not ported yet".
TYPES = {
    "Void": "void", "Task": "void", "Boolean": "bool", "Int32": "int32_t", "Decimal": "Dec", "Single": "float",
    "Decimal&": "Dec&", "Int32&": "int32_t&",
    "Creature": "Creature", "Player": "Player", "CardModel": "CardModel", "PowerModel": "PowerModel",
    "MonsterModel": "MonsterModel", "RelicModel": "RelicModel", "PotionModel": "PotionModel", "EventModel": "EventModel",
    "AbstractModel": "AbstractModel", "CardModel&": "CardModel&",
    "ICombatState": "CombatState", "IRunState": "RunState",
    "PlayerChoiceContext": "PlayerChoiceContext&",
    "AttackCommand": "AttackCommand&",
    "CardPlay": "const CardPlay*",
    "DamageResult": "const DamageResult&",
    "ValueProp": "ValueProp", "CombatSide": "CombatSide", "PileType": "PileType", "AutoPlayType": "AutoPlayType",
    "CardRarity": "CardRarity", "RoomType": "RoomType", "TargetType": "TargetType", "CardType": "CardType",
    "IEnumerable<Creature>": "const CreatureList&", "IReadOnlyList<Creature>": "const CreatureList&",
    "IReadOnlyCollection<CardModel>": "const CardList&", "IEnumerable<CardModel>": "const CardList&",
    "List<CardModel>": "CardList&",
    "PowerType": "PowerType", "PowerStackType": "PowerStackType", "PowerInstanceType": "PowerInstanceType",
    "CardMultiplayerConstraint": "CardMultiplayerConstraint", "OrbEvokeType": "OrbEvokeType",
    "PotionUsage": "PotionUsage", "RelicRarity": "RelicRarity", "PotionRarity": "PotionRarity",
    "IntentType": "IntentType", "CardKeyword": "CardKeyword", "CardTag": "CardTag",
    "MonsterMoveStateMachine": "MoveMachineDef",
    "CombatRoom": "CombatRoom", "AbstractRoom": "AbstractRoom",
    "CardLocation": "CardLocation", "ResourceInfo": "const ResourceInfo&", "AfflictionModel": "AfflictionModel",
    "EnchantmentModel": "EnchantmentModel", "OrbModel": "OrbModel",
    "MerchantEntry": "Merchant::EntryRef",
    "ModifyDamageHookType": "ModifyDamageHookType", "HpLossHookPhase": "HpLossHookPhase",
    "CardPreviewMode": "CardPreviewMode", "CardPilePosition": "CardPilePosition",
    "ISet<CardKeyword>": "CardKeywordSet&",
    "IReadOnlySet<RoomType>": "RoomTypeSet",
    "ActMap": "ActMap",
    "CombatState": "CombatState",
    "Reward": "Reward", "CardReward": "Reward", "RewardsSet": "RewardsSet", "List<Reward>": "RewardList",
    "CardCreationOptions": "const CardCreationOptionsState&", "List<CardCreationResult>": "CardList&",
    "IReadOnlyList<ValueTuple<MonsterModel,String>>": "MonsterSpawnList",
    "IReadOnlyList<EventOption>": "EventOptionList", "IEnumerable<EventOption>": "const EventOptionList&",
    "LocString": "LocKey",
}
# Qualify every sts2 type: members named like their type (TargetType
# TargetType()) would otherwise change the meaning of the name in class scope.
_PRIMITIVE = {"void", "bool", "int32_t", "float", "int32_t&"}


def _qualify(t):
    if t is None or t in _PRIMITIVE:
        return t
    const = t.startswith("const ")
    core = t[6:] if const else t
    return ("const " if const else "") + "sts2::" + core


TYPES = {k: _qualify(v) for k, v in TYPES.items()}
# C# type -> C++ return type (by value).
RETURNS = dict(TYPES)
RETURNS.update({"EncounterModel": "sts2::EncounterId", "CardCreationOptions": "sts2::CardCreationOptionsState", "Task": "void", "Decimal&": None, "Int32&": None, "CardModel&": None, "PlayerChoiceContext": None,
                "AttackCommand": None, "CardPlay": None, "DamageResult": None,
                "IEnumerable<Creature>": None, "IReadOnlyList<Creature>": None,
                "IReadOnlyCollection<CardModel>": None, "IEnumerable<CardModel>": None, "List<CardModel>": None})

# Members that are infrastructure or cosmetic, not gameplay.
# AfterMapGenerated receives a mutable reference-type map; keep its callback
# behind the run hook guard until the member mapping can preserve reference semantics.
SKIP_NAMES = {"CompareTo", "get_IsMock", "DeepCloneFields", "AfterCloned", "get_PreviewOutsideOfCombat",
              "get_ShouldReceiveCombatHooks", "AfterTargetingBlockedVfx",
              "AfterMapGenerated",
              # Table-driven (content/generated/tables.h), not dispatched:
              "get_CanonicalVars", "get_CanonicalKeywords", "get_CanonicalTags", "get_Tags", "get_Pool",
              "get_MinInitialHp", "get_MaxInitialHp", "get_MultiplayerConstraint",
              # PowerModel's abstract Type / StackType: constants per power (PowerData).
              "PowerModel.get_Type", "PowerModel.get_StackType", "PowerModel.get_InstanceType",
              "PowerModel.get_AllowNegative", "PowerModel.get_ShouldScaleInMultiplayer",
              "PowerModel.get_DisplayAmount", "PowerModel.get_IsVisibleInternal",
              # Constants per class (RelicData / PotionData); FoulPotion's
              # TargetType is the one computed override (PotionModel.cpp).
              "RelicModel.get_Rarity", "PotionModel.get_Rarity", "PotionModel.get_Usage", "PotionModel.get_TargetType",
              # EncounterModel: table-driven (EncounterData) or presentation.
              "EncounterModel.get_RoomType", "EncounterModel.get_IsWeak", "EncounterModel.get_ShouldGiveRewards",
              "EncounterModel.get_MinGoldReward", "EncounterModel.get_MaxGoldReward",
              "EncounterModel.get_AllPossibleMonsters", "EncounterModel.get_Slots",
              "EncounterModel.get_FullyCenterPlayers", "EncounterModel.get_HasCustomBackground",
              "EncounterModel.get_AmbientSfx", "EncounterModel.get_ExtraAssetPaths",
              # EventModel: constants per event (EventData) or presentation.
              "EventModel.get_IsDeterministic", "EventModel.get_IsShared", "EventModel.get_LayoutType",
              "EventModel.get_GameInfoOptions",
              # Ported by hand in the base class:
              "InitInternalData"}
# Defaults written by hand even though the C# base body is trivial: an
# intermediate base class (AncientEventModel) overrides them for its subclasses.
FORCE_CUSTOM = {"EventModel.BeforeEventStarted"}

SKIP_PARTS = ("Path", "Portrait", "Title", "Description", "HoverTip", "Vfx", "Sfx", "Anim", "Icon", "Color",
              "LocTable", "LocKey", "Banner", "Overlay", "Visual", "Glow", "Frame", "Texture", "Material", "Scene", "Bestiary",
              "Flavor", "Tint", "Shader", "Spine", "Skin", "Sound", "Music", "Bgm", "Speech", "Dialogue",
              "Camera")

CPP_KEYWORDS = {"default", "new", "delete", "class", "template", "operator", "register"}


def cpp_name(name):
    return name[4:] if name.startswith("get_") else name


def param_name(p):
    n = p.rsplit(" ", 1)[1]
    return n + "_" if n in CPP_KEYWORDS else n


def csharp_body(src, name, nparams):
    """The body of `virtual ... name(...)` with `nparams` parameters in `src`."""
    prop = name.startswith("get_")
    ident = cpp_name(name)
    if prop:
        m = re.search(r"virtual [^\n=;{]*?\b%s\s*=>\s*(.*?);\n" % re.escape(ident), src)
        if m:
            return "=>" + m.group(1).strip()
        m = re.search(r"virtual [^\n=;{]*?\b%s\s*\n\t\{\n\t\tget\n\t\t\{\n(.*?)\n\t\t\}" % re.escape(ident), src, re.S)
        return m.group(1) if m else None
    for m in re.finditer(r"virtual [^\n;{]*?\b%s\((.*?)\)\s*(=>\s*(.*?);\n|\n\t\{\n(.*?)\t\})" % re.escape(ident),
                         src, re.S):
        params = [p for p in m.group(1).split(",") if p.strip()]
        if len(params) != nparams:
            continue
        return ("=>" + m.group(3).strip()) if m.group(3) is not None else m.group(4)
    return None


LITERAL = re.compile(r"^(-?\d+)m$")


def translate_return(expr, ret, params):
    expr = expr.strip()
    names = [param_name(p) for p in params]
    if expr in names:
        return "return %s;" % expr
    if expr in ("true", "false"):
        return "return %s;" % expr
    if expr == "null":
        return "return {};"
    if expr == "decimal.MaxValue":
        return "return Dec::max_value();"
    m = LITERAL.match(expr)
    if m:
        return "return Dec(%s);" % m.group(1)
    if re.fullmatch(r"-?\d+", expr):
        return "return %s;" % expr
    m = re.fullmatch(r"(-?\d+(?:\.\d+)?)f", expr)
    if m and ret == "float":
        return "return %sf;" % (m.group(1) if "." in m.group(1) else m.group(1) + ".0")
    return None


def translate_default(body, ret, params):
    """C++ statements for a simple C# default body, or None."""
    if body is None:
        return None
    if body.startswith("=>"):
        expr = body[2:].strip()
        if expr in ("Task.CompletedTask",):
            return ""
        return translate_return(expr, ret, params)
    lines = [l.strip() for l in body.strip().splitlines() if l.strip()]
    out = []
    for l in lines:
        if l == "return Task.CompletedTask;":
            continue
        m = re.fullmatch(r"return (.*);", l)
        if m:
            t = translate_return(m.group(1), ret, params)
            if t is None:
                return None
            out.append(t)
            continue
        m = re.fullmatch(r"(\w+) = (\w+);", l)  # out-parameter copy: modifiedCost = originalCost;
        if m and m.group(1) in [param_name(p) for p in params]:
            out.append("%s = {};" % m.group(1) if m.group(2) == "null" else l)
            continue
        return None
    return " ".join(out)


def main():
    data = json.load(open(CONTENT, encoding="utf-8"))
    virtuals = data["virtuals"]
    for base, rel in BASES.items():
        src = open(os.path.join(SRC, rel), encoding="utf-8").read()
        lines, missing, custom = [], [], []
        for m in virtuals.get(base, []):
            name = m["name"]
            if name in SKIP_NAMES or "%s.%s" % (base, name) in SKIP_NAMES or any(part in name for part in SKIP_PARTS):
                continue
            ret = RETURNS.get(m["returns"]) if not m["returns"].startswith("Task<") else \
                RETURNS.get(m["returns"][5:-1])
            ptypes = [TYPES.get(p.rsplit(" ", 1)[0]) for p in m["params"]]
            if ret is None or any(t is None for t in ptypes):
                missing.append("%s %s(%s)" % (m["returns"], name, ", ".join(m["params"])))
                continue
            ident = cpp_name(name)
            params = "(%s)" % ", ".join("%s %s" % (t, param_name(p)) for t, p in zip(ptypes, m["params"]))
            args = "(%s)" % ", ".join(param_name(p) for p in m["params"])
            macro = "SIM_MEMBER"
            if m["abstract"]:
                default = "SIM_ABSTRACT"
            elif "%s.%s" % (base, name) in FORCE_CUSTOM:
                macro, default = "SIM_MEMBER_CUSTOM", "SIM_DEFAULT_IMPL"
                custom.append(ident)
            else:
                body = translate_default(csharp_body(src, name, len(m["params"])), ret, m["params"])
                if body is None:
                    macro, default = "SIM_MEMBER_CUSTOM", "SIM_DEFAULT_IMPL"
                    custom.append(ident)
                else:
                    default = "{ %s }" % body if body else "{}"
            lines.append("%s(%s, %s, %s, %s, %s, %s)" % (macro, base, ret, ident, params, args, default))
        out = ["// GENERATED by tools/gen/gen_members.py from data/content.json and %s. Do not edit." % rel,
               "// SIM_MEMBER(Base, Ret, Name, Params, Args, Default): Default is the C++ body of the C#",
               "// base implementation, or SIM_ABSTRACT (abstract in C#; every concrete class defines it).",
               "// SIM_MEMBER_CUSTOM: the default is hand-written as %s::Name_Default." % base,
               ""]
        out += lines
        if missing:
            out += ["", "// Not generated yet (a C# type in the signature has no C++ port):"]
            out += ["//   " + s for s in missing]
        path = os.path.join(OUT, base + ".def")
        with open(path, "w", encoding="utf-8") as f:
            f.write("\n".join(out) + "\n")
        print("%s: %d members, %d hand-written defaults, %d waiting for types" % (base, len(lines), len(custom), len(missing)))
        if custom:
            print("   hand-written: " + ", ".join(custom))


if __name__ == "__main__":
    main()
