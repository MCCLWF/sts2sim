"""Write a checkpoint's weights for the native forward pass (nosl/policy_net.h).

  .venv/bin/python -m nosl.export --checkpoint artifacts/nosl-q2/s1/checkpoint.pt --out artifacts/nosl-q2/s1/policy.bin

The file is little-endian: int32 header {magic 'NOSM', format, observation
schema, width, layers, heads, feedforward, tensor count}, then every tensor
in the fixed order below as {int32 rank, int32 dims..., float32 data}.
A features-1 model is written as format 1. A features-2, -3 or -4 model is format 2:
one more header field (features), then after the format-1 tensors the card
table and its layers, for features >= 3 the route layer, and for features 4
the deck-summary layer. Builds that lack that feature reject it explicitly.
Power-context models use format 3, with (features, power_context=1) after
the header and the two owner-pooling tensors after the feature tensors.
Action-interaction models use format 4, with (features, power_context,
action_interaction=1) and a final bias-free interaction projection.

A checkpoint with a separate critic is written without it: the file holds the
policy, and its value output is not the critic's.
"""
from __future__ import annotations

import argparse

import numpy as np

from .model import load

MAGIC, FORMAT, FORMAT_FEATURES, FORMAT_POWER_CONTEXT, FORMAT_ACTION_INTERACTION = 0x4D534F4E, 1, 2, 3, 4


def tensor_names(layers: int, features: int = 1, power_context: bool = False, action_interaction: bool = False) -> list[str]:
    names = [f"embed.{k}.weight" for k in ("kind", "card", "relic", "potion", "monster", "power", "misc")]
    names += ["token_numbers.weight", "token_numbers.bias", "token_norm.weight", "token_norm.bias",
              "decision.weight", "boss.weight", "global_numbers.weight", "global_numbers.bias",
              "global_norm.weight", "global_norm.bias"]
    for i in range(layers):
        p = f"encoder.layers.{i}."
        names += [p + n for n in ("self_attn.in_proj_weight", "self_attn.in_proj_bias", "self_attn.out_proj.weight",
                                  "self_attn.out_proj.bias", "linear1.weight", "linear1.bias", "linear2.weight",
                                  "linear2.bias", "norm1.weight", "norm1.bias", "norm2.weight", "norm2.bias")]
    names += ["final_norm.weight", "final_norm.bias", "action_kind.weight", "action_target.weight", "action_target.bias",
              "action_number.weight", "action_number.bias", "policy.0.weight", "policy.0.bias", "policy.2.weight",
              "policy.2.bias", "value.0.weight", "value.0.bias", "value.2.weight", "value.2.bias"]
    if features >= 2:
        names += ["card_numbers", "card_flags", "card_powers", "card_features.weight", "card_features.bias",
                  "global_extra.weight", "global_extra.bias"]
    if features >= 3:
        names += ["route.weight", "route.bias"]
    if features >= 4:
        names += ["deck_features.weight", "deck_features.bias"]
    if power_context:
        names += ["power_context.weight", "power_context.bias"]
    if action_interaction:
        names += ["action_interaction.weight"]
    return names


def export(checkpoint: str, out: str, critic: bool = False) -> dict:
    """`critic`: write the checkpoint's separate critic instead, as a network of its own. Its value output is the
    critic's estimate (its action logits are untrained). Offline diagnostics only."""
    model, _ = load(checkpoint, "cpu")
    if critic:
        if model.value_net is None:
            raise ValueError("the checkpoint has no separate critic")
        model = model.value_net
    config, state = model.config, model.state_dict()
    features = config.get("features", 1)
    power_context = bool(config.get("power_context", False))
    action_interaction = bool(config.get("action_interaction", False))
    names = tensor_names(config["layers"], features, power_context, action_interaction)
    # A separate critic (model.value_net) is a training-time part: the policy's logits do not read it, so it is
    # left out. The native value output is then the policy trunk's own value head, which nothing trained.
    separate_critic = any(k.startswith("value_net.") for k in state)
    missing = {k for k in set(state) - set(names) if not k.startswith("value_net.")}
    if missing:
        raise ValueError(f"the checkpoint has tensors the native format does not know: {sorted(missing)}")
    with open(out, "wb") as stream:
        format_ = FORMAT_ACTION_INTERACTION if action_interaction else \
            FORMAT_POWER_CONTEXT if power_context else FORMAT if features == 1 else FORMAT_FEATURES
        extra = [features, int(power_context), 1] if action_interaction else \
            [features, 1] if power_context else [features] if features != 1 else []
        np.array([MAGIC, format_, config["schema"], config["width"], config["layers"],
                  config["heads"], config["feedforward"], len(names)] + extra,
                 dtype="<i4").tofile(stream)
        for name in names:
            tensor = state[name].detach().cpu().numpy().astype("<f4")
            np.array([tensor.ndim, *tensor.shape], dtype="<i4").tofile(stream)
            tensor.tofile(stream)
    return {"tensors": len(names), "parameters": int(sum(state[n].numel() for n in names)),
            "separate_critic_left_out": separate_critic}


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--checkpoint", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--critic", action="store_true", help="write the separate critic as a network of its own")
    args = p.parse_args(argv)
    print(export(args.checkpoint, args.out, args.critic))


if __name__ == "__main__":
    main()
