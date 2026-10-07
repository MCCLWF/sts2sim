"""The actor-critic over observation schema nosl.obs v1.

One set of weights for every decision kind: tokens and the run scalars go
through a small transformer, each legal action is scored from the tokens it
points at, and the value is read from the run token. Nothing is positional,
so token order carries no information.

`features` selects what the model derives from the same observation:
  1  ids and the observation's own numbers;
  2  also what is printed on each card (a constant table by card id and
     upgrade level, tools/nosl/nosl_card_table) and the attack about to land
     this turn (the sum of the shown enemy intents, and how it compares with
     the player's block).
  3  also, on the map, the fewest elites on any path from each travelable
     node to the end of the act (a shortest-path pass over the map nodes on
     screen), and how that compares with the best node on offer.
  4  also, outside combat, the public deck's printed-number totals and means,
     keyword counts, card count and upgraded-card count, weighted by the
     multiplicities of the deck's bag tokens. No combat pile is treated as
     the permanent deck, and padding and offered cards never enter the sum.
All of it is public: the table is static game rules and the rest is arithmetic
on the observation. The observation schema is the same for every feature level.

The independent `power_context` switch pools visible power id embeddings,
weighted by their shown amounts, onto their player/enemy owner. It reads the
public owner ordinals only; no engine id or hidden state enters the pool.

The independent `action_interaction` switch adds a zero-initialized projection
of the elementwise source/target product to the action vector. It uses only
the same public encoded entities and is zero for an action without a target.

`critic="separate"` gives the value its own copy of the whole encoder, so the
value loss shapes its own features and the policy loss its own. Only training
uses it; the exported policy is the same either way (its value output then
comes from an untrained head and means nothing).
"""
from __future__ import annotations

import json

import torch
from torch import nn

from . import native

# Content table sizes (content/generated/ids.h) and the misc vocabulary (nosl/observation.h).
VOCAB = {"kind": 18, "card": 597, "relic": 300, "potion": 66, "monster": 121, "power": 266, "misc": 1280}
DECISION_KINDS, ACTION_KINDS, ENCOUNTERS = 10, 14, 91
ID_COLUMNS = ("kind", "card", "relic", "potion", "monster", "power", "misc", "misc")  # token columns 0..7
G_DECISION, G_BOSS, G_PROMPT, G_SOURCE, G_EVENT = 1, 25, 32, 33, 37
G_ID_LIKE = (0, G_DECISION, 2, G_BOSS, 26, G_PROMPT, G_SOURCE, G_EVENT)
MISC_EVENT = 128
G_BLOCK = 9
TOKEN_ENEMY, TOKEN_SHOP = 6, 14  # nosl/observation.h TokenType
TOKEN_POWER = 7
TOKEN_MAP_NEXT, TOKEN_MAP_NODE = 10, 11
TOKEN_DECK = 5
MAP_ROWS, MAP_COLS = 24, 8       # room for the acts' maps (7 columns, rows 0..16 in v0.111.0)
MAP_ELITE = 1 + 6                # misc::kMapPoint + MapPointType::Elite
N0 = len(ID_COLUMNS)             # first number column of a token
CARD_LEVELS, CARD_NUMBERS, CARD_FLAGS, CARD_POWERS = 2, 14, 42, 2
DECK_NUMBERS = 2 * CARD_NUMBERS + CARD_FLAGS + 2


def load_card_table(path):
    """tools/nosl/nosl_card_table output as (numbers, flags, powers) tensors indexed [card id, level]."""
    table = json.loads(open(path, encoding="utf-8").read())
    if table["version"] != 1 or table["levels"] != CARD_LEVELS or len(table["numbers"]) != CARD_NUMBERS \
            or len(table["flags"]) != CARD_FLAGS:
        raise ValueError("card table of another layout")
    numbers = torch.zeros(VOCAB["card"], CARD_LEVELS, CARD_NUMBERS)
    flags = torch.zeros(VOCAB["card"], CARD_LEVELS, CARD_FLAGS)
    powers = torch.zeros(VOCAB["card"], CARD_LEVELS, CARD_POWERS)
    for card in table["cards"]:
        for level, row in enumerate(card["levels"]):
            numbers[card["id"], level] = torch.tensor(row["numbers"], dtype=torch.float32)
            flags[card["id"], level] = torch.tensor(row["flags"], dtype=torch.float32)
            powers[card["id"], level] = torch.tensor(row["powers"], dtype=torch.float32)
    return numbers, flags, powers


def numeric(x: torch.Tensor) -> torch.Tensor:
    """Game integers as bounded floats: a log scale and a clipped linear scale."""
    x = x.float()
    return torch.cat([torch.sign(x) * torch.log1p(x.abs()) / 3.0, x.clamp(-20, 20) / 10.0], dim=-1)


class ActorCritic(nn.Module):
    def __init__(self, width: int = 128, layers: int = 3, heads: int = 4, feedforward: int = 256,
                 features: int = 1, card_table: str = "", critic: str = "shared", power_context: bool = False,
                 action_interaction: bool = False):
        """`card_table` fills the card buffers of a fresh features-2 model; a loaded checkpoint brings its own."""
        super().__init__()
        self.config = {"width": width, "layers": layers, "heads": heads, "feedforward": feedforward,
                       "schema": native.SCHEMA_VERSION, "features": features}
        if critic != "shared":
            self.config["critic"] = critic
        if power_context:
            self.config["power_context"] = True
        if action_interaction:
            self.config["action_interaction"] = True
        self.value_net = ActorCritic(width, layers, heads, feedforward, features, card_table,
                                    power_context=power_context) if critic == "separate" else None
        self.features = features
        self.power_context = nn.Linear(2 * width, width) if power_context else None
        if self.power_context is not None:
            nn.init.zeros_(self.power_context.weight)
            nn.init.zeros_(self.power_context.bias)
        if features >= 2:
            numbers, flags, powers = load_card_table(card_table) if card_table else (
                torch.zeros(VOCAB["card"], CARD_LEVELS, CARD_NUMBERS), torch.zeros(VOCAB["card"], CARD_LEVELS, CARD_FLAGS),
                torch.zeros(VOCAB["card"], CARD_LEVELS, CARD_POWERS))
            # Float buffers, so the exported weight file stays one tensor type.
            self.register_buffer("card_numbers", numbers)
            self.register_buffer("card_flags", flags)
            self.register_buffer("card_powers", powers)
            self.card_features = nn.Linear(2 * CARD_NUMBERS + CARD_FLAGS, width)
            self.global_extra = nn.Linear(2 * 3, width)
        if features >= 3:
            self.route = nn.Linear(2 * 2, width)
        if features >= 4:
            self.deck_features = nn.Linear(2 * DECK_NUMBERS, width)
        self.embed = nn.ModuleDict({k: nn.Embedding(n, width, padding_idx=0) for k, n in VOCAB.items()})
        self.token_numbers = nn.Linear(2 * (native.TOKEN_WIDTH - len(ID_COLUMNS)), width)
        self.token_norm = nn.LayerNorm(width)
        self.decision = nn.Embedding(DECISION_KINDS, width)
        self.boss = nn.Embedding(ENCOUNTERS, width, padding_idx=0)
        self.global_numbers = nn.Linear(2 * native.GLOBAL_WIDTH, width)
        self.global_norm = nn.LayerNorm(width)
        layer = nn.TransformerEncoderLayer(width, heads, feedforward, dropout=0.0, batch_first=True, norm_first=True)
        self.encoder = nn.TransformerEncoder(layer, layers, enable_nested_tensor=False)
        self.final_norm = nn.LayerNorm(width)
        self.action_kind = nn.Embedding(ACTION_KINDS, width)
        self.action_target = nn.Linear(width, width)
        self.action_number = nn.Linear(2, width)
        self.action_interaction = nn.Linear(width, width, bias=False) if action_interaction else None
        if self.action_interaction is not None:
            nn.init.zeros_(self.action_interaction.weight)
        self.policy = nn.Sequential(nn.Linear(2 * width, width), nn.ReLU(), nn.Linear(width, 1))
        self.value = nn.Sequential(nn.Linear(width, width), nn.ReLU(), nn.Linear(width, 1))
        nn.init.zeros_(self.policy[-1].weight)
        nn.init.zeros_(self.policy[-1].bias)

    def forward(self, global_, tokens, actions, counts):
        """global_ [B,G], tokens [B,T,W], actions [B,A,4], counts [B,2] (integer tensors).
        Returns action logits [B,A] (-inf past each row's action count) and values [B]."""
        global_, tokens, actions, counts = global_.long(), tokens.long(), actions.long(), counts.long()
        run, h_tokens = self.encode(global_, tokens, counts)
        features = self.action_features(run, h_tokens, actions, self.action_interaction is not None)
        logits = self.score_actions(features, counts)
        if self.value_net is not None:
            critic = self.value_net
            return logits, critic.value(critic.encode(global_, tokens, counts)[0]).squeeze(-1)
        return logits, self.value(run).squeeze(-1)

    def action_features(self, run, h_tokens, actions, include_interaction=False):
        """Public encoded action features; offline frozen-encoder training may cache them."""
        def gather(index):
            picked = torch.gather(h_tokens, 1, index.clamp(min=0).unsqueeze(-1).expand(-1, -1, h_tokens.shape[-1]))
            return picked * (index >= 0).unsqueeze(-1)

        source, target = gather(actions[..., 1]), gather(actions[..., 2])
        a = self.action_kind(actions[..., 0].clamp(0, ACTION_KINDS - 1))
        a = a + source + self.action_target(target)
        a = a + self.action_number(numeric(actions[..., 3:4]))
        parts = [run.unsqueeze(1).expand(-1, a.shape[1], -1), a]
        if include_interaction:
            parts.append(source * target)
        return torch.cat(parts, dim=-1)

    def score_actions(self, features, counts):
        """Use the same scoring path for cached learning and normal public inference."""
        width = self.config["width"]
        # The cached three-part tensor gives this slice a wider row stride.
        # Use the same contiguous Linear input as ordinary two-part inference;
        # CUDA's strided path can otherwise round its bias addition differently.
        pair = features[..., :2 * width].contiguous()
        if self.action_interaction is not None:
            pair = torch.cat([pair[..., :width], pair[..., width:] +
                              self.action_interaction(features[..., 2 * width:])], dim=-1)
        logits = self.policy(pair).squeeze(-1)
        slots = torch.arange(features.shape[1], device=features.device).unsqueeze(0)
        logits = logits.masked_fill(slots >= counts[:, 1:2], float("-inf"))
        return logits

    @staticmethod
    def elites_ahead(tokens):
        """[B,T]: for a token at map (col, row), the fewest elites on any path from that node to the end of the
        act, the node itself included. Read from the map-node tokens (type, col, row, columns of the children in
        the next row); meaningful for map tokens only."""
        batch = tokens.shape[0]
        node = tokens[..., 0] == TOKEN_MAP_NODE
        col = tokens[..., N0].clamp(0, MAP_COLS - 1)
        row = tokens[..., N0 + 1].clamp(0, MAP_ROWS - 1)
        cell = row * MAP_COLS + col
        spare = MAP_ROWS * MAP_COLS  # where tokens that are not map nodes are written
        at = torch.where(node, cell, torch.full_like(cell, spare))

        def grid(values):
            flat = torch.zeros(batch, spare + 1, dtype=torch.long, device=tokens.device)
            flat.scatter_(1, at, values.long())
            return flat[:, :spare].view(batch, MAP_ROWS, MAP_COLS)

        elite = grid(node & (tokens[..., 6] == MAP_ELITE))
        exists = grid(node)
        children = grid(tokens[..., N0 + 4] * node)
        to = (children.unsqueeze(-1) >> torch.arange(MAP_COLS, device=tokens.device)) & 1  # [B,R,C,child column]
        far = 1 << 20
        rows = [None] * MAP_ROWS
        rows[-1] = elite[:, -1]
        for r in range(MAP_ROWS - 2, -1, -1):
            open_ = (to[:, r] > 0) & (exists[:, r + 1] > 0).unsqueeze(1)
            onward = torch.where(open_, rows[r + 1].unsqueeze(1), torch.full_like(to[:, r], far)).min(dim=-1).values
            rows[r] = elite[:, r] + torch.where(onward >= far, torch.zeros_like(onward), onward)
        cost = torch.stack(rows, dim=1).view(batch, spare)
        return cost.gather(1, cell)

    def deck_summary(self, tokens, counts):
        """Public Deck bag only: number sums/means, flag counts, total/upgraded cards.

        Upgrade levels index the same static table used by individual card
        features. Multiplicity is n1, not the number of token rows. Slots
        after counts[:, 0] are ignored even if their padding contains cards.
        """
        card = tokens[..., 1].clamp(0, VOCAB["card"] - 1)
        level = tokens[..., N0].clamp(0, CARD_LEVELS - 1)
        positions = torch.arange(tokens.shape[1], device=tokens.device).unsqueeze(0)
        deck = (tokens[..., 0] == TOKEN_DECK) & (card > 0) & (positions < counts[:, :1])
        weight = tokens[..., N0 + 1].clamp(min=0).float() * deck
        count = weight.sum(dim=1, keepdim=True)
        totals = (self.card_numbers[card, level] * weight.unsqueeze(-1)).sum(dim=1)
        flags = (self.card_flags[card, level] * weight.unsqueeze(-1)).sum(dim=1)
        upgraded = (weight * (level > 0)).sum(dim=1, keepdim=True)
        return torch.cat([totals, totals / count.clamp(min=1), flags, count, upgraded], dim=-1)

    def encode(self, global_, tokens, counts):
        """The run vector [B,width] and the token vectors [B,T,width]."""
        batch, token_slots = tokens.shape[:2]
        x = self.token_numbers(numeric(tokens[..., len(ID_COLUMNS):]))
        for column, name in enumerate(ID_COLUMNS):
            x = x + self.embed[name](tokens[..., column].clamp(0, VOCAB[name] - 1))
        if self.power_context is not None:
            owner_context, owner_present = self.power_summary(tokens, counts)
            owner = (tokens[..., N0 + 6] + 1).clamp(0, token_slots)
            context = owner_context.gather(1, owner.unsqueeze(-1).expand(-1, -1, owner_context.shape[-1]))
            present = owner_present.gather(1, owner)
            enemy = (tokens[..., 0] == TOKEN_ENEMY) & present
            x = x + self.power_context(context) * enemy.unsqueeze(-1)
        if self.features >= 2:
            card = tokens[..., 1].clamp(0, VOCAB["card"] - 1)
            # The upgrade level is number 0 of a card token, number 4 of a shop entry.
            level = torch.where(tokens[..., 0] == TOKEN_SHOP, tokens[..., N0 + 4], tokens[..., N0]).clamp(0, CARD_LEVELS - 1)
            printed = torch.cat([numeric(self.card_numbers[card, level]), self.card_flags[card, level]], dim=-1)
            applies = self.embed["power"](self.card_powers[card, level].long()).sum(dim=-2)
            x = x + (self.card_features(printed.to(x.dtype)) + applies) * (card > 0).unsqueeze(-1)
        if self.features >= 3:
            travel = tokens[..., 0] == TOKEN_MAP_NEXT
            if travel.any():
                ahead = self.elites_ahead(tokens)
                best = ahead.masked_fill(~travel, 1 << 20).min(dim=1, keepdim=True).values
                route = torch.stack([ahead, ahead - best], dim=-1) * travel.unsqueeze(-1)
                x = x + self.route(numeric(route).to(x.dtype)) * travel.unsqueeze(-1)
        x = self.token_norm(x)

        scalars = global_.clone()
        scalars[:, list(G_ID_LIKE)] = 0
        g = self.global_numbers(numeric(scalars))
        g = g + self.decision(global_[:, G_DECISION].clamp(0, DECISION_KINDS - 1))
        g = g + self.boss(global_[:, G_BOSS].clamp(0, ENCOUNTERS - 1))
        event = global_[:, G_EVENT]
        g = g + self.embed["misc"](torch.where(event > 0, event + MISC_EVENT, event).clamp(0, VOCAB["misc"] - 1))
        g = g + self.embed["misc"](global_[:, G_PROMPT].clamp(0, VOCAB["misc"] - 1))
        g = g + self.embed["misc"](global_[:, G_SOURCE].clamp(0, VOCAB["misc"] - 1))
        if self.power_context is not None:
            g = g + self.power_context(owner_context[:, 0]) * owner_present[:, :1]
        if self.features >= 2:
            # Shown intents of living enemies: damage per hit times hits.
            alive = (tokens[..., 0] == TOKEN_ENEMY) & (tokens[..., N0 + 7] > 0)
            incoming = (tokens[..., N0 + 3].clamp(min=0) * tokens[..., N0 + 4].clamp(min=1) * alive).sum(dim=1)
            block = global_[:, G_BLOCK]
            extra = torch.stack([incoming, (incoming - block).clamp(min=0), (block - incoming).clamp(min=0)], dim=-1)
            g = g + self.global_extra(numeric(extra).to(g.dtype))
        if self.features >= 4:
            summary = self.deck_summary(tokens, counts)
            g = g + self.deck_features(numeric(summary).to(g.dtype)) * (summary[:, -2:-1] > 0)
        g = self.global_norm(g)

        sequence = torch.cat([g.unsqueeze(1), x], dim=1)
        positions = torch.arange(token_slots, device=tokens.device).unsqueeze(0)
        padding = torch.cat([torch.zeros(batch, 1, dtype=torch.bool, device=tokens.device),
                             positions >= counts[:, :1]], dim=1)
        h = self.final_norm(self.encoder(sequence, src_key_padding_mask=padding))
        return h[:, 0], h[:, 1:]

    def power_summary(self, tokens, counts):
        """[B,T+1,2*width] pools by public owner ordinal (player=0).

        Pool only actual Power rows with a valid owner and id. Empty owners
        are marked separately so an affine-layer bias adds nothing to them.
        Token position and padding never contribute to a pool.
        """
        batch, slots = tokens.shape[:2]
        owner = tokens[..., N0 + 1]
        positions = torch.arange(slots, device=tokens.device).unsqueeze(0)
        valid = ((tokens[..., 0] == TOKEN_POWER) & (tokens[..., 5] > 0) &
                 (owner >= 0) & (owner <= slots) & (positions < counts[:, :1]))
        amount = numeric(tokens[..., N0:N0 + 1]).to(self.embed["power"].weight.dtype)
        embedded = self.embed["power"](tokens[..., 5].clamp(0, VOCAB["power"] - 1))
        weighted = torch.cat([embedded * amount[..., :1], embedded * amount[..., 1:]], dim=-1)
        weighted = weighted * valid.unsqueeze(-1)
        index = owner.long().clamp(0, slots)
        pool = weighted.new_zeros(batch, slots + 1, weighted.shape[-1])
        pool.scatter_add_(1, index.unsqueeze(-1).expand_as(weighted), weighted)
        present = torch.zeros(batch, slots + 1, dtype=torch.long, device=tokens.device)
        present.scatter_add_(1, index, valid.long())
        return pool, present > 0


def grow(model: ActorCritic, features: int, card_table: str, separate_critic: bool,
         power_context: bool = False, action_interaction: bool = False) -> ActorCritic:
    """`model` with card features and/or its own value encoder added, giving the same outputs: the new input
    layers start at zero (and the table of powers a card applies is left empty) and the value encoder starts as
    a copy of the policy's. Returns `model` itself when it already has both."""
    power_context = power_context or model.power_context is not None
    action_interaction = action_interaction or model.action_interaction is not None
    if features == model.features and separate_critic == (model.value_net is not None) \
            and power_context == (model.power_context is not None) \
            and action_interaction == (model.action_interaction is not None):
        return model
    if features >= 2 and model.features < 2 and not card_table:
        raise ValueError("card features need a card table")
    config = {k: v for k, v in model.config.items() if k in ("width", "layers", "heads", "feedforward")}
    grown = ActorCritic(**config, features=features, card_table=card_table,
                        critic="separate" if separate_critic else "shared", power_context=power_context,
                        action_interaction=action_interaction).to(next(model.parameters()).device)
    grown.load_state_dict(model.state_dict(), strict=False)  # a features-2 model keeps its own card table
    if features > model.features:
        fresh = ([grown.card_features, grown.global_extra] if model.features < 2 else []) + \
                ([grown.route] if model.features < 3 <= features else []) + \
                ([grown.deck_features] if model.features < 4 <= features else [])
        for layer in fresh:
            nn.init.zeros_(layer.weight)
            nn.init.zeros_(layer.bias)
        if model.features < 2:
            # The powers a card applies enter through the shared, already trained power embedding, which cannot
            # start at zero; a grown model goes without them (the printed numbers and flags are still there).
            grown.card_powers.zero_()
    if power_context and model.power_context is None:
        nn.init.zeros_(grown.power_context.weight)
        nn.init.zeros_(grown.power_context.bias)
    if separate_critic and model.value_net is None:
        # A separate value encoder has no action-interaction projection: that
        # projection only scores the policy's source/target pair. Copy every
        # critic tensor strictly, without passing policy-only additions to it.
        outer = grown.state_dict()
        grown.value_net.load_state_dict({k: outer[k] for k in grown.value_net.state_dict()})
    return grown


def save(model: ActorCritic, path, **extra) -> None:
    torch.save({"config": model.config, "state": model.state_dict(), **extra}, path)


def load(path, device="cpu") -> tuple[ActorCritic, dict]:
    blob = torch.load(path, map_location=device, weights_only=False)
    config = dict(blob["config"])
    if config.pop("schema") != native.SCHEMA_VERSION:
        raise ValueError("checkpoint was trained on another observation schema")
    model = ActorCritic(**config).to(device)  # a checkpoint without "features" is features 1
    model.load_state_dict(blob["state"])
    return model, blob
