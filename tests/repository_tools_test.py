"""Offline dataset integrity and solver-independent proof CLI acceptance."""
import gzip
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from scripts.run_netlib import ensure_instance
from scripts.run_full_benchmark import run_one
try:
    ensure_instance('missing-instance', '/tmp/markov-no-dataset')
    raise AssertionError('missing dataset was accepted')
except FileNotFoundError:
    pass
spec = importlib.util.spec_from_file_location('datasets', ROOT / 'scripts/datasets.py')
datasets = importlib.util.module_from_spec(spec)
spec.loader.exec_module(datasets)
with tempfile.TemporaryDirectory() as directory:
    datasets.ROOT = Path(directory)
    raw = b'NAME CACHE_TEST\nENDATA\n'
    digest = hashlib.sha256(raw).hexdigest()
    cache = datasets.ROOT / '.cache/dataset-store' / (digest + '.gz')
    cache.parent.mkdir(parents=True)
    cache.write_bytes(gzip.compress(raw))
    entry = {'path': 'data/netlib/cache-test.mps', 'sha256': digest, 'bytes': len(raw)}
    target = datasets.materialize(entry)
    assert target.read_bytes() == raw
    target.write_bytes(b'corrupt')
    try:
        datasets.materialize(entry)
        raise AssertionError('corrupt data accepted')
    except ValueError:
        pass
    entry['path'] = '../escape.mps'
    try:
        datasets.materialize(entry)
        raise AssertionError('unsafe path accepted')
    except ValueError:
        pass
    if len(sys.argv) == 3:
        model = Path(directory) / 'proof.mps'
        model.write_text('NAME PROOF\nROWS\n N COST\n L CAP\nCOLUMNS\n X COST -1 CAP 1\n'
                         'RHS\n R CAP 1.5\nBOUNDS\n LI B X 0\n UI B X 2\nENDATA\n')
        run = subprocess.run([sys.argv[1], str(model)], capture_output=True, text=True, check=True)
        for option, value in [('--threads', '-1'), ('--max-nodes', '12junk'),
                              ('--mip-gap', 'nan'), ('--tolerance', 'inf'),
                              ('--iteration-limit', '-1'), ('--proof-time-limit', 'nan'),
                              ('--proof-max-nodes', '0'), ('--proof-max-values', '16000001')]:
            bad_option = subprocess.run([sys.argv[1], str(model), option, value], capture_output=True)
            assert bad_option.returncode == 8, (option, value, bad_option.returncode)
        result = json.loads(run.stdout)
        assert result['verified'] and result['certificate_type'] == 'independent_mip_tree', result
        assert result['mip_proof_build_ms'] >= 0 and result['mip_proof_verify_ms'] >= 0, result
        proof = Path(directory) / 'proof.txt'
        proof.write_text(result['mip_proof'])
        subprocess.run([sys.argv[2], str(model), str(proof), '--time-limit', '10',
                        '--max-nodes', '100', '--max-values', '1000'],
                       check=True, capture_output=True)
        too_small = subprocess.run([sys.argv[2], str(model), str(proof), '--max-nodes', '1'],
                                   capture_output=True)
        assert too_small.returncode != 0
        bad_gap = subprocess.run([sys.argv[2], str(model), str(proof), '--relative-gap', '1'],
                                 capture_output=True)
        assert bad_gap.returncode == 2
        proof.write_text(result['mip_proof'] + '\nEXTRA\n')
        bad = subprocess.run([sys.argv[2], str(model), str(proof)], capture_output=True)
        assert bad.returncode != 0
print('Offline cache integrity, path validation and CLI proof replay passed')
