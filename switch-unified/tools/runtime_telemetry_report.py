#!/usr/bin/env python3
"""Summarize native Halo perf/match records; never infer missing match results."""
import argparse,json,math,re
from pathlib import Path
KINDS={'runtime_identity','perf_frame','perf_resources','perf_network','match_begin','match_end','match_player','match_teams','match_leader'}
KV=re.compile(r'(\w+)=(?:"([^"]*)"|(\S+))')
def value(quoted,plain):
    if quoted is not None:return quoted
    try:
        x=float(plain)
        if not math.isfinite(x):return plain
        return int(x) if re.fullmatch(r'-?\d+',plain) else x
    except (ValueError,TypeError):return plain

def summarize(path):
    records=[]
    for line in path.read_text(errors='replace').splitlines():
        match=re.search(r'halo-linux: (\w+): (.*)',line)
        if not match or match[1] not in KINDS:continue
        record={'kind':match[1],**{k:value(q if q!='' or plain=='' else None,plain) for k,q,plain in KV.findall(match[2])}}
        records.append(record)
    frames=[r for r in records if r['kind']=='perf_frame']
    resources=[r for r in records if r['kind']=='perf_resources']
    finals=[r for r in records if r['kind']=='match_player' and r.get('event')=='final']
    ends=[r for r in records if r['kind']=='match_end']
    checks={
        'timing_recorded':bool(frames),
        'percentiles_ordered':bool(frames) and all(0<=r['wall_ms_p50']<=r['wall_ms_p95']<=r['wall_ms_p99'] and 0<=r['present_ms_p50']<=r['present_ms_p95']<=r['present_ms_p99'] for r in frames),
        'rss_recorded':bool(resources) and all(r.get('rss_kib',0)>0 for r in resources),
        'final_results_recorded':bool(finals) and all(r.get('result') in ('win','loss','tie') for r in finals),
        'identity_recorded':any(r['kind']=='runtime_identity' and bool(re.fullmatch(r'[0-9a-f]{16,64}',str(r.get('elf_build_id','')))) for r in records),
    }
    return {'log':str(path.resolve()),'checks':checks,'frame_reports':len(frames),'latest_frame':frames[-1] if frames else None,'latest_resources':resources[-1] if resources else None,'match_end_events':ends,'final_players':finals,'records':records}

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('logs',nargs='+',type=Path)
    ap.add_argument('--output',type=Path)
    ap.add_argument('--require-perf',action='store_true')
    ap.add_argument('--require-final',action='store_true')
    ap.add_argument('--require-rss',action='store_true')
    ap.add_argument('--require-identity',action='store_true')
    args=ap.parse_args();results=[summarize(p) for p in args.logs]
    required=[]
    if args.require_perf:required+=['timing_recorded','percentiles_ordered']
    if args.require_final:required+=['final_results_recorded']
    if args.require_rss:required+=['rss_recorded']
    if args.require_identity:required+=['identity_recorded']
    report={'passed':all(all(r['checks'][k] for k in required) for r in results),'logs':results,'required_checks':required}
    text=json.dumps(report,indent=2)+'\n'
    if args.output:args.output.write_text(text)
    print(json.dumps({'passed':report['passed'],'logs':[{k:v for k,v in r.items() if k not in ('records','final_players')} for r in results]},indent=2))
    return 0 if report['passed'] else 1
if __name__=='__main__':raise SystemExit(main())
