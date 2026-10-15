"""The readiness rule: strategic choices judged by what they do to the chance of getting past the act's boss.

tools/nosl/boss_readiness.py fits, per act, a logistic model of getting past the boss from the state a run reaches
it in. For a decision whose effect on that state can be read off the public observation, each action gets the
change it makes to the model's log-odds (dR):

  cards     a card reward (choice CardSelectCmd/CardReward): taking the card; skipping is 0
  upgrade   choosing the card to upgrade (choice FromDeckForUpgrade/index)
  remove    choosing the card a shop removes (choice FromDeckGeneric/index in a shop room; the same prompt in an
            event may mean something else and is left alone)
  rest      a rest site: Heal is the HP coefficient times the HP it restores (30% of the maximum, capped),
            Smith the best upgrade the deck offers
  shop      a shop: buying a card is taking it, a relic its coefficient, a potion the potion coefficient while a
            slot is free, the removal service the best removal the deck offers; leaving is 0. A purchase whose
            score is below `shop_min` is not made, the rule's own included; with `shop_add_only` the rule's own
            purchases stand and the judgment only adds one where the rule would leave

An action's score is its value in the value table (0 where the table has none) plus `lam` times dR; `lam` turns
log-odds into the table's unit, the outcome target. The rule replaces the action already chosen when the best
score is more than `delta` above that action's score. With `use_table` false the score is dR alone.

The chain rule (`chain`) judges by a chain of doors instead of the boss of the act alone: the second half of act 1,
its boss, the two halves of act 2, its boss, each with its own model. An action's dR is then the weighted sum of
what it does to the log-odds of every door still ahead; the weight of door j is the chance of passing every door up
to and including j, times (1 - p_j), times how much higher the outcome target of runs that passed j is than of
runs that did not: the derivative of the expected outcome target with respect to that door's log-odds. The weights
come with the model file, measured per ascension on the runs it was fitted on; `lam` only scales their sum (1 is
their own unit). A door in a later act is judged without knowing its boss (the mean over that act's bosses).

Everything is read from the public observation: the act, the floor, which boss the act ends in, the deck, HP.
"""
from __future__ import annotations

import json
from pathlib import Path

import numpy as np

G_DECISION, G_ASCENSION, G_ACT, G_FLOOR, G_HP, G_MAX_HP, G_BOSS, G_ROOM = 1, 3, 4, 6, 7, 8, 25, 26
G_POTION_COUNT, G_POTION_SLOTS = 23, 24
T_RELIC, T_N4 = 2, 12
ACTION_PROCEED, ACTION_SHOP = 5, 8
MISC_SHOP = 33      # + CharacterCard, ColorlessCard, Relic, Potion, CardRemoval
DECISION_REST, DECISION_SHOP = 4, 5
ROOM_SHOP = 5
T_TYPE, T_CARD, T_MISC_A, T_N0, T_N1 = 0, 1, 6, 8, 9
TOKEN_DECK = 5
ACTION_REST, ACTION_ANSWER = 7, 12
MISC_REST, MISC_CHOICE = 41, 57
HEAL, SMITH = 0, 1
CONTEXTS = {"choice CardSelectCmd/CardReward": "cards", "choice FromDeckForUpgrade/index": "upgrade",
            "choice FromDeckGeneric/index": "remove"}
SCOPES = ("cards", "upgrade", "remove", "rest", "shop")


def _model(t):
    bosses = [int(k) for k in t["boss"]]
    card_boss = {int(b): {int(k): float(v) for k, v in items.items()} for b, items in t["card_boss"].items()}
    mean = {}
    for items in card_boss.values():
        for card, value in items.items():
            mean[card] = mean.get(card, 0.0) + value / len(bosses)
    relic_boss = {int(b): {int(k): float(v) for k, v in items.items()} for b, items in t.get("relic_boss", {}).items()}
    relic_mean = {}
    for items in relic_boss.values():
        for relic, value in items.items():
            relic_mean[relic] = relic_mean.get(relic, 0.0) + value / len(bosses)
    return {"boss": set(bosses), "hp10": float(t["hp10"]), "act": int(t.get("act", -1)), "door": t.get("door"),
            "card": {int(k): (v["n"], v["u"]) for k, v in t["card"].items()}, "card_boss": card_boss, "card_boss_mean": mean,
            "potion": float(t.get("potion", 0.0)), "relic": {int(k): float(v) for k, v in t.get("relic", {}).items()},
            "relic_other": float(t.get("relic_other", 0.0)),
            "relic_boss": relic_boss, "relic_boss_mean": relic_mean}


class Readiness:
    def __init__(self, path, lam, scope=("cards", "upgrade"), acts=(0,), use_table=True, chain=False, shop_min=0.02,
                 shop_add_only=False):
        data = json.loads(Path(path).read_text())
        unknown = set(scope) - set(SCOPES)
        if unknown:
            raise ValueError(f"unknown readiness scope {sorted(unknown)}")
        self.lam, self.scope, self.use_table, self.chain = float(lam), frozenset(scope), bool(use_table), bool(chain)
        self.shop_min, self.shop_add_only = float(shop_min), bool(shop_add_only)
        self.acts = {int(act): _model(data["acts"][str(act)]) for act in acts}
        if chain:
            self.segments = [(name, _model(data["segments"][name])) for name in data["order"]]
            self.weights = {int(level): {name: (float(w["p"]), float(w["gain"])) for name, w in items.items()}
                            for level, items in data["weights"].items()}

    def ahead(self, g):
        """[(model, weight, boss or None)] for the doors a decision at this act and floor still has ahead of it."""
        act, floor, boss = int(g[G_ACT]), int(g[G_FLOOR]), int(g[G_BOSS])
        if not self.chain:
            model = self.acts.get(act)
            return [] if model is None or boss not in model["boss"] else [(model, 1.0, boss)]
        weights = self.weights.get(int(g[G_ASCENSION]))
        if weights is None:
            return []
        out, reach = [], 1.0
        for name, model in self.segments:
            which, _, passes = model["door"]
            if model["act"] < act or (model["act"] == act and which == "first" and floor >= passes - 1):
                continue
            p, gain = weights[name]
            reach *= p
            known = boss if model["act"] == act and boss in model["boss"] else None
            out.append((model, reach * (1.0 - p) * gain, known))
        return out

    def deltas(self, g, tokens, actions, context):
        """dR for every action row of one decision outside combat (nan: an action the rule does not judge), or None
        when the decision is not one it covers."""
        decision = int(g[G_DECISION])
        kind = "rest" if decision == DECISION_REST else "shop" if decision == DECISION_SHOP else CONTEXTS.get(context)
        if kind is None or kind not in self.scope or (kind == "remove" and int(g[G_ROOM]) != ROOM_SHOP):
            return None
        doors = self.ahead(g)
        if not doors:
            return None
        deck = {}  # card id -> [copies, upgraded copies]
        for token in tokens[tokens[:, T_TYPE] == TOKEN_DECK]:
            entry = deck.setdefault(int(token[T_CARD]), [0, 0])
            entry[0] += int(token[T_N1])
            entry[1] += int(token[T_N1]) if token[T_N0] > 0 else 0

        def with_boss(model, boss, card):
            return model["card_boss_mean"].get(card, 0.0) if boss is None else model["card_boss"].get(boss, {}).get(card, 0.0)

        def copy(card, upgraded):
            """Adding one copy."""
            n, u = deck.get(card, (0, 0))
            total = 0.0
            for model, weight, boss in doors:
                coefficients = model["card"].get(card)
                if coefficients is None:
                    continue
                value = coefficients[0][n] if n < len(coefficients[0]) else 0.0
                if upgraded:
                    value += coefficients[1][u] if u < len(coefficients[1]) else 0.0
                if n == 0:
                    value += with_boss(model, boss, card)
                total += weight * value
            return total

        def upgrade(card):
            _, u = deck.get(card, (0, 0))
            total = 0.0
            for model, weight, _ in doors:
                coefficients = model["card"].get(card)
                if coefficients is not None and u < len(coefficients[1]):
                    total += weight * coefficients[1][u]
            return total

        def removal(card, upgraded):
            n, u = deck.get(card, (0, 0))
            total = 0.0
            for model, weight, boss in doors:
                coefficients = model["card"].get(card)
                if coefficients is None or n == 0:
                    continue
                value = -coefficients[0][n - 1] if n <= len(coefficients[0]) else 0.0
                if upgraded and 1 <= u <= len(coefficients[1]):
                    value -= coefficients[1][u - 1]
                if n == 1:
                    value -= with_boss(model, boss, card)
                total += weight * value
            return total

        out = np.full(len(actions), np.nan)
        if kind == "shop":
            free_slot = g[G_POTION_COUNT] < g[G_POTION_SLOTS]
            for i, (action, token, _, _) in enumerate(actions):
                if action == ACTION_PROCEED and token < 0:
                    out[i] = 0.0                                   # leaving
                    continue
                if action != ACTION_SHOP or token < 0:
                    continue
                entry = tokens[token]
                what = int(entry[T_MISC_A]) - MISC_SHOP
                if what in (0, 1):
                    out[i] = copy(int(entry[T_CARD]), entry[T_N4] > 0)
                elif what == 2:
                    relic, total = int(entry[T_RELIC]), 0.0
                    for model, weight, boss in doors:
                        of_boss = model["relic_boss_mean"].get(relic, 0.0) if boss is None else model["relic_boss"].get(boss, {}).get(relic, 0.0)
                        total += weight * (model["relic"].get(relic, model["relic_other"]) + of_boss)
                    out[i] = total
                elif what == 3:
                    out[i] = sum(weight * model["potion"] for model, weight, _ in doors) if free_slot else 0.0
                elif what == 4:
                    options = [removal(card, False) for card, (n, u) in deck.items() if n > u]
                    options += [removal(card, True) for card, (n, u) in deck.items() if u > 0]
                    out[i] = max(options) if options else 0.0
            return out if not np.isnan(out).all() else None
        if kind == "rest":
            missing = 1.0 - g[G_HP] / max(1, g[G_MAX_HP])
            plain = [card for card, (n, u) in deck.items() if n > u]
            for i, (action, token, _, _) in enumerate(actions):
                if action != ACTION_REST or token < 0:
                    continue
                option = int(tokens[token, T_MISC_A]) - MISC_REST
                if option == HEAL:
                    out[i] = sum(weight * model["hp10"] for model, weight, _ in doors) * 10 * min(0.3, missing)
                elif option == SMITH:
                    out[i] = max([upgrade(card) for card in plain] + [0.0])
            return out if not np.isnan(out).all() else None
        for i, (action, token, _, _) in enumerate(actions):
            if action != ACTION_ANSWER or token < 0:
                continue
            option = tokens[token]
            if int(option[T_MISC_A]) - MISC_CHOICE != 0:   # not a card: skipping a reward
                out[i] = 0.0 if kind == "cards" else np.nan
                continue
            card, upgraded = int(option[T_CARD]), option[T_N0] > 0
            out[i] = copy(card, upgraded) if kind == "cards" else upgrade(card) if kind == "upgrade" else removal(card, upgraded)
        return out if not np.isnan(out).all() else None


def readiness_override(readiness, table, delta, describer, A, rows, choice, deviated=None, scores=None):
    """The readiness rule over the actions already chosen for the sampler slots `rows` (all outside combat); see the
    module text. `table` is the value table of nosl.strategy_q (may be empty), `scores[i]` the rule's own per-action
    scores of rows[i], used to choose among actions of equal score."""
    g = A["global"]
    for position, row in enumerate(rows):
        if int(g[row, G_DECISION]) == 1:
            continue
        count = int(A["counts"][row, 1])
        if count < 2:
            continue
        tokens, actions = A["tokens"][row], A["actions"][row, :count]
        context, labels = describer(g[row], tokens, actions)
        dr = readiness.deltas(g[row], tokens, actions, context)
        if dr is None:
            continue
        entries = table.get(context, {}) if readiness.use_table else {}
        lam = readiness.lam if readiness.use_table else 1.0
        values = np.array([np.nan if np.isnan(d) else lam * d + (entries.get(label, 0.0) if label is not None else 0.0)
                           for d, label in zip(dr, labels)])
        own = int(choice[row])
        if own >= count or np.isnan(values[own]):
            continue    # the rule chose something this judgment has no word on (another rest option): left alone
        if int(g[row, G_DECISION]) == DECISION_SHOP:
            buys = actions[:, 0] == ACTION_SHOP
            if readiness.shop_add_only and buys[own]:
                continue
            values = np.where(buys & (values < readiness.shop_min), -1.0, values)   # a purchase not worth making: below leaving
        top = np.nanmax(values)
        if top - values[own] <= delta:
            continue
        candidates = np.flatnonzero(values == top)
        if scores is not None and len(candidates) > 1:
            best = int(candidates[int(np.argmax(scores[position][candidates]))])
        else:
            best = int(candidates[0])
        if best != own:
            choice[row] = best
            if deviated is not None:
                deviated[row] = True
