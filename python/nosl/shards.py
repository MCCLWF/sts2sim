"""Strategic decisions of collected runs kept on disk, so that training does not have to replay the runs.

A shard is one compressed file of decisions taken outside combat: the public
observation (the three tables, unpadded), the action row taken, whether it
was exploratory, and the run it belongs to with that run's real end (floor,
act, status). Only runs that ended in an original victory or death are
written. Nothing here is hidden state: the tables are the actor's
observation, and a shard can be rebuilt from the recorded action rows
(nosl.strategy_q shards) if it is lost.

Reading yields training batches padded to the widest decision in the batch,
with decisions of similar size batched together and a bound on padded tokens
per batch, so that the map screen's hundred tokens cannot exhaust the GPU.
"""
from __future__ import annotations

from pathlib import Path

import numpy as np

from . import native

# Per-decision scalars: the action row taken, whether it was exploratory, and the run's index and real end.
SCALARS = {"choice": np.int16, "explored": bool, "floor": np.int16, "run_act": np.int8, "status": np.int8, "run": np.int32}


class ShardWriter:
    def __init__(self, directory, decisions_per_shard=100_000):
        self.directory = Path(directory)
        self.directory.mkdir(parents=True, exist_ok=True)
        if any(self.directory.glob("shard-*.npz")):
            raise FileExistsError(f"{self.directory} already holds shards")
        self.limit = decisions_per_shard
        self.count = self.decisions = 0
        self._reset()

    def _reset(self):
        self.parts = {k: [] for k in ("g", "tok", "actions", "nt", "na", *SCALARS)}
        self.size = 0

    def add(self, chunk):
        """n decisions as arrays: global [n,G], tokens [n,T,W] and actions [n,A,4] (padded), counts [n,2], SCALARS."""
        n = len(chunk["choice"])
        if n == 0:
            return
        nt, na = chunk["counts"][:, 0].astype(np.int64), chunk["counts"][:, 1].astype(np.int64)
        self.parts["g"].append(np.asarray(chunk["global"], np.int32))
        self.parts["tok"].append(np.concatenate([chunk["tokens"][i, :nt[i]] for i in range(n)])
                                 .astype(np.int32).reshape(-1, native.TOKEN_WIDTH))
        self.parts["actions"].append(np.concatenate([chunk["actions"][i, :na[i]] for i in range(n)])
                                     .astype(np.int32).reshape(-1, native.ACTION_WIDTH))
        self.parts["nt"].append(nt.astype(np.int32))
        self.parts["na"].append(na.astype(np.int32))
        for key, dtype in SCALARS.items():
            self.parts[key].append(np.asarray(chunk[key]).astype(dtype))
        self.size += n
        if self.size >= self.limit:
            self.flush()

    def flush(self):
        if not self.size:
            return
        np.savez_compressed(self.directory / f"shard-{self.count:05d}.npz",
                            **{k: np.concatenate(v) for k, v in self.parts.items()})
        self.count += 1
        self.decisions += self.size
        self._reset()


def shard_paths(directories):
    paths = []
    for directory in directories:
        found = sorted((Path(directory) / "shards").glob("shard-*.npz"))
        if not found:
            raise FileNotFoundError(f"{directory} has no shards; build them with nosl.strategy_q shards")
        paths += [(str(directory), p) for p in found]
    return paths


def load_shard(path):
    """One shard as flat arrays, with the offsets of every decision's token and action rows."""
    with np.load(path) as z:
        data = {k: z[k] for k in z.files}
    data["tok_at"] = np.concatenate([[0], np.cumsum(data["nt"])[:-1]]).astype(np.int64)
    data["act_at"] = np.concatenate([[0], np.cumsum(data["na"])[:-1]]).astype(np.int64)
    return data


def pad(flat, at, count, rows):
    """[len(rows), widest, width]: the variable-length rows of `flat` selected by `rows`, zero padded."""
    width = max(1, int(count[rows].max()))
    position = at[rows, None] + np.arange(width)[None, :]
    inside = np.arange(width)[None, :] < count[rows, None]
    return flat[np.minimum(position, len(flat) - 1)] * inside[..., None]


def batches(data, rows, batch_size, token_budget, rng):
    """Batches over the decisions `rows` of one loaded shard: similar sizes together, random batch order."""
    order = rows[np.argsort(data["nt"][rows] + rng.random(len(rows)))]
    cuts, begin = [], 0
    while begin < len(order):
        widest = int(data["nt"][order[min(len(order), begin + batch_size) - 1]])
        size = max(32, min(batch_size, token_budget // max(1, widest)))
        cuts.append(order[begin:begin + size])
        begin += size
    for which in rng.permutation(len(cuts)):
        pick = np.sort(cuts[which])
        yield {"global": data["g"][pick],
               "tokens": pad(data["tok"], data["tok_at"], data["nt"], pick),
               "actions": pad(data["actions"], data["act_at"], data["na"], pick),
               "counts": np.stack([data["nt"][pick], data["na"][pick]], axis=1).astype(np.int32),
               **{k: data[k][pick] for k in SCALARS}}
