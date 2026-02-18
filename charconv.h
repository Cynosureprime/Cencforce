/*
 * charconv.h -- Reusable character encoding conversion library
 *
 * Provides decode (encoding bytes -> UTF-8) and encode (UTF-8 -> encoding bytes)
 * for single-byte, UTF, and CJK encodings with configurable error strategies.
 *
 * Thread-safe: all tables are read-only after charconv_init().
 */

#ifndef CHARCONV_H
#define CHARCONV_H

#include <stdint.h>

/* ===== Encoding types ===== */
#define ENC_TYPE_SINGLE_BYTE  0
#define ENC_TYPE_UTF8         1
#define ENC_TYPE_UTF7         2
#define ENC_TYPE_UTF16        3   /* BOM detection */
#define ENC_TYPE_UTF16BE      4
#define ENC_TYPE_UTF16LE      5
#define ENC_TYPE_UTF32        6   /* BOM detection */
#define ENC_TYPE_UTF32BE      7
#define ENC_TYPE_UTF32LE      8
#define ENC_TYPE_CESU8        9
#define ENC_TYPE_SHIFT_JIS    10
#define ENC_TYPE_EUC_JP       11
#define ENC_TYPE_ISO2022JP    12
#define ENC_TYPE_GBK          13
#define ENC_TYPE_GB18030      14
#define ENC_TYPE_BIG5         15
#define ENC_TYPE_EUC_KR       16

/* ===== Decode error strategies ===== */
#define DS_STRICT                    0
#define DS_REPLACEMENT_FFFD          1
#define DS_REPLACEMENT_QUESTION      2
#define DS_REPLACEMENT_SUB           3
#define DS_SKIP                      4
#define DS_LATIN1_FALLBACK           5
#define DS_CP1252_FALLBACK           6
#define DS_HEX_ESCAPE_X              7
#define DS_HEX_ESCAPE_PERCENT        8
#define DS_HEX_ESCAPE_ANGLE          9
#define DS_HEX_ESCAPE_0X            10
#define DS_HEX_ESCAPE_BRACKET       11
#define DS_OCTAL_ESCAPE             12
#define DS_CARET_NOTATION           13
#define DS_UNICODE_ESCAPE_U         14
#define DS_BYTE_VALUE_DECIMAL       15
#define DS_BYTE_VALUE_BACKSLASH_DEC 16
#define DS_DOUBLE_PERCENT           17
#define DS_COUNT                    18

/* ===== Encode error strategies ===== */
#define ES_STRICT                    0
#define ES_REPLACEMENT_QUESTION      1
#define ES_REPLACEMENT_SUB           2
#define ES_REPLACEMENT_SPACE         3
#define ES_REPLACEMENT_ZWSP          4
#define ES_REPLACEMENT_UNDERSCORE    5
#define ES_SKIP                      6
#define ES_HTML_DECIMAL              7
#define ES_HTML_HEX                  8
#define ES_HTML_NAMED                9
#define ES_XML_NUMERIC              10
#define ES_URL_ENCODING             11
#define ES_DOUBLE_URL_ENCODING      12
#define ES_HEX_ESCAPE_X             13
#define ES_UNICODE_ESCAPE_U4        14
#define ES_UNICODE_ESCAPE_U8        15
#define ES_UNICODE_ESCAPE_XBRACE    16
#define ES_UNICODE_ESCAPE_UPLUS     17
#define ES_UNICODE_ESCAPE_UBRACE    18
#define ES_PYTHON_NAMED_ESCAPE      19
#define ES_JAVA_SURROGATE_PAIRS     20
#define ES_CSS_ESCAPE               21
#define ES_JSON_ESCAPE              22
#define ES_PUNYCODE                 23
#define ES_TRANSLITERATION          24
#define ES_BASE64_INLINE            25
#define ES_QUOTED_PRINTABLE         26
#define ES_NCR_DECIMAL              27
#define ES_COUNT                    28

/* ===== Strategy name arrays ===== */
extern const char *charconv_decode_strategy_names[DS_COUNT];
extern const char *charconv_encode_strategy_names[ES_COUNT];

/* ===== Reverse map entry for single-byte encode ===== */
struct sb_reverse_entry {
    uint32_t codepoint;
    uint8_t byte;
};

/* ===== Encoding descriptor ===== */
struct CharEncoding {
    const char *name;
    int type;                              /* ENC_TYPE_* */
    const uint32_t *to_unicode;            /* [256] for single-byte, NULL otherwise */
    struct sb_reverse_entry *reverse_map;   /* Built at init for single-byte encode */
    int reverse_map_size;                  /* Number of entries in reverse map */
    int is_ascii_compatible;
};

/* ===== Public API ===== */

/*
 * Decode: encoding bytes -> UTF-8
 * Returns output length in bytes, or -1 on failure (strict mode error, buffer full).
 */
int charconv_decode(const struct CharEncoding *enc,
    const unsigned char *in, int inlen,
    unsigned char *out, int outsize,
    int strategy, int *had_errors);

/*
 * Encode: UTF-8 -> encoding bytes
 * Returns output length in bytes, or -1 on failure.
 */
int charconv_encode(const struct CharEncoding *enc,
    const unsigned char *in, int inlen,
    unsigned char *out, int outsize,
    int strategy, int *had_errors);

/*
 * Build reverse maps for all single-byte encodings (call once at startup).
 * encodings: array of CharEncoding structs
 * count: number of encodings
 */
void charconv_init_reverse_maps(struct CharEncoding *encodings, int count);

/*
 * Check if byte sequence is valid UTF-8.
 * Returns 1 if valid, 0 if not.
 */
int charconv_is_valid_utf8(const unsigned char *data, int len);

/* ===== HTML entities (for ES_HTML_NAMED) ===== */
struct html_entity {
    const char *name;
    uint32_t codepoint;
};
extern const struct html_entity charconv_html_entities[];

/* ===== Transliteration (for ES_TRANSLITERATION) ===== */
struct translit_entry {
    uint32_t codepoint;
    const char *ascii;
};
extern const struct translit_entry charconv_translit_table[];

/* ===== UTF-8 utilities (used by encforce.c) ===== */
int charconv_utf8_encode(uint32_t cp, unsigned char *buf);
uint32_t charconv_utf8_decode(const unsigned char *s, int len, int *consumed);

#endif /* CHARCONV_H */
