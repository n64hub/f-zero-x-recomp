#!/usr/bin/env bash
# Builds the decomp's US version (lib/fzerox's commit) from your ROM and brings its ELF back for tools/gen_syms.sh. It
# runs in tools/docker/Dockerfile.decomp's image with Docker, here or on a Linux host over ssh (BUILD_HOST). Never in
# CI: the build needs the ROM.
#
# `make init` extracts the ROM's code and assets (splat and Torch), builds with IDO 7.1, and checks the result against
# fzerox.us.rev0.md5. KEEP_MDEBUG=1 keeps each object's .mdebug, and the ELF is linked again with it (fzerox.ld
# discards it), for the static functions' symbols IDO leaves out of the symbol table (tools/gen_syms.py); the code is
# the same bytes. Into build/decomp/: fzerox.us.rev0.mdebug.elf. Nothing from it is committed but what
# tools/gen_syms.sh writes from it.
#
# Usage: [BUILD_HOST=<ssh host>] [DECOMP_DIR=<folder under ~, default fzerox-decomp>] tools/build_decomp.sh [roms/fzerox.us.z64]
set -euo pipefail
cd "$(dirname "$0")/.."
rom="${1:-roms/fzerox.us.z64}"
sha1="5f658e88ffa9de23cba6986a8fd3d3a90d7b4340"
[[ "$(sha1sum "$rom" | cut -d' ' -f1)" == "$sha1" ]] || { echo "$rom isn't F-Zero X (USA) as a .z64" >&2; exit 1; }
commit="$(git ls-tree HEAD lib/fzerox | awk '{print $3}')"
image="fzerox-decomp:$(git hash-object tools/docker/Dockerfile.decomp | cut -c1-12)"
work="${DECOMP_DIR:-fzerox-decomp}/${commit:0:12}"

if [[ -n "${BUILD_HOST:-}" ]]; then
    run() { ssh "$BUILD_HOST" "cd && $1"; }
    put() { scp -q "$1" "$BUILD_HOST:$2"; }
    get() { scp -q "$BUILD_HOST:$1" "$2"; }
else
    run() { (cd ~ && bash -c "$1"); }
    put() { cp "$1" ~/"$2"; }
    get() { cp ~/"$1" "$2"; }
fi

run "mkdir -p $work.docker"
put tools/docker/Dockerfile.decomp "$work.docker/Dockerfile"
put lib/fzerox/tools/requirements-python.txt "$work.docker/requirements-python.txt"
run "docker image inspect $image > /dev/null 2>&1 || docker build -q -t $image $work.docker > /dev/null"
run "[ -d $work/.git ] || git clone -q https://github.com/inspectredc/fzerox $work
     git -C $work checkout -q $commit && git -C $work submodule update -q --init --recursive"
put "$rom" "$work/baserom.us.rev0.z64"
run "docker run --rm --name fzerox-decomp-${commit:0:12} -u \$(id -u):\$(id -g) -e HOME=/tmp -v \$HOME/$work:/fz -w /fz $image bash -c '
    set -e
    make toolchain > toolchain.log 2>&1 || { tail -30 toolchain.log; exit 1; }
    make init KEEP_MDEBUG=1 > build.log 2>&1 || { tail -30 build.log; exit 1; }
    grep -q \"fzerox.us.rev0.z64.*OK\" build.log || { echo \"the build does not match the ROM\"; tail -5 build.log; exit 1; }
    b=build/us/rev0
    sed \"/\/DISCARD\/ :/i\    .mdebug 0 : { *(.mdebug) }\" linker_scripts/us/rev0/fzerox.ld > \$b/fzerox.mdebug.ld
    cmd=\$(make -n -W linker_scripts/us/rev0/fzerox.ld \$b/fzerox.us.rev0.elf KEEP_MDEBUG=1 | sed -n \"/^mips-linux-gnu-ld /,/-o /p\" | tr -d \"\\\\\n\")
    cmd=\${cmd/-T linker_scripts\/us\/rev0\/fzerox.ld/-T \$b/fzerox.mdebug.ld}
    cmd=\${cmd/-Map \$b\/fzerox.us.rev0.map/-Map \$b/fzerox.us.rev0.mdebug.elf.map}
    cmd=\${cmd/-o \$b\/fzerox.us.rev0.elf/-o \$b/fzerox.us.rev0.mdebug.elf}
    eval \"\$cmd\"
    mips-linux-gnu-objcopy -O binary \$b/fzerox.us.rev0.elf a.bin
    mips-linux-gnu-objcopy -O binary -R .mdebug \$b/fzerox.us.rev0.mdebug.elf b.bin
    cmp -s a.bin b.bin || { echo \"the relinked ELF differs\"; exit 1; }
    rm a.bin b.bin'"

mkdir -p build/decomp
get "$work/build/us/rev0/fzerox.us.rev0.mdebug.elf" build/decomp/
echo "build/decomp/: the decomp's ELF at ${commit:0:12}, matching the ROM; next, tools/gen_syms.sh"
