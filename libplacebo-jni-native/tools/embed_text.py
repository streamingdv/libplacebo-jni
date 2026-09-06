#!/usr/bin/env python3
import sys
from pathlib import Path

# Usage:
#   embed_text.py <input_file> <symbol_base> <output_header>
#
# Produces:
#   #pragma once
#   static const char <symbol_base>[] = R"__EMBED__( ... )__EMBED__";
#   static const unsigned int <symbol_base>_len = ...;

def main():
    if len(sys.argv) != 4:
        print("Usage: embed_text.py <input_file> <symbol_base> <output_header>", file=sys.stderr)
        return 2

    in_path = Path(sys.argv[1])
    sym = sys.argv[2]
    out_path = Path(sys.argv[3])

    data = in_path.read_text(encoding="utf-8", errors="ignore")

    delim = "__EMBED__"
    while f"){delim}\"" in data:
        delim += "_X"

    out = []
    out.append("#pragma once\n")
    out.append("// Auto-generated. Do not edit.\n\n")
    out.append(f"static const char {sym}[] = R\"{delim}(\n")
    out.append(data)
    if not data.endswith("\n"):
        out.append("\n")
    out.append(f"){delim}\";\n")
    out.append(f"static const unsigned int {sym}_len = {len(data.encode('utf-8'))};\n")

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("".join(out), encoding="utf-8")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
