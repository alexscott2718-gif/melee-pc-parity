"""summ.py RUNDIR: one line per recording of RUNDIR/results.jsonl, then a summary over the recordings
(tamper controls, any name containing 'tamper', are listed but not counted)."""
import json, sys, statistics
rows = [json.loads(l) for l in open(sys.argv[1] + "/results.jsonl")]
corpus = [r for r in rows if "tamper" not in r["recording"]]
firsts = []
for r in rows:
    d = r.get("first_divergence")
    v = r.get("first_velocity_divergence")
    if "error" in r:
        s = "ERROR " + r["error"]
    elif d is None:
        s = "EXACT over %d frames" % r["exact_frames"]
    elif "diffs" in d:
        x = d["diffs"][0]
        s = "f%d p%d %s ulps=%s act=0x%X af=%g" % (d["frame"], x["port"], x["field"], x.get("ulps"),
            d["ref_context"][str(x["port"])]["action"], d["ref_context"][str(x["port"])]["action_frame"])
    else:
        s = "f%d %s" % (d["frame"], d.get("why"))
    vs = "" if not v else " | vel f%d p%d %s %s ulps act=0x%X af=%g" % (v["frame"], v["port"], v["field"], v["ulps"], v["action"], v["action_frame"])
    im = r.get("input_mismatch")
    ims = "" if not im else " | INPUT-MISMATCH f%d p%d" % (im["frame"], im["port"])
    print("%-14s %s%s%s" % (r["recording"], s, vs, ims))
exact = [r for r in corpus if r.get("first_divergence") is None and "error" not in r]
fd = [r["first_divergence"]["frame"] for r in corpus if r.get("first_divergence")]
print("SUMMARY: %d/%d exact; first divergence median %s min %s (of %d divergent); exact frames total %d / %d" % (
    len(exact), len(corpus), statistics.median(fd) if fd else "-", min(fd) if fd else "-", len(fd),
    sum(r.get("exact_frames", 0) for r in corpus), sum(r["last"] - r["first"] + 1 for r in corpus if "last" in r)))
