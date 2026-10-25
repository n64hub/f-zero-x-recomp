"""Writes syms/fzerox.us.syms.toml and syms/fzerox.us.datasyms.toml from the decomp's US ELF with N64Recomp's own
context dump (--dump-context), the way the MarioKart64RecompSyms and Starfox64RecompSyms repos were made.

The ELF is tools/build_decomp.sh's fzerox.us.rev0.mdebug.elf: the decomp's own link again, with each object's .mdebug
kept (fzerox.ld discards it), so N64Recomp finds the C files' static functions there (use_mdebug): IDO puts no symbol
for a static function in an object's symbol table, and libultra has 11 (__osLeoResume, _Putfld, ...). The decomp's
assembly is splat's, which sizes every function, and IDO at -O2 ends each static function's .mdebug record right
after it, so every function comes out with its size.

N64Recomp reads each section's VRAM from the ELF's section headers and its ROM offset from the load address
fzerox.ld gives it. That is the decomp's uncompressed ROM, whose ROM the decomp's tools/compress.py then makes by
MIO0-compressing the course textures and models and moving everything after them up. The code before them is where
the ROM has it; a code section after them (ovl_i11, the 64DD's boot) is found in the ROM by its bytes, and its ROM
offset written from there.

Usage: py -3 -I tools/gen_syms.py <N64Recomp> <elf> <rom>
"""
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
# The decomp's entrypoint (asm/us/rev0/entrypoint.s), where the boot code jumps after it copies 1 MB from ROM 0x1000.
ENTRYPOINT = 0x80067000
# libleo's font lookups for the 64DD's error screens (lib/getkadr and lib/getaadr in the decomp's overlays.yaml), which
# the decomp keeps as raw bytes, each function followed by its table: the ELF has them as unsized labels. Without a
# function of that name N64Recomp makes one of each jal's target and runs on into the table. Sized here to where the
# yaml's commented-out split puts the data.
EXTRA_FUNCTIONS = {
    ".leo": [("LeoGetKAdr", 0x80411760, 0xF0), ("LeoGetAAdr", 0x804122A0, 0x7C)],
}


def elf_sections(path):
    """Each section's bytes in the ELF, by name."""
    data = Path(path).read_bytes()
    if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 2:
        sys.exit(f"{path}: not a 32-bit big-endian ELF")
    e_shoff, = struct.unpack_from(">I", data, 0x20)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from(">HHH", data, 0x2E)
    headers = [struct.unpack_from(">IIIIIIIIII", data, e_shoff + i * e_shentsize) for i in range(e_shnum)]
    names = headers[e_shstrndx][4]
    return {data[names + h[0]:data.index(b"\0", names + h[0])].decode(): data[h[4]:h[4] + h[5]] for h in headers}


def fix_rom_offsets(dump, sections, rom):
    """Each [[section]]'s rom where the ROM holds its bytes: the dump's own offset when they are there, else the one
    place in the ROM that holds its first 0x100 bytes. compress.py also rewrites the ROM addresses the code loads of
    what it moved (main's, ovl_i2's and ovl_i3's), so those sections differ from the ELF's in a few words, and are
    found where the ELF has them."""
    def fixed(m):
        name, offset = m.group(1), int(m.group(2), 16)
        code = sections[name]
        if rom[offset:offset + len(code)] == code:
            return m.group(0)
        head = code[:0x100]
        found = rom.find(head)
        if found < 0 or rom.find(head, found + 1) >= 0:
            sys.exit(f"{name}: its first 0x100 bytes are in the ROM {'more than once' if found >= 0 else 'nowhere'}")
        if found != offset:
            print(f"{name}: ROM 0x{found:X}, not the ELF's 0x{offset:X}")
        return m.group(0).replace(m.group(2), f"0x{found:08X}")

    return re.sub(r'name = "([^"]+)"\nrom = (0x[0-9A-F]+)', fixed, dump)


def add_functions(dump):
    """EXTRA_FUNCTIONS, at the start of their sections' function lists."""
    for section, funcs in EXTRA_FUNCTIONS.items():
        entries = "".join(f'    {{ name = "{n}", vram = 0x{v:08X}, size = 0x{s:X} }},\n' for n, v, s in funcs)
        dump, count = re.subn(rf'(name = "{re.escape(section)}"\n(?:.*\n)*?functions = \[\n)', rf"\g<1>{entries}", dump,
                              count=1)
        if not count:
            sys.exit(f"no section {section} in the dump")
    return dump


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__.strip().splitlines()[-1])
    n64recomp, elf, rom = (Path(a).resolve() for a in sys.argv[1:])
    with tempfile.TemporaryDirectory() as tmp:
        config = Path(tmp) / "context.toml"
        config.write_text(
            "[input]\n"
            f"entrypoint = 0x{ENTRYPOINT:08X}\n"
            f'elf_path = "{elf.as_posix()}"\n'
            'output_func_path = "out"\n'
            # The decomp's bss sections are named <segment>_bss (splat's fzerox.ld).
            'bss_section_suffix = "_bss"\n'
            "use_mdebug = true\n")
        run = subprocess.run([str(n64recomp), config.name, "--dump-context"], cwd=tmp, capture_output=True, text=True)
        if run.returncode != 0:
            sys.exit((run.stdout + run.stderr)[-3000:])
        dump = fix_rom_offsets((Path(tmp) / "dump.toml").read_text(), elf_sections(elf), rom.read_bytes())
        dump = add_functions(dump)
        out = ROOT / "syms"
        out.mkdir(exist_ok=True)
        (out / "fzerox.us.syms.toml").write_text(dump, newline="\n")
        (out / "fzerox.us.datasyms.toml").write_text((Path(tmp) / "data_dump.toml").read_text(), newline="\n")
    print(f"{dump.count('[[section]]')} sections, {dump.count('{ name = ')} functions; "
          "wrote syms/fzerox.us.syms.toml and syms/fzerox.us.datasyms.toml")


if __name__ == "__main__":
    main()
