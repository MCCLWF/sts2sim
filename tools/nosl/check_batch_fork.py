#!/usr/bin/env python3
"""Check owned sampler forks at recorded reward-choice and rest boundaries."""
import argparse
import json
from pathlib import Path
import sys

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'python'))
from nosl import native


def buffers(n):
    return {'global': np.zeros((n, native.GLOBAL_WIDTH), np.int32),
            'tokens': np.zeros((n, 192, native.TOKEN_WIDTH), np.int32),
            'actions': np.zeros((n, 160, native.ACTION_WIDTH), np.int32),
            'counts': np.zeros((n, 2), np.int32), 'status': np.zeros(n, np.int32),
            'steps': np.zeros(n, np.int32)}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--trajectories', required=True)
    args = p.parse_args()
    checked = []
    needed = {4, 7}
    for line in Path(args.trajectories).read_text().splitlines():
        trajectory = json.loads(line)
        batch = native.Batch(4, 192, 160, 30000)
        batch.set_filter(trajectory['actor_filter'])
        batch.set_active(1)
        assert batch.reset(0, trajectory['seed'], trajectory['ascension']) == native.RUNNING
        A = buffers(4)

        def step(choices):
            batch.step(np.asarray(choices, dtype=np.int32), *A.values())

        step([-1] * 4)
        for index, row in enumerate(trajectory['action_rows']):
            decision = int(A['global'][0, native.G_DECISION])
            count = int(A['counts'][0, 1])
            if decision in needed and count >= 2 and A['global'][0, 11] == 0:
                before = {k: v[0].copy() for k, v in A.items()}
                assert batch.fork(0, 1) == native.RUNNING
                assert batch.fork(0, 2) == native.RUNNING
                assert batch.fork(1, 3) == native.RUNNING
                batch.set_active(4)
                step([-1] * 4)
                for slot in range(4):
                    for k, v in A.items():
                        assert np.array_equal(v[slot], before[k]), (k, slot, 'initial fork')
                # A skip exercises the copied filter's open-reward/undo history.
                chosen = next((i for i in range(count) if decision == 7 and A['actions'][0, i, 1] < 0), int(row))
                step([-1, chosen, chosen, -1])
                for k, v in A.items():
                    assert np.array_equal(v[1], v[2]), (k, 'same action')
                    assert np.array_equal(v[0], before[k]), (k, 'source mutated')
                    assert np.array_equal(v[3], before[k]), (k, 'sibling mutated')
                assert A['status'][1] in (native.RUNNING, native.VICTORY, native.DEATH)
                after = {k: v[1].copy() for k, v in A.items()}
                step([-1, -1, -1, (chosen + 1) % count])
                for k, v in A.items():
                    assert np.array_equal(v[1], after[k]) and np.array_equal(v[2], after[k]), (k, 'other branch mutated')
                step([chosen, -1, -1, -1])
                for k, v in A.items():
                    assert np.array_equal(v[0], after[k]), (k, 'source continuation/filter differs')
                assert A['steps'][0] > before['steps']
                try:
                    batch.fork(0, 0)
                    raise AssertionError('self fork accepted')
                except ValueError:
                    pass
                checked.append({'seed': trajectory['seed'], 'policy_index': index, 'decision': decision,
                                'actions': count, 'steps_before': int(before['steps']), 'steps_after': int(A['steps'][0])})
                needed.remove(decision)
                break
            step([int(row), -1, -1, -1])
        if not needed:
            break
    if needed:
        raise RuntimeError('recorded trajectories did not cover fork boundaries: ' + str(needed))
    print(json.dumps({'passed': True, 'checked': checked}))


if __name__ == '__main__':
    main()
