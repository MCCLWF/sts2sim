#!/usr/bin/env python3
"""Public feature invariants and independent whole-run CEM/result audits."""
import argparse
import copy
import collections
from fractions import Fraction
import json
from pathlib import Path
import re
import subprocess
import sys
from unittest.mock import patch

import numpy as np
import torch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'python'))
from nosl.branch_credit import KEYS, inputs
from nosl.endturn import baseline, write
from nosl.episodic_strategy import digest
from nosl.model import load as generic_load, load_card_table
from nosl.tactical import FEATURES, POWERS, PRIOR, TacticalPolicy, active, feature_table, load, save
from nosl.teacher_distill import sha

START = ROOT / 'artifacts/nosl-q4r/start.pt'
def lines(path):return [json.loads(s) for s in Path(path).read_text().splitlines() if s]


def contract(learning, out):
    out = Path(out);out.mkdir(parents=True, exist_ok=False)
    base = baseline(START);frozen = digest(base)
    table = ROOT / 'artifacts/nosl-q4e/card_table.json'
    assert sha(table) == '17a35189779ad6774cfa3289a1ee8adf8bc08640e8821618746220151f471f23'
    numbers, flags, powers = load_card_table(table)
    assert all(torch.equal(x, y) for x, y in [(numbers, base.card_numbers), (flags, base.card_flags), (powers, base.card_powers)])
    ids = (ROOT / 'content/generated/ids.h').read_text()
    body = re.search(r'enum class PowerId[^\{]+\{(.*?)\};', ids, re.S).group(1)
    names = [s.strip().split('=')[0].strip() for s in body.split(',') if s.strip()]
    for name, identity in POWERS.items():assert names[identity] == name.upper() + '_POWER'
    cards = {c['entry']: c['id'] for c in json.loads(table.read_text())['cards']}
    assert cards['DAGGER_SPRAY'] == 129
    # Game's basic card faces: StrikeSilent 6, DefendSilent 5, Neutralize 3 + 1 Weak.
    g = torch.zeros(1, 40, dtype=torch.long);g[0, 7:9] = 70;g[0, 11:13] = torch.tensor([1, 3]);g[0, 17] = 3
    t = torch.zeros(1, 8, 16, dtype=torch.long)
    for i, name in enumerate(('STRIKE_SILENT', 'DEFEND_SILENT', 'NEUTRALIZE')):
        t[0, i, 0] = 1;t[0, i, 1] = cards[name];t[0, i, 9] = 0 if i == 2 else 1;t[0, i, 11] = 2 if i == 1 else 1
    t[0, 3, 0] = 6;t[0, 3, 8:16] = torch.tensor([10, 10, 0, 7, 1, 0, 0, 1])
    for i, (power, owner, amount) in enumerate((('strength', 0, 3), ('vulnerable', 1, 1), ('dexterity', 0, 2)), 4):
        t[0, i, 0] = 7;t[0, i, 5] = POWERS[power];t[0, i, 8:10] = torch.tensor([amount, owner])
    a = torch.tensor([[[2, 0, 3, 0], [2, 1, -1, 0], [2, 2, 3, 0], [3, -1, -1, 0]]]);c = torch.tensor([[7, 4]])
    expected = torch.zeros(1, 4, 16, dtype=torch.float64)
    expected[0, 0, [0, 1, 2, 10, 13]] = torch.tensor([10., 1., 7., -1., -1.], dtype=torch.float64)
    expected[0, 1, [3, 10, 13]] = torch.tensor([7., -1., -1.], dtype=torch.float64)
    expected[0, 2, [0, 8, 13]] = torch.tensor([9., 1.75, -1.], dtype=torch.float64)
    assert torch.equal(feature_table(base, g, t, a, c), expected)
    t[0, 7] = 1000000;assert torch.equal(feature_table(base, g, t, a, c), expected)
    save(out / 'R.json', PRIOR)
    models = {'R': TacticalPolicy(base), 'diagnostic': TacticalPolicy(base, PRIOR * np.exp(np.linspace(-1, 1, 16)))}
    if learning:
        for arm in ('H1', 'H3'):models[arm] = load(Path(learning) / arm / 'checkpoint.json', START)[0]
    teacher = ROOT / 'artifacts/nosl-q4o/cloud/labels/public-tables.npz'
    assert sha(teacher) == '8d30a2c7e3ce51c7c0bb73551d062671a4892b5f5d3f241959864d0d74599f29'
    cases = inactive = outside = batch_checks = changed = 0
    with patch('nosl.native.Batch', side_effect=AssertionError('actor attempted an Env call')):
        for path, prefixes in ((ROOT / 'artifacts/nosl-q4ad/data/public-roots.npz', ('',)), (teacher, ('', 'anchor_'))):
            with np.load(path, allow_pickle=False) as raw:
                for prefix in prefixes:
                    data = {key: raw[prefix + key] for key in KEYS}
                    for first in range(0, len(data['counts']), 128):
                        args = inputs(data, np.arange(first, min(first + 128, len(data['counts']))), 'cpu')
                        with torch.no_grad():
                            old, value = base(*args);use, legal = active(args[0], args[2], args[3])
                            for name, model in models.items():
                                scores, now = model(*args)
                                assert torch.equal(value, now) and torch.equal(scores[~use], old[~use].double())
                                assert torch.isneginf(scores[~legal]).all() and torch.isfinite(scores[legal]).all()
                                if name == 'R':changed += int(((scores.argmax(1) != old.argmax(1)) & use).sum())
                            if use.any():
                                selected = [arg[use] for arg in args]
                                feature = feature_table(base, *selected)
                                for row in range(len(feature)):
                                    single = feature_table(base, *[arg[row:row + 1] for arg in selected])[0]
                                    assert torch.allclose(single, feature[row], rtol=0, atol=1e-12)
                                    batch_checks += 1
                        cases += len(args[0]);inactive += int((~use).sum());outside += int((args[0][:, 11] == 0).sum())
    assert cases == 3209 and outside == 368 and changed > 0
    for model in models.values():assert digest(model.base) == frozen
    blob = json.loads((out / 'R.json').read_text());rejected = []
    for reason in ('format', 'schema', 'baseline', 'powers', 'nonfinite', 'negative', 'size'):
        bad = copy.deepcopy(blob)
        if reason == 'format':bad['format'] = 'old-endturn'
        elif reason == 'schema':bad['schema'] = 2
        elif reason == 'baseline':bad['baseline_checkpoint_sha256'] = 'wrong'
        elif reason == 'powers':bad['powers']['weak'] = 0
        elif reason == 'nonfinite':bad['weights'][0] = float('nan')
        elif reason == 'negative':bad['weights'][0] = -1
        else:bad['weights'].pop()
        path = out / (reason + '-invalid.json');write(path, bad)
        try:load(path, START)
        except ValueError:rejected.append(reason)
        else:raise AssertionError('invalid tactical checkpoint accepted')
    try:generic_load(out / 'R.json', 'cpu')
    except Exception:rejected.append('generic_loader')
    else:raise AssertionError('generic loader silently dropped tactical policy')
    result = {'passed': True, 'public_states': cases, 'inactive': inactive, 'outside': outside,
        'active_batch_shape_feature_checks': batch_checks, 'R_changed_greedy_decisions': changed,
        'basic_face_and_owner_golden_verified': True, 'padding_ignored': True,
        'base_tensors_value_nonapplicable_scores_masks_unchanged': True,
        'actor_has_no_Env_calls': True, 'invalid_checkpoints_rejected': rejected,
        'frozen_X1_digest': frozen, 'models_checked': list(models)}
    write(out / 'result.json', result);print(json.dumps(result), flush=True);return 0


def learning(directory, out):
    directory = Path(directory);summary = json.loads((directory / 'summary.json').read_text())
    assert summary['completed'];verified = {};total = 0
    for seed in (1, 3):
        arm = f'H{seed}';folder = directory / arm;records = lines(folder / 'generations.jsonl')
        assert len(records) == 4
        rng = np.random.default_rng(seed);prior = np.log(PRIOR);mean = prior.copy();sigma = np.repeat(.35, 16)
        for generation, record in enumerate(records):
            assert record['generation'] == generation
            assert np.array_equal(mean, record['old_mean']) and np.array_equal(sigma, record['old_sigma'])
            proposals = [mean.copy()]
            proposals.extend(np.clip(rng.normal(mean, sigma, (7, 16)), prior - 1.5, prior + 1.5))
            expected = [f'NQ4AH{seed}TRAIN{generation * 32 + i:06d}' for i in range(32)]
            before = json.loads((folder / f'g{generation}-proposals.json').read_text())
            assert before['seed_bank'] == expected and np.array_equal(before['proposals'], proposals)
            assert before['rng_state_after_proposals'] == rng.bit_generator.state
            assert record['seed_bank'] == expected
            objectives = []
            for index, candidate in enumerate(record['candidates']):
                assert candidate['candidate'] == index and np.array_equal(candidate['log_weights'], proposals[index])
                rows = lines(folder / f'g{generation}-c{index}.jsonl')
                assert [r['seed'] for r in rows] == expected
                assert all(r['mc_qualified'] and r['status'] in (1, 2) and r['end'] in ('death', 'victory') for r in rows)
                assert all(r['ascension'] == 10 and r['actor_filter'] == 1 for r in rows)
                score = Fraction(sum(r['floor'] for r in rows) + 100 * sum(r['end'] == 'victory' for r in rows), 1600)
                assert float(score) == candidate['objective'];objectives.append(score);total += len(rows)
                stats = candidate['statistics']
                assert stats['terminals'] == stats['attempts'] == 32
                assert stats['floor_mean'] == sum(r['floor'] for r in rows) / 32
                assert stats['victories'] == sum(r['end'] == 'victory' for r in rows)
            assert len(objectives) == 8
            chosen = sorted(range(8), key=lambda i: (-objectives[i], i))[:2]
            assert chosen == record['elite']
            pair = np.stack([proposals[i] for i in chosen])
            mean = .5 * mean + .5 * pair.mean(0);sigma = np.maximum(.1, .5 * sigma + .5 * pair.std(0))
            assert np.array_equal(mean, record['next_mean']) and np.array_equal(sigma, record['next_sigma'])
        final = json.loads((folder / 'optimizer-final.json').read_text())
        assert np.array_equal(mean, final['mean']) and np.array_equal(sigma, final['sigma'])
        assert final['rng_state'] == rng.bit_generator.state
        model, blob = load(folder / 'checkpoint.json', START)
        assert np.array_equal(np.exp(mean), blob['weights']) and not np.array_equal(mean, prior)
        assert blob['training']['final_generation'] == 4 and blob['training']['learning_seed'] == seed
        assert sha(folder / 'checkpoint.json') == summary['training'][arm]['checkpoint_sha256']
        verified[arm] = {'generations': 4, 'candidate_runs': 1024, 'unique_run_seeds': 128,
            'all_proposals_elites_mean_sigma_rng_and_final_weights_identical': True}
    assert total == 2048
    result = {'independent_verification': True, 'actual_terminal_runs': total, 'heads': verified}
    write(out, result);print(json.dumps(result), flush=True);return 0


def evaluation(directory, out):
    directory = Path(directory);config = json.loads((directory / 'experiment.json').read_text())
    result = json.loads((directory / 'results.json').read_text())
    assert config['mode'] == 'evaluate' and config['runs'] == 1024 and config['seconds'] == 1800
    assert config['device'] == 'cpu' and config['greedy_batch'] == 64 and config['cloud_used'] is False
    rows, statistics = {}, {}
    expected = [f'NQ4AHEVAL{i:06d}' for i in range(1024)]
    for arm in ('X1', 'R', 'H1', 'H3'):
        attempts = lines(directory / 'evaluation' / (arm + '.jsonl'));rows[arm] = attempts
        assert [r['seed'] for r in attempts] == expected
        assert all(r['mc_qualified'] and r['status'] in (1, 2) and r['end'] in ('death', 'victory') for r in attempts)
        assert all(r['ascension'] == 10 and r['actor_filter'] == 1 for r in attempts)
        stats = {'attempts': 1024, 'terminals': 1024, 'ends': dict(collections.Counter(r['end'] for r in attempts)),
            'floor_mean': sum(r['floor'] for r in attempts) / 1024,
            'act2': sum(r['act'] >= 1 for r in attempts), 'act3': sum(r['act'] >= 2 for r in attempts),
            'victories': sum(r['end'] == 'victory' for r in attempts),
            'endturn_decisions': sum(r['endturn_decisions'] for r in attempts),
            'policy_decisions': sum(len(r['action_rows']) for r in attempts)}
        assert stats == result['policies'][arm];statistics[arm] = stats
        chosen = attempts[:2]
        deepest = next(r for r in attempts if r['floor'] == max(s['floor'] for s in attempts))
        if deepest['seed'] not in {r['seed'] for r in chosen}:chosen.append(deepest)
        for r in attempts:
            if r['end'] == 'victory' and r['seed'] not in {s['seed'] for s in chosen}:chosen.append(r)
        assert chosen == lines(directory / (arm + '-normal-inputs.jsonl'))
    paired = {}
    for arm, baseline_arm in [('R', 'X1'), ('H1', 'R'), ('H3', 'R'), ('H1', 'X1'), ('H3', 'X1')]:
        delta = np.asarray([a['floor'] - b['floor'] for a, b in zip(rows[arm], rows[baseline_arm])])
        rng = np.random.default_rng(20261006);means = []
        for _ in range(40):means.extend(delta[rng.integers(0, 1024, (250, 1024))].mean(1).tolist())
        ci = np.quantile(means, [.025, .975]);threshold = 1. if baseline_arm == 'R' else 2.
        paired[arm + '-' + baseline_arm] = {'floor_gain': float(delta.mean()), 'ci95': ci.tolist(),
            'threshold': threshold, 'gate': bool(delta.mean() >= threshold and ci[0] > 0)}
    assert paired == result['paired']
    gate = all(paired[f'{h}-{b}']['gate'] for h in ('H1', 'H3') for b in ('R', 'X1')) and all(
        statistics[h][key] >= max(statistics[b][key] for b in ('X1', 'R'))
        for h in ('H1', 'H3') for key in ('act3', 'victories'))
    assert result['all_evaluation_runs_terminal'] and result['gate_passed'] == gate
    verified = {'independent_verification': True, 'all_4096_actual_terminals': True,
        'normal_selection_independently_verified': True, 'policies': statistics, 'paired': paired, 'gate_passed': gate}
    write(out, verified);print(json.dumps(verified), flush=True);return 0


def main():
    p = argparse.ArgumentParser(description=__doc__);p.add_argument('mode', choices=('contract', 'learning', 'evaluation'))
    p.add_argument('--learning');p.add_argument('--evaluation');p.add_argument('--out', required=True);a = p.parse_args();torch.set_num_threads(1)
    if a.mode == 'evaluation':return evaluation(a.evaluation, a.out)
    return contract(a.learning, a.out) if a.mode == 'contract' else learning(a.learning, a.out)


if __name__ == '__main__':sys.exit(main())
