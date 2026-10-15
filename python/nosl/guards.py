"""Guards: fixed corrections of plainly wasteful actions, applied last, over whatever the networks and rules chose.

They answer defects seen in real play (2026-10-11): a potion thrown away to make room for one just bought, a potion
bought with no free slot, rewards left on the screen, every potion drunk in the next ordinary fight, the shop's card
removal never used. Each guard reads the public observation only and names another legal action; nothing is simulated.

  nodiscard    never DiscardPotion while another action exists
  nobuyfull    no potion is bought while every potion slot is taken
  nopotionbuy  no potion is bought at all
  takerewards  the reward screen is not left while gold or a relic is on it, or a potion with a slot free for it
  hold=X       inside combat no potion is used in a fight that is neither an elite's nor a boss's while HP is at least
               the fraction X of the maximum and a potion slot is still free
  removefirst  at a shop the card removal is bought when it is offered and the deck still holds an unupgraded basic
               Strike or Defend; the card then removed is an unupgraded Strike, else an unupgraded Defend
  smith=X      at a rest site a card is upgraded when HP is at least the fraction X of the maximum and Smith is offered

A second set may replace the first in runs at or below an ascension level ("5:hold=0.5,removefirst"): what pays at
ascension 0 does not pay at ascension 10 (R10, R12).

A forbidden choice falls back to the highest-scored allowed action of the policy acting there (the combat network inside
combat, the base policy outside)."""
from __future__ import annotations

import collections

import numpy as np

from .contexts import bucket, enum_names

G_DECISION, G_ASCENSION, G_HP, G_MAX_HP, G_POTIONS, G_SLOTS, G_ROOM, G_PROMPT, G_SOURCE = 1, 3, 7, 8, 23, 24, 26, 32, 33
PROCEED, TAKE, REST_OPTION, BUY, USE_POTION, DISCARD_POTION, ANSWER = 5, 4, 7, 8, 10, 11, 12
REWARDS, REST, SHOP, CHOICE = 2, 4, 5, 7
MISC_SMITH = 41 + 1
T_DECK = 5
MISC_GOLD, MISC_POTION, MISC_RELIC = 17 + 2, 17 + 3, 17 + 4
SHOP_POTION, SHOP_REMOVAL = 33 + 3, 33 + 4
ELITE, BOSS, ROOM_SHOP = 2, 3, 5
NAMES = ("nodiscard", "nobuyfull", "nopotionbuy", "takerewards", "hold", "removefirst", "smith")


def parse(text):
    """'nodiscard,hold=0.5' -> {'nodiscard': 1.0, 'hold': 0.5}"""
    out = {}
    for item in text.split(","):
        if item:
            name, _, value = item.partition("=")
            if name not in NAMES:
                raise ValueError(f"unknown guard {name}")
            out[name] = float(value) if value else 1.0
    return out


def parse_low(text):
    """'5:hold=0.5,removefirst' -> (5, {...}); '' -> (-1, {})"""
    if not text:
        return -1, {}
    level, _, items = text.partition(":")
    return int(level), parse(items)


class Guards:
    def __init__(self, guards, low=None, low_max=-1):
        self.default, self.low, self.low_max = dict(guards or {}), dict(low or {}), low_max
        self.fired = collections.Counter()
        cards = enum_names("CardId")
        self.strike, self.defend = cards.index("STRIKE_SILENT"), cards.index("DEFEND_SILENT")
        self.removal = bucket("FromDeckGeneric")     # the shop's removal asks with the generic deck prompt

    def __call__(self, A, choice, scores_of):
        """`choice`: numpy array of the chosen action row per sampler slot, changed in place. `scores_of(slot)` gives
        the acting policy's scores of that slot's action rows. Returns the slots whose choice changed."""
        g, counts = A["global"], A["counts"]
        slots = np.flatnonzero(counts[:, 1] > 1)
        kinds = A["actions"][slots, choice[slots], 0]
        watch = np.isin(kinds, (PROCEED, BUY, USE_POTION, DISCARD_POTION, ANSWER)) | np.isin(g[slots, G_DECISION], (SHOP, REST))
        changed = []
        for slot in slots[watch]:
            actions = A["actions"][slot, :counts[slot, 1]]
            tokens = A["tokens"][slot]
            new = self.one(g[slot], tokens, actions, int(choice[slot]), lambda: scores_of(slot))
            if new is not None and new != choice[slot]:
                choice[slot] = new
                changed.append(slot)
        return changed

    def one(self, g, tokens, actions, chosen, scores):
        on = self.low if g[G_ASCENSION] <= self.low_max else self.default
        kind, decision = actions[chosen, 0], g[G_DECISION]
        misc = lambda row: tokens[row[1], 6] if row[1] >= 0 else -1
        full = g[G_POTIONS] >= g[G_SLOTS]
        if "smith" in on and decision == REST and g[G_HP] >= on["smith"] * max(1, g[G_MAX_HP]) \
                and not (kind == REST_OPTION and misc(actions[chosen]) == MISC_SMITH):
            for i, row in enumerate(actions):
                if row[0] == REST_OPTION and misc(row) == MISC_SMITH and tokens[row[1], 9]:
                    self.fired["smith"] += 1
                    return i
        if "removefirst" in on:
            if decision == SHOP and kind != BUY or decision == SHOP and misc(actions[chosen]) != SHOP_REMOVAL:
                offered = [i for i, row in enumerate(actions) if row[0] == BUY and misc(row) == SHOP_REMOVAL]
                deck = tokens[tokens[:, 0] == T_DECK]
                if offered and ((np.isin(deck[:, 1], (self.strike, self.defend))) & (deck[:, 8] == 0)).any():
                    self.fired["removefirst"] += 1
                    return offered[0]
            if decision == CHOICE and kind == ANSWER and g[G_ROOM] == ROOM_SHOP and g[G_SOURCE] == self.removal:
                for card in (self.strike, self.defend):
                    rows = [i for i, row in enumerate(actions) if row[0] == ANSWER and row[1] >= 0
                            and tokens[row[1], 1] == card and tokens[row[1], 8] == 0]
                    if rows:
                        if tokens[actions[chosen, 1], 1] != card or tokens[actions[chosen, 1], 8] != 0:
                            self.fired["removefirst target"] += 1
                        return rows[0] if actions[chosen, 1] < 0 or tokens[actions[chosen, 1], 1] != card or tokens[actions[chosen, 1], 8] != 0 else chosen
        if "takerewards" in on and decision == REWARDS and kind == PROCEED:
            for wanted in (MISC_GOLD, MISC_RELIC) + (() if full else (MISC_POTION,)):
                rows = [i for i, row in enumerate(actions) if row[0] == TAKE and misc(row) == wanted]
                if rows:
                    self.fired["takerewards"] += 1
                    return rows[0]
        banned = None
        if "nodiscard" in on and kind == DISCARD_POTION:
            banned, name = actions[:, 0] == DISCARD_POTION, "nodiscard"
        elif kind == BUY and misc(actions[chosen]) == SHOP_POTION and ("nopotionbuy" in on or "nobuyfull" in on and full):
            banned = np.array([row[0] == BUY and misc(row) == SHOP_POTION for row in actions])
            name = "nopotionbuy" if "nopotionbuy" in on else "nobuyfull"
            if "nodiscard" in on:
                banned |= actions[:, 0] == DISCARD_POTION
        elif "hold" in on and kind == USE_POTION and g[G_ROOM] not in (ELITE, BOSS) and not full \
                and g[G_HP] >= on["hold"] * max(1, g[G_MAX_HP]):
            banned, name = actions[:, 0] == USE_POTION, "hold"
        if banned is None or banned.all():
            return None
        self.fired[name] += 1
        s = np.asarray(scores(), dtype=np.float64)[:len(actions)].copy()
        s[banned] = -np.inf
        return int(s.argmax())
