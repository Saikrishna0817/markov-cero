#!/usr/bin/env python3
"""RES-01 slice E (blueprint step 5 and acceptance): hosted OS hard limits.

The hosted service must spawn each solve with OS CPU/memory/file/output/wall
limits and kill on timeout. This test drives `server.run_solver` directly with
small declared caps so every limit binds deterministically, then runs a real
solve under the service defaults to show the declared caps admit normal work.

Usage: python3 web/backend/os_limits_test.py /path/to/markov-cero-solve
"""

import json
import pathlib
import resource
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import server  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[2]


def require(condition, message):
    if not condition:
        raise SystemExit(f"FAIL: {message}")


def run(command, timeout, cpu_seconds, as_bytes, fsize_bytes, directory):
    return server.run_solver(
        command,
        timeout=timeout,
        cpu_seconds=cpu_seconds,
        as_bytes=as_bytes,
        fsize_bytes=fsize_bytes,
        stdout_path=pathlib.Path(directory) / "stdout.log",
        stderr_path=pathlib.Path(directory) / "stderr.log",
    )


def main():
    solver = sys.argv[1] if len(sys.argv) > 1 else server.SOLVER_BIN
    require(0 < server.SOLVE_RLIMIT_CPU_SECONDS <= server.SOLVE_WALL_TIMEOUT_SECONDS,
            "declared CPU limit must sit at or under the wall timeout")
    require(server.SOLVE_RLIMIT_AS_BYTES >= 512 * 1024 * 1024,
            "declared address-space limit must leave room for real models")
    require(server.SOLVE_RLIMIT_FSIZE_BYTES >= server.MAX_RESULT,
            "declared file-size limit must admit the result cap")

    with tempfile.TemporaryDirectory(prefix="markov-os-limits-") as directory:
        # Wall limit: the child is killed and TimeoutExpired raised promptly.
        started = time.monotonic()
        try:
            run([sys.executable, "-c", "import time; time.sleep(60)"],
                timeout=1, cpu_seconds=8, as_bytes=1024 ** 3,
                fsize_bytes=1024 ** 2, directory=directory)
            raise SystemExit("FAIL: wall timeout did not kill the child")
        except subprocess.TimeoutExpired:
            pass
        require(time.monotonic() - started < 15, "wall timeout must fire promptly")

        # CPU limit: a busy child dies on SIGXCPU / SIGKILL within the cap.
        started = time.monotonic()
        cpu_run = run([sys.executable, "-c", "while True: pass"],
                      timeout=30, cpu_seconds=1, as_bytes=1024 ** 3,
                      fsize_bytes=1024 ** 2, directory=directory)
        require(cpu_run.returncode < 0,
                "the CPU limit must terminate a busy child with a signal")
        require(time.monotonic() - started < 15,
                "the CPU-limited child must die within the wall timeout")

        # Address-space limit: a 4 GiB mapping fits a 16 GiB cap, not 512 MiB.
        mapper = ("import mmap,sys\n"
                  "try:\n"
                  "    m = mmap.mmap(-1, 4 * 1024 ** 3)\n"
                  "    m.close()\n"
                  "    sys.exit(0)\n"
                  "except Exception:\n"
                  "    sys.exit(42)\n")
        pathlib.Path(directory, "mapper.py").write_text(mapper, encoding="utf-8")
        uncapped = run([sys.executable, str(pathlib.Path(directory) / "mapper.py")],
                       timeout=30, cpu_seconds=8, as_bytes=16 * 1024 ** 3,
                       fsize_bytes=1024 ** 2, directory=directory)
        require(uncapped.returncode == 0,
                "the generous address-space cap must admit a 4 GiB mapping")
        capped = run([sys.executable, str(pathlib.Path(directory) / "mapper.py")],
                     timeout=30, cpu_seconds=8, as_bytes=512 * 1024 * 1024,
                     fsize_bytes=1024 ** 2, directory=directory)
        require(capped.returncode != 0,
                "the address-space cap must refuse the same 4 GiB mapping")

        # File-size limit: an 8 MiB write dies under a 1 MiB cap.
        writer = ("f = open(r'" + str(pathlib.Path(directory, "big.bin")) +
                  "', 'wb')\nf.write(b'x' * (8 * 1024 ** 2))\nf.close()\n")
        pathlib.Path(directory, "writer.py").write_text(writer, encoding="utf-8")
        big = run([sys.executable, str(pathlib.Path(directory) / "writer.py")],
                  timeout=30, cpu_seconds=8, as_bytes=1024 ** 3,
                  fsize_bytes=1024 * 1024, directory=directory)
        require(big.returncode != 0,
                "the file-size limit must refuse an 8 MiB write under a 1 MiB cap")

        # Real solve under the declared service defaults: caps admit normal
        # work. A sanitizer build reserves ~14 TB of shadow address space and
        # dies under the declared RLIMIT_AS; retry once with only that bound
        # lifted, and say so (hosted-limits section 3).
        result_path = pathlib.Path(directory) / "result.json"
        real = run([solver, str(ROOT / "examples" / "blend.mps"),
                    "--output", str(result_path)],
                   timeout=30,
                   cpu_seconds=server.SOLVE_RLIMIT_CPU_SECONDS,
                   as_bytes=server.SOLVE_RLIMIT_AS_BYTES,
                   fsize_bytes=server.SOLVE_RLIMIT_FSIZE_BYTES,
                   directory=directory)
        if real.returncode != 0 and b"AddressSanitizer" in (
                pathlib.Path(directory, "stderr.log").read_bytes()):
            print("note: sanitizer build blocked by the declared RLIMIT_AS; "
                  "retrying the control solve with only that bound lifted")
            real = run([solver, str(ROOT / "examples" / "blend.mps"),
                        "--output", str(result_path)],
                       timeout=30,
                       cpu_seconds=server.SOLVE_RLIMIT_CPU_SECONDS,
                       as_bytes=resource.RLIM_INFINITY,
                       fsize_bytes=server.SOLVE_RLIMIT_FSIZE_BYTES,
                       directory=directory)
        require(real.returncode == 0,
                f"a normal solve must run inside the service caps (rc={real.returncode})")
        require(result_path.is_file(), "the capped solve must write its result")
        data = json.loads(result_path.read_text(encoding="utf-8"))
        require(data.get("status") == "Optimal",
                f"the capped control solve must stay Optimal, got {data.get('status')}")

    print("hosted os limits: ok")


if __name__ == "__main__":
    main()
