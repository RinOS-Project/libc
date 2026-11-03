#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate immutable tables from Python's strict built-in Japanese codecs."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
CP932_OUTPUT = ROOT / "iconv_cp932_table.h"
OUTPUT = ROOT / "iconv_japanese_tables.h"


def decode_single(codec: str, data: bytes) -> int:
    try:
        decoded = data.decode(codec, "strict")
    except UnicodeDecodeError:
        return 0
    if len(decoded) != 1:
        raise ValueError(f"non-scalar {codec} mapping for {data.hex()}")
    scalar = ord(decoded)
    if scalar == 0 or scalar > 0xFFFF:
        raise ValueError(f"unexpected {codec} mapping: {data.hex()} -> {scalar:#x}")
    return scalar


def dense_dbcs(codec: str) -> list[int]:
    leads = list(range(0x81, 0xA0)) + list(range(0xE0, 0xFD))
    trails = list(range(0x40, 0x7F)) + list(range(0x80, 0xFD))
    return [decode_single(codec, bytes((lead, trail)))
            for lead in leads for trail in trails]


def dense_euc_plane(codec: str, plane: int) -> list[int]:
    values: list[int] = []
    prefix = b"\x8f" if plane == 2 else b""
    for row in range(0xA1, 0xFF):
        for cell in range(0xA1, 0xFF):
            values.append(decode_single(codec, prefix + bytes((row, cell))))
    return values


def reverse_table(codec: str) -> list[int]:
    entries: list[int] = []
    for scalar in range(0x80, 0x110000):
        try:
            encoded = chr(scalar).encode(codec, "strict")
        except UnicodeEncodeError:
            continue
        if len(encoded) < 2 or len(encoded) > 3:
            continue
        try:
            decoded = encoded.decode(codec, "strict")
        except UnicodeDecodeError:
            continue
        if len(decoded) != 1 or ord(decoded) != scalar:
            continue
        packed = int.from_bytes(encoded, "big")
        entries.append((scalar << 32) | (len(encoded) << 24) | packed)
    return entries


def emit_array(lines: list[str], c_type: str, name: str,
               values: list[int], width: int = 8) -> None:
    lines.append(f"static const {c_type} {name}[{len(values)}] = {{")
    for offset in range(0, len(values), width):
        chunk = values[offset:offset + width]
        suffix = "u" if c_type != "uint64_t" else "ull"
        digits = 16 if c_type == "uint64_t" else 4
        lines.append("    " + ", ".join(
            f"0x{value:0{digits}x}{suffix}" for value in chunk) + ",")
    lines.append("};")
    lines.append("")


def main() -> None:
    cp932 = dense_dbcs("cp932")
    shift_jis = dense_dbcs("shift_jis")
    euc_plane1 = dense_euc_plane("euc_jp", 1)
    euc_plane2 = dense_euc_plane("euc_jp", 2)
    cp932_encode = reverse_table("cp932")
    shift_jis_encode = reverse_table("shift_jis")
    euc_jp_encode = reverse_table("euc_jp")

    cp932_lines = [
        "/* SPDX-License-Identifier: MIT */",
        "/* Generated with generate_iconv_japanese_tables.py. */",
        "/* Mapping values come from Python's strict built-in Japanese codecs. */",
        "#ifndef RIN_LIBC_ICONV_CP932_TABLE_H",
        "#define RIN_LIBC_ICONV_CP932_TABLE_H",
        "",
        "#include <stdint.h>",
        "",
        "#define RIN_ICONV_JIS_LEAD_COUNT 60u",
        "#define RIN_ICONV_JIS_TRAIL_COUNT 188u",
        "",
    ]
    emit_array(cp932_lines, "uint16_t", "g_rin_iconv_cp932_pairs", cp932)
    cp932_lines.extend(["#endif", ""])
    CP932_OUTPUT.write_text("\n".join(cp932_lines), encoding="ascii",
                            newline="\n")

    lines = [
        "/* SPDX-License-Identifier: MIT */",
        "/* Generated with generate_iconv_japanese_tables.py. */",
        "/* Mapping values come from Python's strict built-in Japanese codecs. */",
        "#ifndef RIN_LIBC_ICONV_JAPANESE_TABLES_H",
        "#define RIN_LIBC_ICONV_JAPANESE_TABLES_H",
        "",
        "#include <stdint.h>",
        "",
        "#define RIN_ICONV_JIS_LEAD_COUNT 60u",
        "#define RIN_ICONV_JIS_TRAIL_COUNT 188u",
        "#define RIN_ICONV_EUC_JP_CELLS 8836u",
        f"#define RIN_ICONV_CP932_ENCODE_COUNT {len(cp932_encode)}u",
        f"#define RIN_ICONV_SHIFT_JIS_ENCODE_COUNT {len(shift_jis_encode)}u",
        f"#define RIN_ICONV_EUC_JP_ENCODE_COUNT {len(euc_jp_encode)}u",
        "",
    ]
    emit_array(lines, "uint16_t", "g_rin_iconv_shift_jis_pairs", shift_jis)
    emit_array(lines, "uint16_t", "g_rin_iconv_euc_jp_plane1", euc_plane1)
    emit_array(lines, "uint16_t", "g_rin_iconv_euc_jp_plane2", euc_plane2)
    emit_array(lines, "uint64_t", "g_rin_iconv_cp932_encode",
               cp932_encode, width=4)
    emit_array(lines, "uint64_t", "g_rin_iconv_shift_jis_encode",
               shift_jis_encode, width=4)
    emit_array(lines, "uint64_t", "g_rin_iconv_euc_jp_encode",
               euc_jp_encode, width=4)
    lines.extend(["#endif", ""])
    OUTPUT.write_text("\n".join(lines), encoding="ascii", newline="\n")


if __name__ == "__main__":
    main()
