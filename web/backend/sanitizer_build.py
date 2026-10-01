"""Sanitizer-build detection shared by the hosted OS/service limit tests.

hosted-limits.md section 3 declares that a sanitizer build cannot run under
the 1 GiB RLIMIT_AS the service applies to its solver child, and that the two
hosted tests may lift only that bound when they are pointed at a sanitizer
build (printing that they did so). This module answers that one question:

    is this solver binary built with a sanitizer runtime?

The answer is read from the binary itself rather than from how a capped child
happened to die, because the failure modes differ across runtimes: a static
compiler-rt runtime prints an AddressSanitizer/ThreadSanitizer banner and
exits nonzero, while some dynamically linked runtimes die silently (signal 11)
with nothing on stderr to key on. A release binary contains none of the
marker strings and links none of the sanitizer libraries, so the check cannot
misfire on the configuration the contract actually deploys.
"""

import pathlib
import subprocess

_MARKERS = (
    b"AddressSanitizer",
    b"ThreadSanitizer",
    b"UndefinedBehaviorSanitizer",
    b"LeakSanitizer",
)
# Substrings of the runtime's SONAME in `ldd` output: GCC ships libasan/
# libtsan/libubsan/liblsan, compiler-rt ships libclang_rt.asan-* and friends.
_SANITIZER_LIBRARIES = (b"asan", b"lsan", b"tsan", b"ubsan")


def is_sanitizer_build(solver):
    """True when `solver` is linked against a sanitizer runtime.

    Two probes cover both linkage styles: statically linked compiler-rt
    runtimes embed their own name in the executable image, while dynamically
    linked runtimes (GCC's, or compiler-rt's shared variants) show up as
    DT_NEEDED entries in `ldd`.
    """
    path = pathlib.Path(solver)
    try:
        blob = path.read_bytes()
    except OSError:
        return False
    if any(marker in blob for marker in _MARKERS):
        return True
    try:
        linked = subprocess.run(["ldd", str(path)],
                                capture_output=True, timeout=30).stdout
    except (OSError, subprocess.SubprocessError):
        return False
    return any(name in linked for name in _SANITIZER_LIBRARIES)
