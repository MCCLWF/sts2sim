#!/usr/bin/env python3
"""Paired floor/act comparisons from greedy_eval --rows-dir, retaining faults."""
import argparse
import collections
import json
import math
from pathlib import Path

import numpy as np


def read(path):
    rows = [json.loads(line) for line in Path(path).read_text().splitlines() if line]
    result = {r['seed']: r for r in rows}
    if len(result) != len(rows):
        raise ValueError('duplicate evaluation seeds in ' + path)
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--control', required=True)
    p.add_argument('--candidate', required=True)
    p.add_argument('--out', default='')
    args = p.parse_args()
    control, candidate = read(args.control), read(args.candidate)
    if control.keys() != candidate.keys():
        raise ValueError('paired evaluations must attempt the same seeds')
    matched = [s for s in control if control[s]['status'] in (1, 2) and candidate[s]['status'] in (1, 2)]
    if len(matched) < 2:
        raise ValueError('fewer than two pairs reached real terminals')
    a = np.array([control[s]['floor'] for s in matched], dtype=float)
    b = np.array([candidate[s]['floor'] for s in matched], dtype=float)
    delta = b - a
    mean, se = float(delta.mean()), float(delta.std(ddof=1) / math.sqrt(len(delta)))
    result = {'attempts': len(control), 'paired_terminals': len(matched), 'excluded_pairs': len(control) - len(matched),
              'control_ends': dict(collections.Counter(r['status'] for r in control.values())),
              'candidate_ends': dict(collections.Counter(r['status'] for r in candidate.values())),
              'control_floor': float(a.mean()), 'candidate_floor': float(b.mean()),
              'difference': mean, 'difference_se': se, 'difference_ci95': [mean - 1.96 * se, mean + 1.96 * se],
              'control_act2': float(np.mean([control[s]['act'] >= 1 for s in matched])),
              'candidate_act2': float(np.mean([candidate[s]['act'] >= 1 for s in matched]))}
    print(json.dumps(result))
    if args.out:
        Path(args.out).write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
