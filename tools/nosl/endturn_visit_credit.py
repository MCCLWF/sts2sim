#!/usr/bin/env python3
"""Offline visitation replay and one-action intervention; never a deployed actor."""
import argparse
import collections
import json
from pathlib import Path
import resource
import subprocess
import sys
import time

import numpy as np
import torch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'python'))
from nosl import native
from nosl.branch_credit import KEYS, buffers, inputs
from nosl.endturn import decode, scope, write
from nosl.endturn_action import load, public_features, source
from nosl.endturn_credit import public_digest, same_slot, REWARD_COLUMNS
from nosl.episodic_strategy import public_row
from nosl.returns import RunRewards, reward_state
from nosl.teacher_distill import padded_stack, sha
from check_endturn_credit import replay

ARMS = ('B1', 'C1', 'B3', 'C3')
START = ROOT / 'artifacts/nosl-q4r/start.pt'
HEADS = ROOT / 'artifacts/nosl-q4ac/qualification'
PRIOR = ROOT / 'artifacts/nosl-q4ae'
LEARNING = PRIOR / 'learning'
SEEDS = [f'NQ4AEEVAL{i:06d}' for i in range(128)]
MANIFESTS = {
    'nosl-q4ac': '65a97ceebdd214c6d12417df1e418a6d0968afc9a84fa0ed510129bfe3f08530',
    'nosl-q4ad': '724d51d67ed89761939fd2d90477e20fc1cb431bafb7393361b6fb3ef2878878',
    'nosl-q4ae': '90bd24c738b2e89b3d7a84832dd159a164bba77d9a9f06295396778c42afa9a0'}


def lines(path):return [json.loads(s) for s in Path(path).read_text().splitlines() if s]


def sealed_inputs():
    verified = {}
    for name, digest in MANIFESTS.items():
        directory = ROOT / 'artifacts' / name
        assert sha(directory / 'manifest.json') == digest
        manifest = json.loads((directory / 'manifest.json').read_text())
        for record in manifest['files']:
            path = directory / record['path']
            assert path.stat().st_size == record['bytes'] and sha(path) == record['sha256'], str(path)
        verified[name] = len(manifest['files'])
    return verified


def bucket(floor):return '1-3' if floor < 4 else '4-9' if floor < 10 else '10+'


def inference(model, reference, data, slots):
    public = inputs(data, np.asarray(slots), 'cpu')
    with torch.no_grad():
        gate, action, old, _ = public_features(model.base, *public)
        changed, initial = model.stop(gate, action), reference.head(gate).squeeze(-1)
        choice = decode(old, changed, *[public[i] for i in (0, 2, 3)]).argmax(1)
        previous = decode(old, initial, *[public[i] for i in (0, 2, 3)]).argmax(1)
        active = scope(public[0], public[2], public[3])[0]
    return choice.numpy(), previous.numpy(), active.numpy(), changed.numpy(), initial.numpy()


def initialize_reward(rewarder, state, slot):
    rewarder.boss_ref[slot] = rewarder.boss_paid[slot] = 0
    if state.inside_boss[slot]:
        reference = max(float(state.enemy_hp[slot]), float(state.enemy_max_hp[slot]), 1.)
        rewarder.boss_ref[slot] = reference
        rewarder.boss_paid[slot] = np.clip(1 - state.enemy_hp[slot] / reference, 0, 1)


def tick(batch, data, selected, live, traces, rewarder, seeds):
    before = reward_state(data['global'], data['tokens'], data['counts'], .2, 70)
    hp = data['global'][:, native.G_HP].copy()
    batch.step(selected, *data.values())
    after = reward_state(data['global'], data['tokens'], data['counts'], .2, 70)
    finished = [{'slot': slot, 'seed': seeds[slot], 'status': int(data['status'][slot]),
                 **batch.run_info(slot)} for slot in live if data['status'][slot] != native.RUNNING]
    rewards, _ = rewarder.step(before, after, finished, seeds)
    for slot in live:
        if slot not in traces:continue
        traces[slot]['actions'].append(int(selected[slot]))
        traces[slot]['rewards'].append([int(before.floor[slot]), int(hp[slot]),
            int(before.inside_boss[slot]), int(before.enemy_hp[slot]), int(before.enemy_max_hp[slot]),
            int(after.floor[slot]), int(data['global'][slot, native.G_HP]), int(after.inside_boss[slot]),
            int(after.enemy_hp[slot]), int(after.enemy_max_hp[slot]), int(data['status'][slot]), float(rewards[slot])])


def terminal(batch, data, slot, seed, actions):
    status = int(data['status'][slot]);qualified = status in (native.DEATH, native.VICTORY)
    result = {'seed': seed, 'status': status, 'end': native.STATUS_NAMES[status],
              **batch.run_info(slot), 'ascension': 10, 'actor_filter': 1,
              'action_rows': actions, 'mc_qualified': qualified}
    if not qualified:result['error'] = 'registered deadline' if status == native.RUNNING else batch.last_error(slot)
    return result


def collect(out):
    out = Path(out);out.mkdir(parents=True, exist_ok=False)
    verified = sealed_inputs()
    models = {arm: load(LEARNING / arm / 'checkpoint.pt', START, HEADS)[0] for arm in ARMS}
    references = {seed: source(START, HEADS, seed) for seed in (1, 3)}
    config = {'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
              'source_sha256': sha(__file__), 'seeds': SEEDS, 'arms': list(ARMS), 'seconds': 900,
              'block': 64, 'continuation_batch': 1, 'max_steps': 30000, 'device': 'cpu', 'cloud_used': False,
              'native_sha256': sha(ROOT / 'build/libsts2nosl_c.so'), 'sealed_verified': verified,
              'head_sha256': {arm: sha(LEARNING / arm / 'checkpoint.pt') for arm in ARMS},
              'prior_sha256': {arm: sha(PRIOR / 'experiment/evaluation' / (arm + '.jsonl')) for arm in ARMS},
              'reward_columns': list(REWARD_COLUMNS), 'continuation': 'same_updated_head_both_branches',
              'scope': 'retrospective_development_diagnosis_not_new_strength_holdout'}
    write(out / 'config.json', config)
    began = time.monotonic();deadline = began + 900
    roots, public_roots, attempts, outcomes = [], [], [], []
    controls = {'frozen_fork_after_original_terminal': 0, 'matched_original_actions': 0}
    with (out / 'visits.jsonl').open('w') as visits, (out / 'outcomes.jsonl').open('w') as stream:
        for arm in ARMS:
            prior = lines(PRIOR / 'experiment/evaluation' / (arm + '.jsonl'))
            assert len(prior) == 1024 and [r['seed'] for r in prior[:128]] == SEEDS
            assert all(r['mc_qualified'] for r in prior)
            model, reference = models[arm], references[int(arm[1])]
            for first in range(0, 128, 64):
                scheduled = prior[first:first + 64];n = len(scheduled);seeds = [r['seed'] for r in scheduled]
                if time.monotonic() >= deadline:
                    attempts.extend({'arm': arm, 'seed': seed, 'end': 'not_started', 'reproduced': False} for seed in seeds)
                    continue
                batch = native.Batch(3 * n, 256, 256, 30000);batch.set_filter(1);batch.set_active(n)
                data = buffers(3 * n, 256, 256)
                for i, seed in enumerate(seeds):assert batch.reset(i, seed, 10) == native.RUNNING
                batch.step(np.full(3 * n, -1, np.int32), *data.values())
                histories = [[] for _ in seeds];found, original_traces = {}, {}
                original_rewards = RunRewards(3 * n, gamma=1, boss_damage=.3)
                while np.any(data['status'][:n] == native.RUNNING) and time.monotonic() < deadline:
                    live = np.flatnonzero(data['status'][:n] == native.RUNNING).tolist()
                    choice, initial, active, stop, old_stop = inference(model, reference, data, live)
                    selected = np.full(3 * n, -1, np.int32)
                    state = reward_state(data['global'], data['tokens'], data['counts'], .2, 70)
                    for j, slot in enumerate(live):
                        decision = len(histories[slot]);row = scheduled[slot]
                        assert decision < len(row['action_rows']) and int(choice[j]) == row['action_rows'][decision], (arm, seeds[slot], decision)
                        assert 0 <= choice[j] < data['counts'][slot, 1]
                        controls['matched_original_actions'] += 1
                        selected[slot] = choice[j]
                        if active[j]:
                            record = {'arm': arm, 'seed': seeds[slot], 'decision': decision,
                                'floor': int(data['global'][slot, native.G_TOTAL_FLOOR]),
                                'act': int(data['global'][slot, native.G_ACT]),
                                'energy': int(data['global'][slot, 12]), 'block': int(data['global'][slot, 9]),
                                'choice': int(choice[j]), 'initial_choice': int(initial[j]),
                                'choice_kind': int(data['actions'][slot, choice[j], 0]),
                                'initial_kind': int(data['actions'][slot, initial[j], 0]),
                                'stop': float(stop[j]), 'initial_stop': float(old_stop[j])}
                            visits.write(json.dumps(record) + '\n')
                            if choice[j] != initial[j] and slot not in found:
                                assert batch.fork(slot, n + slot) == native.RUNNING
                                assert batch.fork(slot, 2 * n + slot) == native.RUNNING
                                root = {**record, 'prefix': histories[slot].copy(),
                                    'public': public_row(data, slot),
                                    'frozen': {key: value[slot].copy() for key, value in data.items()}}
                                root['audit_root'] = sum(r['arm'] == arm for r in roots) + len(found) < 4
                                root['public_sha256'] = public_digest(root['public']);found[slot] = root
                                original_traces[slot] = {'actions': [], 'rewards': []}
                                initialize_reward(original_rewards, state, slot)
                        else:assert choice[j] == initial[j]
                        histories[slot].append(int(choice[j]))
                    tick(batch, data, selected, live, original_traces, original_rewards, seeds * 3)
                for slot, seed in enumerate(seeds):
                    actual = terminal(batch, data, slot, seed, histories[slot])
                    reproduced = actual['mc_qualified'] and actual['action_rows'] == scheduled[slot]['action_rows']
                    if reproduced:
                        for key in ('status', 'end', *native.RUN_INFO_FIELDS):assert actual[key] == scheduled[slot][key], (arm, seed, key)
                    attempts.append({'arm': arm, 'seed': seed, 'root': slot in found,
                        'reproduced': reproduced, **{key: value for key, value in actual.items() if key != 'action_rows'}})
                original_terminals = {slot: terminal(batch, data, slot, seeds[slot], histories[slot]) for slot in found}
                batch.set_active(3 * n);batch.step(np.full(3 * n, -1, np.int32), *data.values())
                traces = {}
                counter_rewards = RunRewards(3 * n, gamma=1, boss_damage=.3)
                initial_state = reward_state(data['global'], data['tokens'], data['counts'], .2, 70)
                for slot, root in found.items():
                    for counter in (n + slot, 2 * n + slot):
                        assert same_slot(data, counter, root['frozen']), (arm, root['seed'], 'fork root changed')
                        controls['frozen_fork_after_original_terminal'] += 1
                        traces[counter] = {'actions': [], 'rewards': []}
                        initialize_reward(counter_rewards, initial_state, counter)
                first_action = True
                while any(data['status'][slot] == native.RUNNING for slot in traces) and time.monotonic() < deadline:
                    live = [slot for slot in traces if data['status'][slot] == native.RUNNING]
                    selected = np.full(3 * n, -1, np.int32)
                    if first_action:
                        for slot in live:
                            parent = slot % n
                            selected[slot] = found[parent]['choice' if slot < 2 * n else 'initial_choice']
                    else:
                        with torch.no_grad():
                            for slot in live:
                                logits, _ = model(*inputs(data, np.asarray([slot]), 'cpu'))
                                selected[slot] = int(logits[0].argmax())
                    tick(batch, data, selected, live, traces, counter_rewards, seeds * 3)
                    if first_action:
                        for slot, root in found.items():
                            if root['audit_root']:
                                np.savez_compressed(out / f'{arm}-{root["seed"]}-fork.npz',
                                    **{'root_' + k: v for k, v in root['frozen'].items()},
                                    **{'updated_' + k: v[n + slot] for k, v in data.items()},
                                    **{'initial_' + k: v[2 * n + slot] for k, v in data.items()})
                    first_action = False
                for slot, root in found.items():
                    record = {key: value for key, value in root.items() if key not in ('public', 'frozen', 'updated_after')}
                    pair = []
                    for label, position, trace in (('updated', n + slot, traces[n + slot]), ('initial_once', 2 * n + slot, traces[2 * n + slot])):
                        actual = terminal(batch, data, position, root['seed'], root['prefix'] + trace['actions'])
                        actual.update(arm=arm, branch=label, reward_steps=trace['rewards'],
                            branch_actions=trace['actions'], return_from_root=sum(r[-1] for r in trace['rewards']) if actual['mc_qualified'] else None)
                        stream.write(json.dumps(actual) + '\n');pair.append(actual);outcomes.append(actual)
                    qualified = all(r['mc_qualified'] for r in pair)
                    record.update(complete=qualified,
                        original_terminal=original_terminals[slot],
                        canonical_updated_matches_prior=pair[0]['action_rows'] == scheduled[slot]['action_rows'],
                        floor_delta=pair[1]['floor'] - pair[0]['floor'] if qualified else None,
                        return_delta=pair[1]['return_from_root'] - pair[0]['return_from_root'] if qualified else None)
                    roots.append(record);public_roots.append(root['public'])
                stream.flush();visits.flush()
                for name, rows in (('roots.jsonl', roots), ('attempts.jsonl', attempts)):
                    (out / name).write_text(''.join(json.dumps(row) + '\n' for row in rows))
                print(json.dumps({'arm': arm, 'processed': first + n, 'roots': len(roots),
                    'seconds': time.monotonic() - began}), flush=True)
                del batch
    packed = padded_stack(public_roots) if public_roots else {k: v for k, v in buffers(0, 1, 1).items() if k in KEYS}
    np.savez_compressed(out / 'public-roots.npz', **packed)
    result = {'attempts': len(attempts), 'reproduced': sum(r['reproduced'] for r in attempts),
        'roots': len(roots), 'complete_roots': sum(r['complete'] for r in roots),
        'outcomes': len(outcomes), 'seconds': time.monotonic() - began, 'controls': controls,
        'all_prior_reproduced': len(attempts) == 512 and all(r['reproduced'] for r in attempts),
        'all_root_terminal': all(r['complete'] for r in roots),
        'maxrss_mib': resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024,
        'public_roots_sha256': sha(out / 'public-roots.npz')}
    write(out / 'summary.json', result);print(json.dumps(result), flush=True)
    return 0 if result['all_prior_reproduced'] and result['all_root_terminal'] else 1


def normal(data, out):
    data, out = Path(data), Path(out);out.mkdir(parents=True, exist_ok=False)
    attempts = lines(data / 'outcomes.jsonl');rows = []
    for arm in ARMS:
        candidates = [r for r in attempts if r['arm'] == arm and r['branch'] == 'initial_once']
        selected = candidates[:2]
        complete = [r for r in candidates if r['mc_qualified']]
        if complete:
            deepest = max(complete, key=lambda row: row['floor'])
            if deepest['seed'] not in {r['seed'] for r in selected}:selected.append(deepest)
        for row in complete:
            if row['end'] == 'victory' and row['seed'] not in {r['seed'] for r in selected}:selected.append(row)
        for trajectory in selected:
            result = replay(trajectory, out / arm);result['arm'] = arm;rows.append(result)
            with (out / 'results.jsonl').open('a') as stream:stream.write(json.dumps(result) + '\n')
            print(json.dumps(result), flush=True)
    summary = {'runs': len(rows), 'whole_run_qualified': sum(r['whole_run_qualified'] for r in rows),
        'decisions_matched': sum(r.get('comparison', {}).get('matched', 0) for r in rows),
        'selection': 'per_arm_first_two_roots_deepest_and_all_victories_deduplicated',
        'oracle_test_mode': False, 'verify_snapshots': True,
        'failures': [r for r in rows if not r['whole_run_qualified']]}
    write(out / 'summary.json', summary);print(json.dumps(summary), flush=True)
    return 0 if rows and not summary['failures'] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('collect', 'normal'))
    parser.add_argument('--data');parser.add_argument('--out', required=True)
    args = parser.parse_args();torch.set_num_threads(1)
    return collect(args.out) if args.mode == 'collect' else normal(args.data, args.out)


if __name__ == '__main__':sys.exit(main())
