#!/usr/bin/env python3
"""Validate or apply only the recorded local Lineage24 platform increments."""
import argparse,hashlib,json,os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parent

def git(repo,*args,**kw):
 return subprocess.check_output(['git','-C',str(repo),*args],**kw)
def prepare(top,project):
 repo=top/project['path'];base=project['base_commit']
 if git(repo,'status','--porcelain','--untracked-files=all').strip():
  raise RuntimeError('Uncommitted source: '+project['path'])
 if git(repo,'rev-parse',base+'^{tree}').decode().strip()!=project['baseline_tree']:
  raise RuntimeError('Wrong fixed platform baseline: '+project['path'])
 current=git(repo,'rev-parse','HEAD^{tree}').decode().strip();prefixes=[project['baseline_tree']]
 paths=[]
 with tempfile.TemporaryDirectory(prefix='lineage24-patches-') as temp:
  env=dict(os.environ,GIT_INDEX_FILE=str(Path(temp)/'index'))
  git(repo,'read-tree',base,env=env)
  for item in project['patches']:
   p=ROOT/item['file']
   if hashlib.sha256(p.read_bytes()).hexdigest()!=item['sha256']:
    raise RuntimeError('Patch checksum mismatch: '+item['file'])
   paths.append(p);git(repo,'apply','--cached','--whitespace=nowarn',str(p),env=env)
   prefixes.append(git(repo,'write-tree',env=env).decode().strip())
 if prefixes[-1]!=project['patched_tree']:
  raise RuntimeError('Full replay tree differs: '+project['path'])
 if current not in prefixes:
  raise RuntimeError('Current source is not an exact recorded prefix: '+project['path'])
 count=prefixes.index(current)
 print(project['path']+': full replay tree verified; applied '+str(count)+'/'+str(len(paths)),flush=True)
 return repo,paths[count:],project['patched_tree']

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--check',action='store_true');parser.add_argument('--top',type=Path)
 args=parser.parse_args();top=args.top or next(p for p in ROOT.parents if (p/'build/envsetup.sh').exists())
 series=json.loads((ROOT/'series.json').read_text())
 snap=top/series['port_docs_snapshot']
 if hashlib.sha256(snap.read_bytes()).hexdigest()!=series['port_docs_snapshot_sha256']:
  raise RuntimeError('Wrong port_docs snapshot')
 plans=[prepare(top,p) for p in series['projects']]
 if not args.check:
  for repo,pending,tree in plans:
   if pending:subprocess.check_call(['git','-C',str(repo),'am',*map(str,pending)])
   if git(repo,'rev-parse','HEAD^{tree}').decode().strip()!=tree:
    raise RuntimeError('Final source tree mismatch: '+str(repo))
 print('Local Lineage24 platform series '+('validated' if args.check else 'ready'),flush=True)
if __name__=='__main__':main()
