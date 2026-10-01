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
    if len(sys.argv) >= 3:
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
        assert result['proof_message'], result['proof_message']
        assert result['mip_proof'].startswith('MARKOV_MIP_PROOF 3\n'), result['mip_proof'][:64]
        proof_lines = result['mip_proof'].splitlines()
        node_count = int(proof_lines[1].split()[2])
        nodes_used, witness_values_used, budget_time_ms, exhausted, budget_kind = proof_lines[2].split()
        assert int(nodes_used) == node_count == 3, proof_lines[:3]
        assert int(witness_values_used) > 0 and float(budget_time_ms) >= 0, proof_lines[:3]
        assert (exhausted, budget_kind) == ('0', '0'), proof_lines[:3]
        fingerprint_length, fingerprint = proof_lines[3].split(' ', 1)
        assert int(fingerprint_length) == len(fingerprint) and fingerprint.isdigit(), proof_lines[:4]
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
        proof.write_text(result['mip_proof'].replace('MARKOV_MIP_PROOF 3',
                                                     'MARKOV_MIP_PROOF 7', 1))
        unknown_version = subprocess.run([sys.argv[2], str(model), str(proof)],
                                         capture_output=True, text=True)
        assert unknown_version.returncode == 2, unknown_version
        assert 'unsupported MIP proof format version' in unknown_version.stderr
        proof.write_text(result['mip_proof'] + '\nEXTRA\n')
        bad = subprocess.run([sys.argv[2], str(model), str(proof)], capture_output=True)
        assert bad.returncode != 0
    if len(sys.argv) >= 4:
        # MINLP-02 contract §7 (minlp-proof-replay.md): the checked-in
        # NLOBJ + NLCON fixture solves with an accepted OA proof, and the
        # standalone replay CLI accepts it, rejects an exhausted budget and
        # rejects a corrupted version.
        fixture = ROOT / 'tests/fixtures/minlp_case_a.mps'
        run = subprocess.run([sys.argv[1], str(fixture)], capture_output=True,
                             text=True, check=True)
        result = json.loads(run.stdout)
        assert result['status'] == 'Optimal', result
        assert result['assurance'] == 'oa_replayed', result
        assert result['guarantee_tier'] == 'independent_oa', result
        assert result['verified'] is True and result['proof_status'] == 'accepted', result
        assert result['oa_proof_build_ms'] >= 0 and result['oa_proof_verify_ms'] >= 0, result
        assert result['oa_proof'].startswith('MARKOV_OA_PROOF 1\n'), result['oa_proof'][:32]
        proof = Path(directory) / 'oa_proof.txt'
        proof.write_text(result['oa_proof'])
        accepted = subprocess.run([sys.argv[3], str(fixture), str(proof),
                                   '--time-limit', '10'], capture_output=True, text=True)
        assert accepted.returncode == 0, accepted
        assert 'VERIFIED:' in accepted.stdout, accepted.stdout
        starved = subprocess.run([sys.argv[3], str(fixture), str(proof),
                                  '--max-nodes', '1'], capture_output=True, text=True)
        assert starved.returncode == 1, starved
        assert 'REJECTED:' in starved.stdout, starved.stdout
        proof.write_text(result['oa_proof'].replace('MARKOV_OA_PROOF 1',
                                                    'MARKOV_OA_PROOF 7', 1))
        unknown_version = subprocess.run([sys.argv[3], str(fixture), str(proof)],
                                         capture_output=True, text=True)
        assert unknown_version.returncode == 1, unknown_version
        assert 'REJECTED:' in unknown_version.stdout, unknown_version.stdout
print('Offline cache integrity, path validation and CLI proof replay passed')
