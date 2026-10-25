#!/usr/bin/env bash
# Writes syms/fzerox.us.syms.toml and syms/fzerox.us.datasyms.toml from the ELF tools/build_decomp.sh brought back
# (build/decomp/fzerox.us.rev0.mdebug.elf) and your ROM: builds the pinned N64Recomp (tools/build_n64recomp.sh) and
# runs tools/gen_syms.py, which says how.
#
# Usage: tools/gen_syms.sh [cmake arguments for tools/build_n64recomp.sh]
set -euo pipefail
cd "$(dirname "$0")/.."
elf=build/decomp/fzerox.us.rev0.mdebug.elf
rom=roms/fzerox.us.z64
[[ -f "$elf" ]] || { echo "no $elf: run tools/build_decomp.sh first" >&2; exit 1; }
[[ -f "$rom" ]] || { echo "no $rom: put your F-Zero X (USA) .z64 there" >&2; exit 1; }
bash tools/build_n64recomp.sh "$@"
if command -v py > /dev/null; then python=(py -3); else python=(python3); fi
"${python[@]}" -I tools/gen_syms.py build/n64recomp/N64Recomp "$elf" "$rom"
