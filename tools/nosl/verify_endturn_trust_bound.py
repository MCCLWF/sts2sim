#!/usr/bin/env python3
"""Independently replay projected optimization from raw terminal labels."""
import argparse
import collections
import json
from pathlib import Path
import sys

import numpy as np
import torch
import torch.nn.functional as F

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'python'))
from nosl.branch_credit import KEYS, inputs
from nosl.endturn import write
from nosl.endturn_action import initial, load, public_features
from nosl.episodic_strategy import digest
from nosl.teacher_distill import sha
from verify_endturn_credit import metrics, same

START = ROOT / 'artifacts/nosl-q4r/start.pt'
HEADS = ROOT / 'artifacts/nosl-q4ac/qualification'
CACHE = ROOT / 'artifacts/nosl-q4ae/learning/cache'


def lines(path):return [json.loads(s) for s in Path(path).read_text().splitlines() if s]


def divergences(score, initial_score):
    p = torch.sigmoid(initial_score)
    return p * (F.logsigmoid(initial_score) - F.logsigmoid(score)) + (
        1 - p) * (F.logsigmoid(-initial_score) - F.logsigmoid(-score))


def learning(directory, out):
    directory = Path(directory)
    config = json.loads((directory / 'learning-config.json').read_text())
    summary = json.loads((directory / 'summary.json').read_text())
    assert config['device'] == 'cpu' and config['precision'] == 'float32' and not config['cloud_used']
    assert config['limits'] == [.02, .2] and config['working_limits'] == [.0199, .1999]
    assert config['seconds'] == summary['registered_seconds'] == 180 and summary['seconds'] <= 180
    assert sha(CACHE / 'cache.npz') == config['cached_input_sha256']
    roots = lines(ROOT / 'artifacts/nosl-q4ad/data/roots.jsonl')
    branches = lines(ROOT / 'artifacts/nosl-q4ad/data/branches.jsonl')
    pair = {(r['seed'], r['branch']): r for r in branches}
    target, direction, weight, held = [], [], [], []
    for root in roots:
        a, b = pair[(root['seed'], 'end')], pair[(root['seed'], 'continue')]
        assert a['mc_qualified'] and b['mc_qualified'] and a['end'] == b['end'] == 'death'
        delta = sum(r[-1] for r in a['reward_steps']) - sum(r[-1] for r in b['reward_steps'])
        assert root['delta'] == delta and root['complete']
        used = abs(delta) > 1e-5
        target.append(int(delta > 0) if used else 0);direction.append(used)
        weight.append(min(abs(delta) / .02, 5.) if used else 0.);held.append(root['held'])
    expected = {'complete': np.ones(450, bool), 'held': np.asarray(held, bool),
        'direction': np.asarray(direction, bool), 'target': np.asarray(target, np.float32), 'weight': np.asarray(weight, np.float32)}
    with np.load(CACHE / 'cache.npz', allow_pickle=False) as raw:
        for key, value in expected.items():assert np.array_equal(raw[key], value)
        gate, action = torch.from_numpy(raw['gate']), torch.from_numpy(raw['action'])
    target, weight = torch.from_numpy(expected['target']), torch.from_numpy(expected['weight'])
    anchors = np.flatnonzero(~expected['held'])
    rows = np.flatnonzero(expected['direction'] & ~expected['held'])
    held_rows = np.flatnonzero(expected['direction'] & expected['held'])
    assert (len(anchors), len(rows), len(held_rows)) == (352, 177, 49)
    results = {};total_steps = 0
    for seed in (1, 3):
        u = directory / f'U{seed}';t = directory / f'T{seed}'
        old = ROOT / f'artifacts/nosl-q4ae/learning/C{seed}'
        same(torch.load(u / 'checkpoint.pt', map_location='cpu', weights_only=False),
             torch.load(old / 'checkpoint.pt', map_location='cpu', weights_only=False))
        same(torch.load(u / 'optimizer.pt', map_location='cpu', weights_only=False),
             torch.load(old / 'optimizer.pt', map_location='cpu', weights_only=False))
        final, blob = load(t / 'checkpoint.pt', START, HEADS)
        model = initial(START, HEADS, seed, True);parameters = model.learned_parameters()
        optimizer = torch.optim.Adam(parameters, lr=.0003)
        with torch.no_grad():reference = model.stop(gate, action).detach().clone()
        records = lines(t / 'steps.jsonl');epochs = lines(t / 'training.jsonl')
        assert len(records) == 256 and len(epochs) == 129 and blob['final_epoch'] == 128
        assert blob['training'] == json.loads((t / 'config.json').read_text())
        rng = np.random.default_rng(seed);step = 0
        for epoch in range(1, 129):
            order = rng.permutation(rows)
            for first in range(0, len(order), 128):
                selected = order[first:first + 128];record = records[step]
                assert record['epoch'] == epoch and record['update'] == step + 1
                assert record['minibatch_indices'] == selected.tolist()
                prediction = model.stop(gate[selected], action[selected])
                label_loss = (F.binary_cross_entropy_with_logits(prediction, target[selected], reduction='none') * weight[selected]).mean()
                regularizer = divergences(model.stop(gate[anchors], action[anchors]), reference[anchors]).mean()
                optimizer.zero_grad(set_to_none=True);(label_loss + .1 * regularizer).backward()
                torch.nn.utils.clip_grad_norm_(parameters, 1., error_if_nonfinite=True)
                before = [p.detach().clone() for p in parameters];optimizer.step()
                proposal = [p.detach().clone() for p in parameters]
                with torch.no_grad():proposed_kl = divergences(model.stop(gate, action)[anchors], reference[anchors])
                assert float(proposed_kl.mean()) == record['proposal_mean_kl']
                assert float(proposed_kl.max()) == record['proposal_max_kl']
                found_alpha = None
                # Recompute every larger trial, rather than trusting logged alpha.
                for attempt in range(22):
                    alpha = 2. ** -attempt if attempt <= 20 else 0.
                    with torch.no_grad():
                        for p, prior, proposed in zip(parameters, before, proposal):
                            p.copy_(proposed if alpha == 1 else prior if not alpha else prior + alpha * (proposed - prior))
                        divergence = divergences(model.stop(gate, action)[anchors], reference[anchors])
                    if float(divergence.mean()) <= .0199 and float(divergence.max()) <= .1999 or not alpha:
                        found_alpha = alpha;break
                assert found_alpha == record['alpha'] and attempt == record['halvings']
                assert float(divergence.mean()) == record['accepted_mean_kl'] <= .02
                assert float(divergence.max()) == record['accepted_max_kl'] <= .2
                actual = sum(float((p.detach() - prior).square().sum()) for p, prior in zip(parameters, before))
                assert actual == record['actual_parameter_delta_squared'];step += 1
            with torch.no_grad():scores = model.stop(gate, action)
            assert metrics(scores[rows], target[rows]) == epochs[epoch]['train']
            assert metrics(scores[held_rows], target[held_rows]) == epochs[epoch]['held']
        same(model.state_dict(), final.state_dict())
        same(optimizer.state_dict(), torch.load(t / 'optimizer.pt', map_location='cpu', weights_only=False))
        assert digest(model.base) == digest(final.base) == blob['frozen_parameter_sha256']
        assert not any(p.requires_grad or p.grad is not None for p in model.base.parameters())
        with torch.no_grad():
            final_scores = model.stop(gate, action)
            divergence = divergences(final_scores[anchors], reference[anchors])
            bce = float((F.binary_cross_entropy_with_logits(final_scores[rows], target[rows], reduction='none') * weight[rows]).mean())
        assert bce == epochs[-1]['weighted_bce']
        assert epochs[0]['weighted_bce'] - bce >= .001
        results[f'T{seed}'] = {'full_optimizer_replay_steps': step, 'all_parameters_and_Adam_identical': True,
            'mean_anchor_kl': float(divergence.mean()), 'max_anchor_kl': float(divergence.max()),
            'projected_steps': sum(r['alpha'] < 1 for r in records), 'rejected_steps': sum(r['alpha'] == 0 for r in records)}
        total_steps += step
    result = {'independent_verification': True, 'raw_terminal_label_pairs': len(roots),
        'full_T_optimizer_replay_steps': total_steps, 'U_controls_exact_prior_reproductions': True,
        'heads': results}
    write(out, result);print(json.dumps(result), flush=True);return 0


def evaluation(directory, out):
    directory = Path(directory);config = json.loads((directory / 'experiment.json').read_text())
    result = json.loads((directory / 'results.json').read_text())
    assert config['mode'] == 'evaluate' and config['runs'] == 1024 and config['seconds'] == 1800
    assert config['device'] == 'cpu' and config['precision'] == 'float32' and config['greedy_batch'] == 64
    rows, stats = {}, {}
    arms = ('X1', 'E1', 'U1', 'T1', 'E3', 'U3', 'T3')
    expected = [f'NQ4AGEVAL{i:06d}' for i in range(1024)]
    for arm in arms:
        attempts = lines(directory / 'evaluation' / (arm + '.jsonl'));rows[arm] = attempts
        assert [r['seed'] for r in attempts] == expected
        assert all(r['mc_qualified'] and r['status'] in (1, 2) and r['end'] in ('victory', 'death') for r in attempts)
        assert all(r['actor_filter'] == 1 and r['ascension'] == 10 for r in attempts)
        stats[arm] = {'attempts': 1024, 'terminals': 1024,
            'ends': dict(collections.Counter(r['end'] for r in attempts)),
            'floor_mean': sum(r['floor'] for r in attempts) / 1024,
            'act2': sum(r['act'] >= 1 for r in attempts), 'act3': sum(r['act'] >= 2 for r in attempts),
            'victories': sum(r['end'] == 'victory' for r in attempts),
            'endturn_decisions': sum(r['endturn_decisions'] for r in attempts),
            'policy_decisions': sum(len(r['action_rows']) for r in attempts)}
        assert stats[arm] == result['policies'][arm]
        if arm != 'X1':
            chosen = attempts[:2]
            deepest = next(r for r in attempts if r['floor'] == max(s['floor'] for s in attempts))
            if deepest['seed'] not in {r['seed'] for r in chosen}:chosen.append(deepest)
            for r in attempts:
                if r['end'] == 'victory' and r['seed'] not in {s['seed'] for s in chosen}:chosen.append(r)
            assert chosen == lines(directory / (arm + '-normal-inputs.jsonl'))
    comparisons = {}
    for seed in (1, 3):
        arm = 'T' + str(seed)
        for baseline in ('U' + str(seed), 'E' + str(seed), 'X1'):
            delta = np.asarray([a['floor'] - b['floor'] for a, b in zip(rows[arm], rows[baseline])])
            rng = np.random.default_rng(20261006);means = []
            for _ in range(40):means.extend(delta[rng.integers(0, 1024, (250, 1024))].mean(1).tolist())
            ci = np.quantile(means, [.025, .975]);threshold = 2. if baseline.startswith('U') else .5
            comparisons[arm + '-' + baseline] = {'floor_gain': float(delta.mean()), 'ci95': ci.tolist(),
                'threshold': threshold, 'gate': bool(delta.mean() >= threshold and ci[0] > 0)}
    assert comparisons == result['paired']
    mechanism = all(comparisons[f'T{s}-U{s}']['gate'] for s in (1, 3))
    net = all(comparisons[f'T{s}-{b}']['gate'] for s in (1, 3) for b in (f'E{s}', 'X1')) and all(
        stats[f'T{s}']['act3'] >= stats['X1']['act3'] for s in (1, 3))
    assert mechanism == result['mechanism_gate'] and net == result['net_quality_gate']
    assert result['all_evaluation_runs_terminal'] and result['gate_passed'] == (mechanism and net)
    verified = {'independent_verification': True, 'policies': stats, 'paired': comparisons,
        'mechanism_gate': mechanism, 'net_quality_gate': net,
        'all_7168_actual_terminals': True, 'normal_selection_independently_verified': True}
    write(out, verified);print(json.dumps(verified), flush=True);return 0


def main():
    p = argparse.ArgumentParser(description=__doc__);group = p.add_mutually_exclusive_group(required=True)
    group.add_argument('--learning');group.add_argument('--evaluation')
    p.add_argument('--out', required=True);a = p.parse_args();torch.set_num_threads(1)
    return learning(a.learning, a.out) if a.learning else evaluation(a.evaluation, a.out)


if __name__ == '__main__':sys.exit(main())
