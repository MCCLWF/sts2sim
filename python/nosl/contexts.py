"""What a strategic decision offers, in words: its context and a label for every action.

Used twice, and it has to be the same both times: tools/nosl/exploration_effects.py compares the outcomes of
uniformly random exploratory actions within a context, and nosl.strategy_q's table rule looks a decision up in the
table made from that comparison. A context is one kind of decision offering one set of things (a rest site at an HP
level, a map step with its node types, an event, a nested choice named by its source and prompt); a label says what
an action is (Heal or Smith, the type of the node entered, the event option's index, the card picked).

Everything here reads the public observation tables only.
"""
from __future__ import annotations

import re
from pathlib import Path

G_DECISION, G_ASCENSION, G_ACT, G_HP, G_MAX_HP, G_PROMPT, G_SOURCE = 1, 3, 4, 7, 8, 32, 33
T_TYPE, T_CARD, T_MISC_A, T_N0 = 0, 1, 6, 8
ACTION_NAMES = ("StartRun", "MapMove", "PlayCard", "EndTurn", "TakeReward", "Proceed", "EventOption", "RestOption",
                "ShopBuy", "TreasurePick", "UsePotion", "DiscardPotion", "Answer", "Resume")
DECISIONS = ("map", "combat", "rewards", "event", "rest", "shop", "treasure", "choice", "ended", "boundary")
REST = ("Heal", "Smith", "Mend", "Lift", "Dig", "Cook", "Clone", "Hatch", "Kindle")
MAP_POINT = ("Unassigned", "Unknown", "Shop", "Treasure", "RestSite", "Monster", "Elite", "Boss", "Ancient")
REWARD = ("None", "Card", "Gold", "Potion", "Relic", "RemoveCard", "SpecialCard")
SHOP = ("CharacterCard", "ColorlessCard", "Relic", "Potion", "CardRemoval")
CHOICE_OPTION = ("Card", "Bundle", "Reward", "Alternative", "Proceed")
MISC_MAP, MISC_REWARD, MISC_SHOP, MISC_REST, MISC_CHOICE, MISC_EVENT = 1, 17, 33, 41, 57, 128
KNOWN_TEXT = ("CardSelectCmd", "CardReward", "rewards", "take a reward, or proceed", "FromSimpleGridForRewards", "index",
              "count", "FromDeckGeneric", "FromDeckForEnchantment", "FromDeckForUpgrade", "FromDeckForTransformation",
              "FromDeckForRemoval", "FromChooseACardScreen", "FromChooseABundleScreen", "bundle", "crystal_sphere",
              "CRYSTAL_SPHERE.minigame")
ROOT = Path(__file__).resolve().parents[2]


def bucket(text):
    """nosl/observation.cpp Bucket(): the misc id of a text key."""
    h = 2166136261
    for c in text.encode():
        h = ((h ^ c) * 16777619) & 0xFFFFFFFF
    return 256 + h % 1024


def enum_names(enum):
    text = (ROOT / "content/generated/ids.h").read_text()
    body = re.search(r"enum class %s[^{]*\{([^}]*)\}" % enum, text, re.S).group(1)
    return [x.split("=")[0].strip() for x in body.split(",") if x.strip()]


class Describer:
    def __init__(self, split_ascension=True):
        self.cards, self.events = enum_names("CardId"), enum_names("EventId")
        self.text_of = {bucket(t): t for t in KNOWN_TEXT}
        self.split_ascension = split_ascension

    def card_label(self, token):
        return self.cards[token[T_CARD]] + "+" * int(token[T_N0]) if 0 < token[T_CARD] < len(self.cards) else "?"

    def __call__(self, g, tokens, actions):
        """(context, labels) for one decision: `actions` holds its action rows only. A label is None for an action
        the comparison has no name for."""
        kind = DECISIONS[g[G_DECISION]]
        act = f"act{g[G_ACT] + 1}"
        if self.split_ascension:
            act = f"A{g[G_ASCENSION]} " + act
        if kind in ("rest", "map", "rewards", "shop", "treasure"):
            def simple(row):
                tok = tokens[row[1]] if row[1] >= 0 else None
                what = ACTION_NAMES[row[0]]
                if tok is None:
                    return "leave" if kind == "shop" and what == "Proceed" else what
                if kind == "rest" and what == "RestOption":
                    return REST[tok[T_MISC_A] - MISC_REST]
                if kind == "map" and what == "MapMove":
                    return MAP_POINT[tok[T_MISC_A] - MISC_MAP]
                if kind == "rewards" and what == "TakeReward":
                    return "take " + REWARD[tok[T_MISC_A] - MISC_REWARD]
                if kind == "shop" and what == "ShopBuy":
                    return "buy " + SHOP[tok[T_MISC_A] - MISC_SHOP]
                return what
            labels = [simple(row) for row in actions]
            where = kind
            if kind == "rest":
                hp = g[G_HP] / max(1, g[G_MAX_HP])
                where = f"rest {act} " + ("hp<50%" if hp < 0.5 else "hp 50-80%" if hp < 0.8 else "hp>=80%")
            elif kind == "map":
                where = "map " + act
            return f"{where} offering {'|'.join(sorted(set(labels)))}", labels
        if kind == "event":
            labels, context = [], "event ?"
            for row in actions:
                token = tokens[row[1]] if row[1] >= 0 else None
                if ACTION_NAMES[row[0]] == "EventOption" and token is not None and MISC_EVENT <= token[T_MISC_A] < MISC_EVENT + len(self.events):
                    context = "event " + self.events[token[T_MISC_A] - MISC_EVENT]
                    labels.append(f"option {int(token[T_N0])}")
                else:
                    labels.append(None)
            return context, labels
        if kind == "choice":
            source = self.text_of.get(int(g[G_SOURCE]), f"#{int(g[G_SOURCE])}")
            prompt = self.text_of.get(int(g[G_PROMPT]), f"#{int(g[G_PROMPT])}")
            labels = []
            for row in actions:
                token = tokens[row[1]] if row[1] >= 0 else None
                if ACTION_NAMES[row[0]] != "Answer" or token is None:
                    labels.append(ACTION_NAMES[row[0]] + f" {int(row[3])}")
                    continue
                option = CHOICE_OPTION[token[T_MISC_A] - MISC_CHOICE] if MISC_CHOICE <= token[T_MISC_A] < MISC_CHOICE + 5 else "?"
                labels.append(self.card_label(token) if option == "Card" else option)
            return f"choice {source}/{prompt}", labels
        return kind, [None] * len(actions)
