#!/usr/bin/python3
# SPDX-License-Identifier: Apache-2.0
"""Exercise the real GPT engine against sparse regular-file copies of Aries GPTs."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import zlib

parser = argparse.ArgumentParser()
parser.add_argument('--binary', required=True)
parser.add_argument('--fixture', required=True, help='directory with captured primary/backup GPT and layout-before.json')
parser.add_argument('--output', required=True)
args = parser.parse_args()
fixture = Path(args.fixture)
layout = json.loads((fixture / 'layout-before.json').read_text())
primary = (fixture / 'primary-gpt.bin').read_bytes()
backup = (fixture / 'backup-gpt.bin').read_bytes()
sectors = layout['disk_bytes'] // 512
count, stride = struct.unpack_from('<II', primary, 512 + 80)
entries = primary[1024:1024 + count * stride]
checks = 0
scenarios = []

def check(value, message):
    global checks
    checks += 1
    assert value, message

def snapshot(image):
    with image.open('rb') as f:
        a = f.read(34 * 512)
        f.seek((sectors - 33) * 512)
        b = f.read(33 * 512)
    return a, b

def setup(base):
    image = base / 'disk.bin'
    store = base / 'store'
    store.mkdir()
    with image.open('wb') as f:
        f.truncate(layout['disk_bytes'])
        f.write(primary)
        f.seek((sectors - 33) * 512)
        f.write(backup)
        f.seek((sectors - 33) * 512 - 16384)
        f.write(b'OLD-ENCRYPTION-FOOTER'.ljust(16384, b'X'))
        for p in layout['partitions'][:20]:
            f.seek(p['start'] * 512)
            f.write(b'PROTECTED-ARIES-' + bytes([p['number']]) * 32)
    return image, store

def run(image, store, command, *extra, env=None, rc=0):
    p = subprocess.run([args.binary, '--image', str(image), '--store', str(store), command, *map(str, extra)],
                       env=dict(os.environ, **(env or {})), capture_output=True, text=True)
    check(p.returncode == rc, f'{command} returned {p.returncode}, expected {rc}: {p.stderr}\n{p.stdout}')
    return p.stdout

def protected(image):
    a, b = snapshot(image)
    check(a[:512] == primary[:512], 'protective MBR changed')
    check(a[1024:1024 + 20 * stride] == entries[:20 * stride], 'protected GPT entries changed')
    check(a[1024 + 23 * stride:1024 + count * stride] == entries[23 * stride:], 'unused GPT entries changed')
    with image.open('rb') as f:
        for p in layout['partitions'][:20]:
            f.seek(p['start'] * 512)
            check(f.read(48) == b'PROTECTED-ARIES-' + bytes([p['number']]) * 32, 'protected partition payload changed')

def complete(image, store, mib):
    run(image, store, 'apply', mib, 'yes')
    out = run(image, store, 'probe')
    check('phase=finish' in out and 'ready=1' not in out, 'must finish pending resize first')
    run(image, store, 'finish', 'yes', env={'ARIES_TEST_STALE_KERNEL': '1'}, rc=1)
    check(not (store / '.aries-repartition/formatted-system').exists(), 'stale kernel was formatted')
    run(image, store, 'finish', 'yes')
    with image.open('rb') as f:
        f.seek((sectors - 33) * 512 - 16384)
        check(f.read(16384) == bytes(16384), 'old encryption footer was not cleared')
    out = run(image, store, 'probe')
    check('ready=1' in out and f'current={mib}\n' in out, 'completed geometry differs')
    protected(image)

with tempfile.TemporaryDirectory(prefix='aries-gpt-test-') as tmp:
    base = Path(tmp)
    for mib in (512, 513, 840, 1024, 1536, 2500, 3071, 3072):
        case = base / str(mib)
        case.mkdir()
        image, store = setup(case)
        plan = json.loads(run(image, store, 'plan', mib))
        check(plan['system_mib'] == mib and plan['cache_mib'] == 384, 'capacity calculation differs')
        check(plan['system_end'] + 1 == plan['cache_start'] and plan['cache_end'] + 1 == plan['data_start'], 'partitions overlap')
        check(plan['data_end'] == sectors - 34, 'Data end moved')
        complete(image, store, mib)
        scenarios.append(f'resize-{mib}')
    case = base / 'repeat'; case.mkdir(); image, store = setup(case)
    for mib in (3072, 512, 2048, 1024, 3072):
        for p in (store / '.aries-repartition').glob('formatted-*'):
            p.unlink()
        complete(image, store, mib)
    scenarios.append('repeated-grow-shrink')
    unchanged = snapshot(image)
    run(image, store, 'apply', 3072, 'yes', rc=1)
    check(snapshot(image) == unchanged, 'same-size request modified GPT')
    for value in ('511', '3073', '-1', '0', '4294967296', '1024x'):
        run(image, store, 'apply', value, 'yes', rc=1)
        check(snapshot(image) == unchanged, 'invalid size modified GPT')
    for confirm in ('', 'YES', 'y', 'yes;echo unsafe'):
        run(image, store, 'apply', 1024, confirm, rc=1)
        check(snapshot(image) == unchanged, 'missing confirmation modified GPT')
    run(image, store, 'apply', 1024, 'yes', env={'ARIES_TEST_MOUNTED': '1'}, rc=1)
    check(snapshot(image) == unchanged, 'mounted partition modified')
    with (store / 'tool.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        run(image, store, 'apply', 1024, 'yes', rc=1)
        check(snapshot(image) == unchanged, 'concurrent writer modified GPT')
    run(image, store, 'apply', 1024, 'yes', env={'ARIES_TEST_RECOVERY_NOT_INSTALLED': '1'}, rc=1)
    check(snapshot(image) == unchanged, 'temporary Recovery boot changed GPT')
    scenarios.append('authorization-mount-lock-range-guards')
    scenarios.append('installed-Recovery-required')
    for step in range(1, 5):
        case = base / f'interrupt-{step}'; case.mkdir(); image, store = setup(case)
        run(image, store, 'apply', 3072, 'yes', env={'ARIES_TEST_FAIL_WRITE': str(step)}, rc=75)
        out = run(image, store, 'probe')
        check('ready=1' not in out, 'interrupted operation offered fresh resize')
        run(image, store, 'apply', 1024, 'yes', rc=1)
        run(image, store, 'repair', 'yes', env={'ARIES_TEST_CID': 'f' * 32}, rc=1)
        run(image, store, 'repair', 'yes')
        run(image, store, 'finish', 'yes')
        protected(image)
        scenarios.append(f'interrupted-GPT-write-{step}-repair')
    for step in range(1, 5):
        case = base / f'restore-{step}'; case.mkdir(); image, store = setup(case)
        run(image, store, 'apply', 3072, 'yes', env={'ARIES_TEST_FAIL_WRITE': str(step)}, rc=75)
        run(image, store, 'restore', 'yes')
        check(snapshot(image) == (primary, backup), 'previous GPT not restored exactly')
        protected(image)
        scenarios.append(f'interrupted-GPT-write-{step}-restore')
    for step in range(1, 4):
        case = base / f'format-{step}'; case.mkdir(); image, store = setup(case)
        run(image, store, 'apply', 3072, 'yes')
        run(image, store, 'finish', 'yes', env={'ARIES_TEST_FAIL_FORMAT': str(step)}, rc=76)
        out = run(image, store, 'probe'); check('phase=finish' in out, 'format failure lost pending journal')
        run(image, store, 'finish', 'yes'); protected(image)
        scenarios.append(f'interrupted-format-{step}')
    case = base / 'journal-crc'; case.mkdir(); image, store = setup(case)
    run(image, store, 'apply', 3072, 'yes')
    unchanged = snapshot(image)
    journal = store / '.aries-repartition/journal.bin'
    data = bytearray(journal.read_bytes()); data[-1] ^= 1; journal.write_bytes(data)
    run(image, store, 'probe', rc=1);run(image, store, 'finish', 'yes', rc=1)
    check(snapshot(image) == unchanged, 'damaged journal modified GPT')
    scenarios.append('journal-CRC-guard')
    case = base / 'protected-tamper'; case.mkdir(); image, store = setup(case)
    run(image, store, 'apply', 3072, 'yes', env={'ARIES_TEST_FAIL_WRITE': '1'}, rc=75)
    with image.open('r+b') as f:
        f.seek(1024 + 17 * stride + 32); value=f.read(1);f.seek(-1,1);f.write(bytes([value[0] ^ 1]))
    unchanged=snapshot(image)
    run(image, store, 'repair', 'yes', rc=1);run(image, store, 'restore', 'yes', rc=1)
    check(snapshot(image) == unchanged, 'protected-entry tampering accepted')
    scenarios.append('protected-entry-tamper-guard')
report={'result':'passed','scenarios':scenarios,'scenario_count':len(scenarios),'assertions':checks,
        'fixture':'read-only GPT capture from Aries 132b4897','fixture_primary_sha256':hashlib.sha256(primary).hexdigest(),
        'formatting':'host-only markers simulate format completion/failure; no real device formatted'}
Path(args.output).write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'result':'passed','scenario_count':len(scenarios),'assertions':checks}))
