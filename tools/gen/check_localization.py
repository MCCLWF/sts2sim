#!/usr/bin/env python3
"""Cross-check data/content.json against the game's English localization.

For cards, relics, powers, potions, enchantments and orbs:
  * every exported (non-mock) model has a title in the localization table;
  * every {Placeholder} in its description names an exported DynamicVar
    (or one of the formatter globals the game injects at render time).

Usage: tools/gen/check_localization.py [content.json] [localization/eng]
"""
import json
import os
import re
import sys
from collections import Counter

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CONTENT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "data", "content.json")
LOC = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "..", "Slay the Spire 2", "localization", "eng")

TABLES = {"CARD": "cards.json", "RELIC": "relics.json", "POWER": "powers.json", "POTION": "potions.json",
          "ENCHANTMENT": "enchantments.json", "ORB": "orbs.json"}

PLACEHOLDER = re.compile(r"\{([A-Za-z_][A-Za-z0-9_]*)")


def main():
    content = json.load(open(CONTENT, encoding="utf-8"))
    models = [m for m in content["models"]
              if not m.get("mock") and not m["id"].split(".", 1)[1].startswith("MOCK_")]
    missing_title = []
    unknown = Counter()
    examples = {}
    checked = 0
    for m in models:
        cat, entry = m["id"].split(".", 1)
        if cat not in TABLES:
            continue
        table = json.load(open(os.path.join(LOC, TABLES[cat]), encoding="utf-8"))
        if entry + ".title" not in table:
            missing_title.append(m["id"])
        var_names = {v["name"] for v in m.get("vars", [])}
        for key in (entry + ".description", entry + ".smartDescription", entry + ".upgradedDescription"):
            text = table.get(key)
            if not text:
                continue
            for name in PLACEHOLDER.findall(text):
                checked += 1
                if name not in var_names:
                    unknown[name] += 1
                    examples.setdefault(name, m["id"])
    print("placeholders checked: %d" % checked)
    print("models without a localized title: %d %s" % (len(missing_title), missing_title[:10]))
    print("placeholders that are not exported vars (formatter globals or render-time vars):")
    for name, n in unknown.most_common():
        print("  %-28s %4d  e.g. %s" % (name, n, examples[name]))


if __name__ == "__main__":
    main()
