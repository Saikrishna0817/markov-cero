# Building

Requirements: CMake 3.25+, a C++20 compiler, Python 3.10+, and POSIX shell utilities.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Use `gcc-debug`, `gcc-release`, `clang-debug`, or `clang-release` presets when the matching compiler is installed. Unix Makefiles are the baseline generator; Ninja may be selected explicitly when installed. CUDA is neither required nor detected in M0.

## GLPK comparison baseline on Arch/Omarchy

The W9 comparison calls `glpsol` as a separate process. A system install is
`sudo pacman -S glpk`. When sudo is unavailable, the tested Arch package can be
kept inside the ignored `.venv` directory:

```sh
curl -fL https://stable-mirror.omarchy.org/extra/os/x86_64/glpk-5.0-3-x86_64.pkg.tar.zst \
  -o /tmp/glpk-5.0-3-x86_64.pkg.tar.zst
printf '%s  %s\n' 0ade444b2c7e411eafbb7af75cfed39cd7d686f4eba042aecf08508184e597e4 \
  /tmp/glpk-5.0-3-x86_64.pkg.tar.zst | sha256sum -c -
mkdir -p .venv/local_glpk
bsdtar -xf /tmp/glpk-5.0-3-x86_64.pkg.tar.zst -C .venv/local_glpk
cat > .venv/bin/glpsol <<'EOF'
#!/bin/sh
glpk_root="$(dirname "$0")/../local_glpk/usr"
LD_LIBRARY_PATH="$glpk_root/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    exec "$glpk_root/bin/glpsol" "$@"
EOF
chmod +x .venv/bin/glpsol
.venv/bin/glpsol --version
```

The pinned archive checksum matches the `pacman -Sii glpk` repository entry on
the tested host. The solver core never links against GLPK.
