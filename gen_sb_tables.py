#!/usr/bin/env python3
"""Convert Rust single-byte encoding tables to C static const uint32_t arrays.

Reads all .rs files in the Rust tables directory and produces sb_tables.h
with C arrays suitable for charconv.
"""

import re
import os
import sys

RUST_TABLES_DIR = "/Users/dlr/src/mdfind/encforce/src/encodings/tables"
OUTPUT_FILE = "/Users/dlr/src/mdfind/cenc/sb_tables.h"

def extract_tables(filepath):
    """Extract all table definitions from a Rust source file."""
    tables = []
    with open(filepath, 'r') as f:
        content = f.read()

    # Find all pub const TABLE_NAME: [char; 256] = [ ... ];
    pattern = r'pub\s+const\s+([A-Z_0-9]+)\s*:\s*\[char;\s*256\]\s*=\s*\[(.*?)\];'
    for m in re.finditer(pattern, content, re.DOTALL):
        name = m.group(1)
        body = m.group(2)

        # Remove comments (// ...)
        body_lines = body.split('\n')
        clean_lines = []
        for line in body_lines:
            idx = line.find('//')
            if idx >= 0:
                line = line[:idx]
            clean_lines.append(line)
        body = ' '.join(clean_lines)

        # Parse char literals manually by walking the string
        chars = []
        i = 0
        while i < len(body):
            if body[i] == "'":
                # Start of a char literal - find the codepoint
                if i + 1 < len(body) and body[i+1] == '\\':
                    # Escaped char: '\X' or '\u{XXXX}'
                    if i + 2 < len(body) and body[i+2] == 'u' and i + 3 < len(body) and body[i+3] == '{':
                        # '\u{XXXX}' - Unicode escape
                        end = body.index('}', i+4)
                        hex_str = body[i+4:end]
                        chars.append(int(hex_str, 16))
                        i = end + 2  # skip past }'
                    elif i + 2 < len(body) and body[i+2] == '\\':
                        # '\\' - backslash
                        chars.append(0x5C)
                        i += 4  # skip past '\\'
                    elif i + 2 < len(body) and body[i+2] == "'":
                        # '\'' - escaped single quote
                        chars.append(0x27)
                        i += 4  # skip past '\''
                    elif i + 2 < len(body) and body[i+2] == 'n':
                        chars.append(0x0A)
                        i += 4
                    elif i + 2 < len(body) and body[i+2] == 'r':
                        chars.append(0x0D)
                        i += 4
                    elif i + 2 < len(body) and body[i+2] == 't':
                        chars.append(0x09)
                        i += 4
                    elif i + 2 < len(body) and body[i+2] == '0':
                        chars.append(0x00)
                        i += 4
                    else:
                        raise ValueError(f"Unknown escape at pos {i} in {name}: {body[i:i+10]}")
                elif i + 2 < len(body) and body[i+2] == "'":
                    # 'X' - regular single character
                    chars.append(ord(body[i+1]))
                    i += 3  # skip past 'X'
                else:
                    raise ValueError(f"Malformed char literal at pos {i} in {name}: {body[i:i+10]}")
            else:
                i += 1

        if len(chars) != 256:
            print(f"WARNING: {name} in {filepath} has {len(chars)} entries (expected 256)", file=sys.stderr)
            continue

        # Convert name to lowercase for C
        c_name = name.lower()
        tables.append((c_name, chars))

    return tables

def format_c_table(name, values):
    """Format a single table as a C static const array."""
    lines = []
    lines.append(f"static const uint32_t {name}[256] = {{")
    for row in range(16):
        vals = []
        for col in range(16):
            idx = row * 16 + col
            v = values[idx]
            vals.append(f"0x{v:04X}")
        comment = f"/* 0x{row*16:02X} */"
        lines.append(f"    {', '.join(vals)}, {comment}")
    lines.append("};")
    return '\n'.join(lines)

def main():
    all_tables = []

    # Process each Rust file in order
    rust_files = ['ascii.rs', 'iso8859.rs', 'windows.rs', 'dos.rs',
                  'koi8.rs', 'mac.rs', 'ebcdic.rs', 'legacy.rs']

    for fname in rust_files:
        filepath = os.path.join(RUST_TABLES_DIR, fname)
        if os.path.exists(filepath):
            tables = extract_tables(filepath)
            print(f"{fname}: extracted {len(tables)} tables", file=sys.stderr)
            for name, _ in tables:
                print(f"  {name}", file=sys.stderr)
            all_tables.extend(tables)
        else:
            print(f"WARNING: {filepath} not found", file=sys.stderr)

    print(f"\nTotal: {len(all_tables)} tables", file=sys.stderr)

    # Generate the C header
    with open(OUTPUT_FILE, 'w') as f:
        f.write("/* sb_tables.h — Single-byte encoding tables (auto-generated from Rust source)\n")
        f.write(f" * {len(all_tables)} tables, 256 entries each\n")
        f.write(" * Values are Unicode codepoints. 0xFFFD = unmapped byte.\n")
        f.write(" * Generated by gen_sb_tables.py\n")
        f.write(" */\n\n")
        f.write("#ifndef SB_TABLES_H\n")
        f.write("#define SB_TABLES_H\n\n")
        f.write("#include <stdint.h>\n\n")

        for name, values in all_tables:
            f.write(format_c_table(name, values))
            f.write("\n\n")

        f.write("#endif /* SB_TABLES_H */\n")

    print(f"Wrote {OUTPUT_FILE}", file=sys.stderr)

if __name__ == '__main__':
    main()
