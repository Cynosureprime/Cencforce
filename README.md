# encforce

Text encoding forensics tool. Discovers how text was corrupted by
exhaustively trying encoding/decoding transformations across 106 character
encodings.

Given an input string, encforce applies every combination of encoding,
decoding strategy, and error-handling method to produce all possible
interpretations. This is useful for diagnosing mojibake, recovering
garbled text, and understanding encoding-layer corruption in password
lists and text corpora.

## Building

```
make
```

No external dependencies. All 106 encoding tables are compiled in.

## Usage

```
encforce [OPTIONS] [INPUT...]
```

Input strings are provided as command-line arguments or read from a file
with `-f`. Inputs containing non-UTF-8 bytes can be supplied in `$HEX[...]`
notation (e.g., `$HEX[636166e9]` for the bytes `caf\xe9`).

### Options

| Short | Long | Argument | Default | Description |
|-------|------|----------|---------|-------------|
| `-f` | `--file` | FILE | stdin | Read inputs from file (one per line) |
| `-m` | `--mode` | MODE | `both` | Operation mode (see below) |
| `-e` | `--encoding` | ENC | all | Only use these encodings (repeatable) |
| `-x` | `--exclude` | ENC | none | Exclude these encodings (repeatable) |
| `-j` | `--jobs` | N | CPU count | Worker threads (1-64) |
| `-F` | `--format` | FMT | `lines` | Output format (see below) |
| | `--depth` | N | 1 | Max transcode chain depth |
| | `--raw` | | off | Disable `$HEX[]` input parsing and output encoding |
| | `--unique` | | on | Deduplicate output values per input |
| | `--no-unique` | | | Disable deduplication |
| | `--no-errors` | | off | Hide results that had decoding/encoding errors |
| `-v` | `--verbose` | | off | Show detailed output |
| `-s` | `--suggest` | | off | Show mojibake suggestions |
| `-h` | `--help` | | | Show help |
| `-V` | `--version` | | | Show version |

### Modes (`-m`)

| Mode | Description |
|------|-------------|
| `decode` | Treat input as raw bytes. Decode through each encoding to produce UTF-8 text. |
| `encode` | Treat input as UTF-8 text. Encode into each encoding's byte representation. |
| `both` | Run both decode and encode. (Default) |
| `transcode` | Decode through one encoding, then re-encode through another. |
| `all` | Run decode, encode, and transcode. |

### Output Formats (`-F`)

| Format | Description |
|--------|-------------|
| `lines` | One result per line. Non-UTF-8 output wrapped in `$HEX[...]`. |
| `json` | JSON objects per input line with `input_hex`, `results` array. |
| `tsv` | Tab-separated: input, input\_hex, operation, encoding, target, strategy, output, output\_hex. |

## Examples

Decode a mojibake string:
```
$ echo "cafÃ©" | encforce -m decode -e utf-8,windows-1252
```

Encode text into all single-byte encodings:
```
$ echo "café" | encforce -m encode
```

Find all byte representations using specific encodings:
```
$ encforce -m both -e iso-8859-1,shift_jis,euc-jp "café"
```

Read from a file with 8 threads, JSON output:
```
$ encforce -f wordlist.txt -j 8 -F json -m decode
```

## Encodings (106)

### Single-Byte Encodings (89)

#### ASCII

| Name | Aliases |
|------|---------|
| ASCII | US-ASCII, ANSI_X3.4-1968, 646 |

#### ISO 8859

| Name | Aliases | Script |
|------|---------|--------|
| ISO-8859-1 | Latin-1, latin1, IBM819 | Western European |
| ISO-8859-2 | Latin-2, latin2 | Central European |
| ISO-8859-3 | Latin-3, latin3 | South European |
| ISO-8859-4 | Latin-4, latin4 | North European |
| ISO-8859-5 | Cyrillic | Cyrillic |
| ISO-8859-6 | Arabic, ASMO-708 | Arabic |
| ISO-8859-7 | Greek, ECMA-118 | Greek |
| ISO-8859-8 | Hebrew | Hebrew |
| ISO-8859-9 | Latin-5, Turkish | Turkish |
| ISO-8859-10 | Latin-6, Nordic | Nordic |
| ISO-8859-11 | Thai, TIS-620, CP874 | Thai |
| ISO-8859-13 | Latin-7, Baltic | Baltic |
| ISO-8859-14 | Latin-8, Celtic | Celtic |
| ISO-8859-15 | Latin-9 | Western European (with euro) |
| ISO-8859-16 | Latin-10, Romanian | South-Eastern European |

#### Windows Code Pages

| Name | Aliases | Script |
|------|---------|--------|
| Windows-1250 | CP1250 | Central European |
| Windows-1251 | CP1251 | Cyrillic |
| Windows-1252 | CP1252 | Western European |
| Windows-1253 | CP1253 | Greek |
| Windows-1254 | CP1254 | Turkish |
| Windows-1255 | CP1255 | Hebrew |
| Windows-1256 | CP1256 | Arabic |
| Windows-1257 | CP1257 | Baltic |
| Windows-1258 | CP1258 | Vietnamese |

#### DOS Code Pages

| Name | Aliases | Script |
|------|---------|--------|
| CP437 | IBM437, DOS-US | Original PC |
| CP850 | IBM850, DOS-Latin-1 | Multilingual Latin |
| CP852 | IBM852, DOS-Latin-2 | Central European |
| CP737 | IBM737, DOS-Greek | Greek |
| CP855 | IBM855 | Cyrillic |
| CP857 | IBM857, DOS-Turkish | Turkish |
| CP858 | IBM858 | Multilingual Latin + euro |
| CP860 | IBM860, DOS-Portuguese | Portuguese |
| CP861 | IBM861, DOS-Icelandic | Icelandic |
| CP862 | IBM862, DOS-Hebrew | Hebrew |
| CP863 | IBM863, DOS-Canadian-French | Canadian French |
| CP864 | IBM864, DOS-Arabic | Arabic |
| CP865 | IBM865, DOS-Nordic | Nordic |
| CP866 | IBM866, DOS-Cyrillic | Cyrillic |
| CP869 | IBM869, DOS-Greek-2 | Greek |

#### DOS Code Pages — DOC Graphical Variants

The DOC variants replace box-drawing characters (0x00-0x1F) with
control characters, matching how DOS word processors stored text.

| Name | Aliases |
|------|---------|
| CP437-DOC | IBM437-DOC, DOS-US-DOC |
| CP850-DOC | IBM850-DOC, DOS-Latin-1-DOC |
| CP852-DOC | IBM852-DOC, DOS-Latin-2-DOC |
| CP737-DOC | IBM737-DOC, DOS-Greek-DOC |
| CP855-DOC | IBM855-DOC |
| CP857-DOC | IBM857-DOC, DOS-Turkish-DOC |
| CP858-DOC | IBM858-DOC |
| CP860-DOC | IBM860-DOC, DOS-Portuguese-DOC |
| CP861-DOC | IBM861-DOC, DOS-Icelandic-DOC |
| CP862-DOC | IBM862-DOC, DOS-Hebrew-DOC |
| CP863-DOC | IBM863-DOC, DOS-Canadian-French-DOC |
| CP864-DOC | IBM864-DOC, DOS-Arabic-DOC |
| CP865-DOC | IBM865-DOC, DOS-Nordic-DOC |
| CP869-DOC | IBM869-DOC, DOS-Greek-2-DOC |

#### KOI8

| Name | Aliases | Script |
|------|---------|--------|
| KOI8-R | koi8r | Russian |
| KOI8-U | koi8u | Ukrainian |

#### Mac Encodings

| Name | Aliases | Script |
|------|---------|--------|
| MacRoman | macintosh, x-mac-roman | Western European |
| MacCyrillic | x-mac-cyrillic | Cyrillic |
| MacGreek | x-mac-greek | Greek |
| MacTurkish | x-mac-turkish | Turkish |
| MacCentralEurope | x-mac-ce | Central European |
| MacIcelandic | x-mac-icelandic | Icelandic |
| MacCroatian | x-mac-croatian | Croatian |
| MacRomanian | x-mac-romanian | Romanian |
| MacArabic | x-mac-arabic | Arabic |
| MacHebrew | x-mac-hebrew | Hebrew |
| MacThai | x-mac-thai | Thai |

#### EBCDIC

| Name | Aliases | Region |
|------|---------|--------|
| CP037 | IBM037, ebcdic-cp-us | US/Canada |
| CP500 | IBM500, ebcdic-international | International |
| CP875 | IBM875, ebcdic-greek | Greek |
| CP1026 | IBM1026, ebcdic-cp-tr | Turkish |
| CP1140 | IBM1140 | US + euro |
| CP1141 | IBM1141 | Germany + euro |
| CP1142 | IBM1142 | Denmark/Norway + euro |
| CP1143 | IBM1143 | Finland/Sweden + euro |
| CP1144 | IBM1144 | Italy + euro |
| CP1145 | IBM1145 | Spain + euro |

#### Legacy

| Name | Aliases | Notes |
|------|---------|-------|
| HP-Roman8 | roman8, r8 | HP terminals |
| DEC-MCS | dec-mcs, dec | DEC terminals |
| JIS_X0201 | x0201 | Japanese half-width katakana |
| KZ-1048 | STRK1048-2002, RK1048 | Kazakh |
| GSM-03.38 | GSM, gsm7 | SMS/mobile |
| VISCII | viscii1.1-1 | Vietnamese |
| ATASCII | atari-ascii, atari | Atari 8-bit |
| PETSCII | commodore, c64 | Commodore 64 |
| Adobe-Standard-Encoding | adobe-standard | PostScript |
| Adobe-Symbol-Encoding | adobe-symbol, symbol | PostScript symbols |
| T.61-8bit | T.61, t61 | Teletex |

### Unicode Encodings (9)

| Name | Aliases | Notes |
|------|---------|-------|
| UTF-8 | utf8 | |
| UTF-7 | utf7 | Modified base64 |
| UTF-16 | utf16 | BOM auto-detection, defaults to BE |
| UTF-16BE | utf16be | Big-endian, no BOM |
| UTF-16LE | utf16le | Little-endian, no BOM |
| UTF-32 | utf32 | BOM auto-detection, defaults to BE |
| UTF-32BE | utf32be | Big-endian, no BOM |
| UTF-32LE | utf32le | Little-endian, no BOM |
| CESU-8 | cesu8 | Supplementary chars as surrogate pairs |

### CJK Multi-Byte Encodings (8)

| Name | Aliases | Notes |
|------|---------|-------|
| Shift_JIS | sjis, CP932, MS_Kanji | Japanese (Windows) |
| EUC-JP | eucjp | Japanese (Unix) |
| ISO-2022-JP | jis | Japanese (email) |
| GBK | CP936, windows-936 | Chinese simplified (Windows) |
| GB2312 | EUC-CN | Chinese simplified (Unix) |
| GB18030 | gb18030-2000 | Chinese (national standard, full Unicode) |
| Big5 | CP950, Big5-HKSCS | Chinese traditional |
| EUC-KR | KS_C_5601-1987, korean | Korean |

## Strategies

When a byte or codepoint cannot be mapped during decode or encode,
a **strategy** determines what to produce instead. By default, all
strategies are tried for each encoding, producing one output line per
strategy that generates a distinct result.

Use `-e` with comma-separated strategy names to restrict which strategies
are applied.

### Decode Strategies (18)

Applied to unmappable bytes during decoding (bytes to UTF-8).

| Strategy | Output for byte 0xA9 | Description |
|----------|---------------------|-------------|
| `strict` | *(error)* | Fail; return no result |
| `replacement_fffd` | `\xEF\xBF\xBD` | Unicode replacement character U+FFFD |
| `replacement_question` | `?` | ASCII question mark |
| `replacement_sub` | `\x1A` | ASCII SUB control character |
| `skip` | *(nothing)* | Silently discard the byte |
| `latin1_fallback` | `\xC2\xA9` | Treat byte as ISO-8859-1 codepoint |
| `cp1252_fallback` | `\xC2\xA9` | Map byte through Windows-1252 table |
| `hex_escape_x` | `\xa9` | Backslash-x hex escape |
| `hex_escape_percent` | `%A9` | URL-style percent escape |
| `hex_escape_angle` | `<A9>` | Angle-bracket hex |
| `hex_escape_0x` | `0xA9` | 0x-prefixed hex |
| `hex_escape_bracket` | `[A9]` | Square-bracket hex |
| `octal_escape` | `\251` | Backslash octal |
| `caret_notation` | `\xa9` | Caret for control chars, hex fallback |
| `unicode_escape_u` | `\u00A9` | Unicode 4-digit escape |
| `byte_value_decimal` | `{169}` | Decimal in braces |
| `byte_value_backslash_decimal` | `\169` | Backslash decimal |
| `double_percent` | `%%A9` | Double-percent hex |

### Encode Strategies (28)

Applied to unmappable codepoints during encoding (UTF-8 to bytes).

| Strategy | Output for U+00E9 (e-acute) | Description |
|----------|----------------------------|-------------|
| `strict` | *(error)* | Fail; return no result |
| `replacement_question` | `?` | ASCII question mark |
| `replacement_sub` | `\x1A` | SUB control character |
| `replacement_space` | ` ` | Space |
| `replacement_zwsp` | *(nothing)* | Zero-width space (effectively skip) |
| `replacement_underscore` | `_` | Underscore |
| `skip` | *(nothing)* | Silently discard the codepoint |
| `html_decimal` | `&#233;` | HTML decimal entity |
| `html_hex` | `&#xE9;` | HTML hex entity |
| `html_named` | `&eacute;` | HTML named entity (decimal fallback) |
| `xml_numeric` | `&#233;` | XML numeric character reference |
| `url_encoding` | `%C3%A9` | Percent-encode UTF-8 bytes |
| `double_url_encoding` | `%25C3%25A9` | Double percent-encode |
| `hex_escape_x` | `\xc3\xa9` | Backslash-x per UTF-8 byte |
| `unicode_escape_u4` | `\u00E9` | 16-bit Unicode escape |
| `unicode_escape_u8` | `\U000000E9` | 32-bit Unicode escape |
| `unicode_escape_x_brace` | `\x{E9}` | Perl/Rust style |
| `unicode_escape_u_plus` | `U+00E9` | Unicode notation |
| `unicode_escape_u_brace` | `\u{E9}` | ES6/Rust style |
| `python_named_escape` | `\N{U+00E9}` | Python Unicode escape |
| `java_surrogate_pairs` | `\u00E9` | Java-style (surrogate pairs for >U+FFFF) |
| `css_escape` | `\0000E9` | CSS hex escape |
| `json_escape` | `\u00E9` | JSON Unicode escape |
| `punycode` | `xn--9ca` | RFC 3492 Punycode with IDNA prefix |
| `transliteration` | `e` | Map to ASCII equivalent |
| `base64_inline` | `[base64:w6k=]` | Base64-encode UTF-8 bytes |
| `quoted_printable` | `=C3=A9` | Quoted-Printable encoding |
| `ncr_decimal` | `&#233;` | Numeric character reference |

## Threading

encforce uses a thread pool (`yarn.c`) for parallel processing. Each
worker thread pulls input lines from a shared queue and processes them
independently. The default thread count matches the CPU count (capped
at 64). Override with `-j`.

Output order is not guaranteed when using multiple threads. Each thread
accumulates results in a 2 MB buffer and flushes under a mutex.

## Limits

- Maximum input line length: 256 KB (lines at or above this are skipped)
- Per-result output buffer: 3.25 MB (13x max input, covers worst-case expansion)
- Per-thread output buffer: 2 MB (auto-flushes when full)
- Deduplication hash table: 8192 slots per input line (FNV-1a, open addressing)
