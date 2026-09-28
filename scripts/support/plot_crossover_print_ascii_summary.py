from .plot_crossover_config import (
    Any, BACKEND_TO_SERIES, Dict, List, Optional, SERIES_ORDER, SOLVED_STATUS, csv
)

def _to_float(val: Any) -> float:
    try:
        return float(val)
    except (ValueError, TypeError):
        return float("nan")

def _to_int(val: Any) -> int:
    try:
        return int(float(val))
    except (ValueError, TypeError):
        return 0

def _is_solved(entry: Dict[str, Any]) -> bool:
    status = (entry.get("status") or "").strip()
    if status and status != SOLVED_STATUS:
        return False
    ms = entry.get("ms", float("nan"))
    return ms == ms and ms > 0.0

def _solved_ms(entry: Optional[Dict[str, Any]]) -> float:
    if entry and _is_solved(entry):
        return entry["ms"]
    return float("nan")

def parse_crossover_csv(filepath: str) -> List[Dict[str, Any]]:
    records: List[Dict[str, Any]] = []
    index: Dict[str, Dict[str, Any]] = {}
    with open(filepath, "r", newline="") as f:
        reader = csv.DictReader(f)
        fields = reader.fieldnames or []
        wide = "backend" not in fields
        for row in reader:
            if wide:
                records.append({
                    "instance": row.get("instance", ""),
                    "nonzeros": _to_int(row.get("nonzeros", row.get("nnz", "0"))),
                    "series": {
                        "simplex": {
                            "status": row.get("simplex_status", ""),
                            "ms": _to_float(row.get("simplex_time_ms")),
                            "verified": row.get("pass", "True").lower() == "true",
                        },
                        "cpu_pdlp": {
                            "status": row.get("cpu_pdlp_status", ""),
                            "ms": _to_float(row.get("cpu_pdlp_time_ms")),
                            "verified": row.get("pass", "True").lower() == "true",
                        },
                        "gpu": {
                            "status": row.get("gpu_pdlp_status", ""),
                            "ms": _to_float(row.get("gpu_total_ms")),
                            "verified": row.get("gpu_verified", "True").lower() == "true",
                        },
                    },
                })
                continue

            name = row.get("instance", "")
            rec = index.get(name)
            if rec is None:
                rec = {"instance": name,
                       "nonzeros": _to_int(row.get("nnz", "0")),
                       "series": {}}
                index[name] = rec
                records.append(rec)
            key = BACKEND_TO_SERIES.get((row.get("backend") or "").strip())
            if key is None:
                continue
            rec["series"][key] = {
                "status": (row.get("status") or "").strip(),
                "ms": _to_float(row.get("total_ms")),
                "verified": (row.get("verified") or "").strip().lower() == "true",
            }
    records.sort(key=lambda r: (r["nonzeros"], r["instance"]))
    return records

def _format_ms(entry: Optional[Dict[str, Any]], width: int) -> str:
    if entry is None:
        return "N/A".rjust(width)
    if _is_solved(entry):
        return ("%.*f" % (3 if entry["ms"] < 100 else 1, entry["ms"])).rjust(width)
    status = (entry.get("status") or "?").strip()
    return status[:width].rjust(width)

def _flag(entry: Optional[Dict[str, Any]]) -> str:
    """Compact verification flag: V verified / u unverified / . not solved / - absent."""
    if entry is None:
        return "-"
    if _is_solved(entry):
        return "V" if entry.get("verified") else "u"
    return "."

def find_crossover(records: List[Dict[str, Any]]) -> Optional[Dict[str, Any]]:
    """First instance (ascending nnz) where a first-order engine beats simplex."""
    for rec in records:
        s_ms = _solved_ms(rec["series"].get("simplex"))
        if s_ms != s_ms:
            continue
        options = [(key, _solved_ms(rec["series"].get(key)))
                   for key in ("cpu_pdlp", "gpu")]
        options = [(key, ms) for key, ms in options if ms == ms]
        if not options:
            continue
        key, o_ms = min(options, key=lambda kv: kv[1])
        if o_ms < s_ms:
            return {"instance": rec["instance"], "nonzeros": rec["nonzeros"],
                    "series": key}
    return None

def print_ascii_summary(records: List[Dict[str, Any]]):
    print("\n" + "=" * 100)
    print(" markov-cero Scale Crossover Study — Empirical Scaling Summary (Gap 8 / T-5.13)")
    print("=" * 100)
    header = ("{:<12} {:>9} | {:>13} | {:>13} | {:>13} | {:>10} | {:>9}")
    print(header.format("Instance", "NNZ", "Simplex(ms)", "CPUp(ms)", "GPUp(ms)",
                        "SPD(S/FO)", "Verified"))
    print("-" * 100)

    present = [k for k in SERIES_ORDER
               if any(k in rec["series"] for rec in records)]
    attempted_unsolved = []

    for rec in records:
        s = rec["series"].get("simplex")
        c = rec["series"].get("cpu_pdlp")
        g = rec["series"].get("gpu")
        s_ms = _solved_ms(s)
        c_ms = _solved_ms(c)
        g_ms = _solved_ms(g)
        fo_ms = g_ms if g_ms == g_ms else c_ms
        if s_ms == s_ms and fo_ms == fo_ms and fo_ms > 0:
            spd = "%9.1fx" % (s_ms / fo_ms)
        else:
            spd = "        -"
        flags = ["%s:%s" % (key[0], _flag(rec["series"].get(key)))
                 for key in present]
        print("{:<12} {:>9} | {} | {} | {} | {} | {:>9}".format(
            rec["instance"], rec["nonzeros"],
            _format_ms(s, 13), _format_ms(c, 13), _format_ms(g, 13),
            spd, " ".join(flags)))
        for key in present:
            entry = rec["series"].get(key)
            if entry is not None and not _is_solved(entry):
                attempted_unsolved.append((rec["instance"], key,
                                           entry.get("status") or "?"))

    print("  Verified flags: V=Optimal+verified, u=Optimal not verified, "
          ".=run did not reach Optimal, -=no such run")
    print("-" * 100)
    crossover = find_crossover(records)
    if crossover:
        if crossover["nonzeros"] <= records[0]["nonzeros"]:
            print("[*] Empirical Crossover Point N* <= smallest measured instance "
                  "(%s, %s nonzeros):" % (crossover["instance"],
                                          format(crossover["nonzeros"], ",")))
            print("    %s beats CPU simplex already at the smallest scale measured,"
                  % crossover["series"].replace("_", " "))
            print("    so N* is at or below %s nonzeros; the whole measured range"
                  % format(crossover["nonzeros"], ","))
            print("    sits in the first-order regime (nothing smaller was run).")
        else:
            print("[*] Empirical Crossover Point N*: %s (~%s nonzeros, %s wins)"
                  % (crossover["instance"], format(crossover["nonzeros"], ","),
                     crossover["series"].replace("_", " ")))
            print("    Below N*: simplex pivoting is cheaper; above N*: first-order")
            print("    PDLP SpMV amortizes and takes the lead.")
    else:
        simplex_comparisons = sum(
            _solved_ms(rec["series"].get("simplex")) == _solved_ms(rec["series"].get("simplex"))
            and any(_solved_ms(rec["series"].get(k)) == _solved_ms(rec["series"].get(k))
                    for k in ("cpu_pdlp", "gpu"))
            for rec in records
        )
        if simplex_comparisons == 0:
            print("[*] No simplex-to-first-order crossover can be estimated: "
                  "no comparable CPU simplex result is present.")
        else:
            print("[*] No first-order engine beats simplex in the comparable runs.")
        cpu_gpu = [(_solved_ms(rec["series"].get("cpu_pdlp")),
                    _solved_ms(rec["series"].get("gpu"))) for rec in records]
        cpu_gpu = [(cpu, gpu) for cpu, gpu in cpu_gpu if cpu == cpu and gpu == gpu]
        if cpu_gpu and all(cpu < gpu for cpu, gpu in cpu_gpu):
            print("[*] CPU PDLP is faster than GPU PDLP in all %d comparable rows."
                  % len(cpu_gpu))
    if attempted_unsolved:
        print("[!] runs that never reached Optimal (kept honest in the CSV):")
        for inst, key, status in attempted_unsolved:
            print("      %-12s %-11s %s" % (inst, key, status))
    print("=" * 100 + "\n")
