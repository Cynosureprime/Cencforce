/*
 * charconv.c -- Character encoding conversion library
 *
 * Implements decode (encoding bytes -> UTF-8) and encode (UTF-8 -> encoding bytes)
 * for single-byte, UTF, and CJK encodings with configurable error strategies.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "charconv.h"
#include "sb_tables.h"
#include "cjk_data.h"

/* ===== Strategy name arrays ===== */
const char *charconv_decode_strategy_names[DS_COUNT] = {
    "strict", "replacement_fffd", "replacement_question", "replacement_sub",
    "skip", "latin1_fallback", "cp1252_fallback", "hex_escape_x",
    "hex_escape_percent", "hex_escape_angle", "hex_escape_0x",
    "hex_escape_bracket", "octal_escape", "caret_notation",
    "unicode_escape_u", "byte_value_decimal", "byte_value_backslash_decimal",
    "double_percent"
};

const char *charconv_encode_strategy_names[ES_COUNT] = {
    "strict", "replacement_question", "replacement_sub", "replacement_space",
    "replacement_zwsp", "replacement_underscore", "skip",
    "html_decimal", "html_hex", "html_named", "xml_numeric",
    "url_encoding", "double_url_encoding", "hex_escape_x",
    "unicode_escape_u4", "unicode_escape_u8", "unicode_escape_x_brace",
    "unicode_escape_u_plus", "unicode_escape_u_brace",
    "python_named_escape", "java_surrogate_pairs",
    "css_escape", "json_escape", "punycode",
    "transliteration", "base64_inline", "quoted_printable", "ncr_decimal"
};

/* ===== HTML named entities ===== */
const struct html_entity charconv_html_entities[] = {
    {"quot", 0x0022}, {"amp", 0x0026}, {"apos", 0x0027},
    {"lt", 0x003C}, {"gt", 0x003E},
    {"nbsp", 0x00A0}, {"iexcl", 0x00A1}, {"cent", 0x00A2},
    {"pound", 0x00A3}, {"curren", 0x00A4}, {"yen", 0x00A5},
    {"brvbar", 0x00A6}, {"sect", 0x00A7}, {"uml", 0x00A8},
    {"copy", 0x00A9}, {"ordf", 0x00AA}, {"laquo", 0x00AB},
    {"not", 0x00AC}, {"shy", 0x00AD}, {"reg", 0x00AE},
    {"macr", 0x00AF}, {"deg", 0x00B0}, {"plusmn", 0x00B1},
    {"sup2", 0x00B2}, {"sup3", 0x00B3}, {"acute", 0x00B4},
    {"micro", 0x00B5}, {"para", 0x00B6}, {"middot", 0x00B7},
    {"cedil", 0x00B8}, {"sup1", 0x00B9}, {"ordm", 0x00BA},
    {"raquo", 0x00BB}, {"frac14", 0x00BC}, {"frac12", 0x00BD},
    {"frac34", 0x00BE}, {"iquest", 0x00BF},
    {"Agrave", 0x00C0}, {"Aacute", 0x00C1}, {"Acirc", 0x00C2},
    {"Atilde", 0x00C3}, {"Auml", 0x00C4}, {"Aring", 0x00C5},
    {"AElig", 0x00C6}, {"Ccedil", 0x00C7}, {"Egrave", 0x00C8},
    {"Eacute", 0x00C9}, {"Ecirc", 0x00CA}, {"Euml", 0x00CB},
    {"Igrave", 0x00CC}, {"Iacute", 0x00CD}, {"Icirc", 0x00CE},
    {"Iuml", 0x00CF}, {"ETH", 0x00D0}, {"Ntilde", 0x00D1},
    {"Ograve", 0x00D2}, {"Oacute", 0x00D3}, {"Ocirc", 0x00D4},
    {"Otilde", 0x00D5}, {"Ouml", 0x00D6}, {"times", 0x00D7},
    {"Oslash", 0x00D8}, {"Ugrave", 0x00D9}, {"Uacute", 0x00DA},
    {"Ucirc", 0x00DB}, {"Uuml", 0x00DC}, {"Yacute", 0x00DD},
    {"THORN", 0x00DE}, {"szlig", 0x00DF},
    {"agrave", 0x00E0}, {"aacute", 0x00E1}, {"acirc", 0x00E2},
    {"atilde", 0x00E3}, {"auml", 0x00E4}, {"aring", 0x00E5},
    {"aelig", 0x00E6}, {"ccedil", 0x00E7}, {"egrave", 0x00E8},
    {"eacute", 0x00E9}, {"ecirc", 0x00EA}, {"euml", 0x00EB},
    {"igrave", 0x00EC}, {"iacute", 0x00ED}, {"icirc", 0x00EE},
    {"iuml", 0x00EF}, {"eth", 0x00F0}, {"ntilde", 0x00F1},
    {"ograve", 0x00F2}, {"oacute", 0x00F3}, {"ocirc", 0x00F4},
    {"otilde", 0x00F5}, {"ouml", 0x00F6}, {"divide", 0x00F7},
    {"oslash", 0x00F8}, {"ugrave", 0x00F9}, {"uacute", 0x00FA},
    {"ucirc", 0x00FB}, {"uuml", 0x00FC}, {"yacute", 0x00FD},
    {"thorn", 0x00FE}, {"yuml", 0x00FF},
    {"OElig", 0x0152}, {"oelig", 0x0153}, {"Scaron", 0x0160},
    {"scaron", 0x0161}, {"Yuml", 0x0178}, {"fnof", 0x0192},
    {"circ", 0x02C6}, {"tilde", 0x02DC},
    {"Alpha", 0x0391}, {"Beta", 0x0392}, {"Gamma", 0x0393},
    {"Delta", 0x0394}, {"Epsilon", 0x0395}, {"Zeta", 0x0396},
    {"Eta", 0x0397}, {"Theta", 0x0398}, {"Iota", 0x0399},
    {"Kappa", 0x039A}, {"Lambda", 0x039B}, {"Mu", 0x039C},
    {"Nu", 0x039D}, {"Xi", 0x039E}, {"Omicron", 0x039F},
    {"Pi", 0x03A0}, {"Rho", 0x03A1}, {"Sigma", 0x03A3},
    {"Tau", 0x03A4}, {"Upsilon", 0x03A5}, {"Phi", 0x03A6},
    {"Chi", 0x03A7}, {"Psi", 0x03A8}, {"Omega", 0x03A9},
    {"alpha", 0x03B1}, {"beta", 0x03B2}, {"gamma", 0x03B3},
    {"delta", 0x03B4}, {"epsilon", 0x03B5}, {"zeta", 0x03B6},
    {"eta", 0x03B7}, {"theta", 0x03B8}, {"iota", 0x03B9},
    {"kappa", 0x03BA}, {"lambda", 0x03BB}, {"mu", 0x03BC},
    {"nu", 0x03BD}, {"xi", 0x03BE}, {"omicron", 0x03BF},
    {"pi", 0x03C0}, {"rho", 0x03C1}, {"sigmaf", 0x03C2},
    {"sigma", 0x03C3}, {"tau", 0x03C4}, {"upsilon", 0x03C5},
    {"phi", 0x03C6}, {"chi", 0x03C7}, {"psi", 0x03C8},
    {"omega", 0x03C9}, {"thetasym", 0x03D1}, {"upsih", 0x03D2},
    {"piv", 0x03D6},
    {"ensp", 0x2002}, {"emsp", 0x2003}, {"thinsp", 0x2009},
    {"zwnj", 0x200C}, {"zwj", 0x200D}, {"lrm", 0x200E},
    {"rlm", 0x200F},
    {"ndash", 0x2013}, {"mdash", 0x2014}, {"lsquo", 0x2018},
    {"rsquo", 0x2019}, {"sbquo", 0x201A}, {"ldquo", 0x201C},
    {"rdquo", 0x201D}, {"bdquo", 0x201E}, {"dagger", 0x2020},
    {"Dagger", 0x2021}, {"bull", 0x2022}, {"hellip", 0x2026},
    {"permil", 0x2030}, {"prime", 0x2032}, {"Prime", 0x2033},
    {"lsaquo", 0x2039}, {"rsaquo", 0x203A}, {"oline", 0x203E},
    {"frasl", 0x2044},
    {"euro", 0x20AC}, {"image", 0x2111}, {"weierp", 0x2118},
    {"real", 0x211C}, {"trade", 0x2122}, {"alefsym", 0x2135},
    {"larr", 0x2190}, {"uarr", 0x2191}, {"rarr", 0x2192},
    {"darr", 0x2193}, {"harr", 0x2194}, {"crarr", 0x21B5},
    {"lArr", 0x21D0}, {"uArr", 0x21D1}, {"rArr", 0x21D2},
    {"dArr", 0x21D3}, {"hArr", 0x21D4},
    {"forall", 0x2200}, {"part", 0x2202}, {"exist", 0x2203},
    {"empty", 0x2205}, {"nabla", 0x2207}, {"isin", 0x2208},
    {"notin", 0x2209}, {"ni", 0x220B}, {"prod", 0x220F},
    {"sum", 0x2211}, {"minus", 0x2212}, {"lowast", 0x2217},
    {"radic", 0x221A}, {"prop", 0x221D}, {"infin", 0x221E},
    {"ang", 0x2220}, {"and", 0x2227}, {"or", 0x2228},
    {"cap", 0x2229}, {"cup", 0x222A}, {"int", 0x222B},
    {"there4", 0x2234}, {"sim", 0x223C}, {"cong", 0x2245},
    {"asymp", 0x2248}, {"ne", 0x2260}, {"equiv", 0x2261},
    {"le", 0x2264}, {"ge", 0x2265}, {"sub", 0x2282},
    {"sup", 0x2283}, {"nsub", 0x2284}, {"sube", 0x2286},
    {"supe", 0x2287}, {"oplus", 0x2295}, {"otimes", 0x2297},
    {"perp", 0x22A5}, {"sdot", 0x22C5},
    {"lceil", 0x2308}, {"rceil", 0x2309}, {"lfloor", 0x230A},
    {"rfloor", 0x230B}, {"lang", 0x2329}, {"rang", 0x232A},
    {"loz", 0x25CA},
    {"spades", 0x2660}, {"clubs", 0x2663}, {"hearts", 0x2665},
    {"diams", 0x2666},
    {NULL, 0}
};

/* ===== Transliteration table ===== */
const struct translit_entry charconv_translit_table[] = {
    {0x00C0, "A"}, {0x00C1, "A"}, {0x00C2, "A"}, {0x00C3, "A"},
    {0x00C4, "A"}, {0x00C5, "A"}, {0x00C6, "AE"}, {0x00C7, "C"},
    {0x00C8, "E"}, {0x00C9, "E"}, {0x00CA, "E"}, {0x00CB, "E"},
    {0x00CC, "I"}, {0x00CD, "I"}, {0x00CE, "I"}, {0x00CF, "I"},
    {0x00D0, "D"}, {0x00D1, "N"}, {0x00D2, "O"}, {0x00D3, "O"},
    {0x00D4, "O"}, {0x00D5, "O"}, {0x00D6, "O"}, {0x00D8, "O"},
    {0x00D9, "U"}, {0x00DA, "U"}, {0x00DB, "U"}, {0x00DC, "U"},
    {0x00DD, "Y"}, {0x00DE, "Th"}, {0x00DF, "ss"},
    {0x00E0, "a"}, {0x00E1, "a"}, {0x00E2, "a"}, {0x00E3, "a"},
    {0x00E4, "a"}, {0x00E5, "a"}, {0x00E6, "ae"}, {0x00E7, "c"},
    {0x00E8, "e"}, {0x00E9, "e"}, {0x00EA, "e"}, {0x00EB, "e"},
    {0x00EC, "i"}, {0x00ED, "i"}, {0x00EE, "i"}, {0x00EF, "i"},
    {0x00F0, "d"}, {0x00F1, "n"}, {0x00F2, "o"}, {0x00F3, "o"},
    {0x00F4, "o"}, {0x00F5, "o"}, {0x00F6, "o"}, {0x00F8, "o"},
    {0x00F9, "u"}, {0x00FA, "u"}, {0x00FB, "u"}, {0x00FC, "u"},
    {0x00FD, "y"}, {0x00FE, "th"}, {0x00FF, "y"},
    {0x0100, "A"}, {0x0101, "a"}, {0x0102, "A"}, {0x0103, "a"},
    {0x0104, "A"}, {0x0105, "a"}, {0x0106, "C"}, {0x0107, "c"},
    {0x0108, "C"}, {0x0109, "c"}, {0x010A, "C"}, {0x010B, "c"},
    {0x010C, "C"}, {0x010D, "c"}, {0x010E, "D"}, {0x010F, "d"},
    {0x0110, "D"}, {0x0111, "d"}, {0x0112, "E"}, {0x0113, "e"},
    {0x0116, "E"}, {0x0117, "e"}, {0x0118, "E"}, {0x0119, "e"},
    {0x011A, "E"}, {0x011B, "e"}, {0x011C, "G"}, {0x011D, "g"},
    {0x011E, "G"}, {0x011F, "g"}, {0x0120, "G"}, {0x0121, "g"},
    {0x0122, "G"}, {0x0123, "g"}, {0x0124, "H"}, {0x0125, "h"},
    {0x0126, "H"}, {0x0127, "h"}, {0x0128, "I"}, {0x0129, "i"},
    {0x012A, "I"}, {0x012B, "i"}, {0x012E, "I"}, {0x012F, "i"},
    {0x0130, "I"}, {0x0131, "i"}, {0x0134, "J"}, {0x0135, "j"},
    {0x0136, "K"}, {0x0137, "k"}, {0x0139, "L"}, {0x013A, "l"},
    {0x013B, "L"}, {0x013C, "l"}, {0x013D, "L"}, {0x013E, "l"},
    {0x0141, "L"}, {0x0142, "l"}, {0x0143, "N"}, {0x0144, "n"},
    {0x0145, "N"}, {0x0146, "n"}, {0x0147, "N"}, {0x0148, "n"},
    {0x014C, "O"}, {0x014D, "o"}, {0x0150, "O"}, {0x0151, "o"},
    {0x0152, "OE"}, {0x0153, "oe"}, {0x0154, "R"}, {0x0155, "r"},
    {0x0158, "R"}, {0x0159, "r"}, {0x015A, "S"}, {0x015B, "s"},
    {0x015C, "S"}, {0x015D, "s"}, {0x015E, "S"}, {0x015F, "s"},
    {0x0160, "S"}, {0x0161, "s"}, {0x0162, "T"}, {0x0163, "t"},
    {0x0164, "T"}, {0x0165, "t"}, {0x0168, "U"}, {0x0169, "u"},
    {0x016A, "U"}, {0x016B, "u"}, {0x016C, "U"}, {0x016D, "u"},
    {0x016E, "U"}, {0x016F, "u"}, {0x0170, "U"}, {0x0171, "u"},
    {0x0172, "U"}, {0x0173, "u"}, {0x0174, "W"}, {0x0175, "w"},
    {0x0176, "Y"}, {0x0177, "y"}, {0x0178, "Y"},
    {0x0179, "Z"}, {0x017A, "z"}, {0x017B, "Z"}, {0x017C, "z"},
    {0x017D, "Z"}, {0x017E, "z"},
    /* Greek */
    {0x0391, "A"}, {0x0392, "B"}, {0x0393, "G"}, {0x0394, "D"},
    {0x0395, "E"}, {0x0396, "Z"}, {0x0397, "E"}, {0x0398, "Th"},
    {0x0399, "I"}, {0x039A, "K"}, {0x039B, "L"}, {0x039C, "M"},
    {0x039D, "N"}, {0x039E, "X"}, {0x039F, "O"}, {0x03A0, "P"},
    {0x03A1, "R"}, {0x03A3, "S"}, {0x03A4, "T"}, {0x03A5, "Y"},
    {0x03A6, "F"}, {0x03A7, "Ch"}, {0x03A8, "Ps"}, {0x03A9, "O"},
    {0x03B1, "a"}, {0x03B2, "b"}, {0x03B3, "g"}, {0x03B4, "d"},
    {0x03B5, "e"}, {0x03B6, "z"}, {0x03B7, "e"}, {0x03B8, "th"},
    {0x03B9, "i"}, {0x03BA, "k"}, {0x03BB, "l"}, {0x03BC, "m"},
    {0x03BD, "n"}, {0x03BE, "x"}, {0x03BF, "o"}, {0x03C0, "p"},
    {0x03C1, "r"}, {0x03C2, "s"}, {0x03C3, "s"}, {0x03C4, "t"},
    {0x03C5, "y"}, {0x03C6, "f"}, {0x03C7, "ch"}, {0x03C8, "ps"},
    {0x03C9, "o"},
    /* Cyrillic */
    {0x0410, "A"}, {0x0411, "B"}, {0x0412, "V"}, {0x0413, "G"},
    {0x0414, "D"}, {0x0415, "E"}, {0x0416, "Zh"}, {0x0417, "Z"},
    {0x0418, "I"}, {0x0419, "J"}, {0x041A, "K"}, {0x041B, "L"},
    {0x041C, "M"}, {0x041D, "N"}, {0x041E, "O"}, {0x041F, "P"},
    {0x0420, "R"}, {0x0421, "S"}, {0x0422, "T"}, {0x0423, "U"},
    {0x0424, "F"}, {0x0425, "Kh"}, {0x0426, "Ts"}, {0x0427, "Ch"},
    {0x0428, "Sh"}, {0x0429, "Shch"}, {0x042A, "\""}, {0x042B, "Y"},
    {0x042C, "'"}, {0x042D, "E"}, {0x042E, "Yu"}, {0x042F, "Ya"},
    {0x0430, "a"}, {0x0431, "b"}, {0x0432, "v"}, {0x0433, "g"},
    {0x0434, "d"}, {0x0435, "e"}, {0x0436, "zh"}, {0x0437, "z"},
    {0x0438, "i"}, {0x0439, "j"}, {0x043A, "k"}, {0x043B, "l"},
    {0x043C, "m"}, {0x043D, "n"}, {0x043E, "o"}, {0x043F, "p"},
    {0x0440, "r"}, {0x0441, "s"}, {0x0442, "t"}, {0x0443, "u"},
    {0x0444, "f"}, {0x0445, "kh"}, {0x0446, "ts"}, {0x0447, "ch"},
    {0x0448, "sh"}, {0x0449, "shch"}, {0x044A, "\""}, {0x044B, "y"},
    {0x044C, "'"}, {0x044D, "e"}, {0x044E, "yu"}, {0x044F, "ya"},
    /* Symbols */
    {0x00A9, "(c)"}, {0x00AE, "(R)"}, {0x2122, "(TM)"},
    {0x00D7, "x"}, {0x00F7, "/"},
    {0x2013, "-"}, {0x2014, "--"}, {0x2026, "..."},
    {0x2018, "'"}, {0x2019, "'"}, {0x201C, "\""}, {0x201D, "\""},
    {0x00AB, "<<"}, {0x00BB, ">>"},
    {0x00BC, "1/4"}, {0x00BD, "1/2"}, {0x00BE, "3/4"},
    {0x20AC, "EUR"}, {0x00A3, "GBP"}, {0x00A5, "JPY"},
    {0, NULL}
};

/* ===== UTF-8 utilities ===== */

int charconv_utf8_encode(uint32_t cp, unsigned char *buf) {
    if (cp < 0x80) {
        buf[0] = (unsigned char)cp;
        return 1;
    } else if (cp < 0x800) {
        buf[0] = 0xC0 | (cp >> 6);
        buf[1] = 0x80 | (cp & 0x3F);
        return 2;
    } else if (cp < 0x10000) {
        buf[0] = 0xE0 | (cp >> 12);
        buf[1] = 0x80 | ((cp >> 6) & 0x3F);
        buf[2] = 0x80 | (cp & 0x3F);
        return 3;
    } else if (cp <= 0x10FFFF) {
        buf[0] = 0xF0 | (cp >> 18);
        buf[1] = 0x80 | ((cp >> 12) & 0x3F);
        buf[2] = 0x80 | ((cp >> 6) & 0x3F);
        buf[3] = 0x80 | (cp & 0x3F);
        return 4;
    }
    return 0;
}

uint32_t charconv_utf8_decode(const unsigned char *s, int len, int *consumed) {
    if (len <= 0) { *consumed = 0; return 0xFFFFFFFF; }
    unsigned char c = s[0];
    uint32_t cp;
    int need;

    if (c < 0x80) { *consumed = 1; return c; }
    else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; need = 2; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; need = 3; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; need = 4; }
    else { *consumed = 1; return 0xFFFFFFFF; }

    if (need > len) { *consumed = 1; return 0xFFFFFFFF; }
    for (int i = 1; i < need; i++) {
        if ((s[i] & 0xC0) != 0x80) { *consumed = 1; return 0xFFFFFFFF; }
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    *consumed = need;
    if (need == 2 && cp < 0x80) return 0xFFFFFFFF;
    if (need == 3 && cp < 0x800) return 0xFFFFFFFF;
    if (need == 4 && cp < 0x10000) return 0xFFFFFFFF;
    if (cp >= 0xD800 && cp <= 0xDFFF) return 0xFFFFFFFF;
    if (cp > 0x10FFFF) return 0xFFFFFFFF;
    return cp;
}

int charconv_is_valid_utf8(const unsigned char *data, int len) {
    int i = 0;
    while (i < len) {
        int consumed;
        uint32_t cp = charconv_utf8_decode(data + i, len - i, &consumed);
        if (cp == 0xFFFFFFFF || consumed == 0) return 0;
        i += consumed;
    }
    return 1;
}

/* ===== Decode strategy application ===== */
static int apply_decode_strategy(int strategy, unsigned char byte, char *out) {
    switch (strategy) {
    case DS_STRICT:
        return -1;
    case DS_REPLACEMENT_FFFD:
        out[0] = 0xEF; out[1] = 0xBF; out[2] = 0xBD;
        return 3;
    case DS_REPLACEMENT_QUESTION:
        out[0] = '?'; return 1;
    case DS_REPLACEMENT_SUB:
        out[0] = 0x1A; return 1;
    case DS_SKIP:
        return 0;
    case DS_LATIN1_FALLBACK: {
        unsigned char buf[4];
        int n = charconv_utf8_encode((uint32_t)byte, buf);
        memcpy(out, buf, n);
        return n;
    }
    case DS_CP1252_FALLBACK: {
        uint32_t cp = windows_1252_to_unicode[byte];
        if (cp == 0xFFFD) { out[0] = '?'; return 1; }
        unsigned char buf[4];
        int n = charconv_utf8_encode(cp, buf);
        memcpy(out, buf, n);
        return n;
    }
    case DS_HEX_ESCAPE_X:
        return sprintf(out, "\\x%02x", byte);
    case DS_HEX_ESCAPE_PERCENT:
        return sprintf(out, "%%%02X", byte);
    case DS_HEX_ESCAPE_ANGLE:
        return sprintf(out, "<%02X>", byte);
    case DS_HEX_ESCAPE_0X:
        return sprintf(out, "0x%02X", byte);
    case DS_HEX_ESCAPE_BRACKET:
        return sprintf(out, "[%02X]", byte);
    case DS_OCTAL_ESCAPE:
        return sprintf(out, "\\%03o", byte);
    case DS_CARET_NOTATION:
        if (byte < 0x20) {
            out[0] = '^'; out[1] = byte + 0x40;
            return 2;
        } else if (byte == 0x7F) {
            out[0] = '^'; out[1] = '?';
            return 2;
        }
        return sprintf(out, "\\x%02x", byte);
    case DS_UNICODE_ESCAPE_U:
        return sprintf(out, "\\u%04X", byte);
    case DS_BYTE_VALUE_DECIMAL:
        return sprintf(out, "{%u}", byte);
    case DS_BYTE_VALUE_BACKSLASH_DEC:
        return sprintf(out, "\\%u", byte);
    case DS_DOUBLE_PERCENT:
        return sprintf(out, "%%%%%02X", byte);
    }
    return -1;
}

/* UTF-16 error handler: matches Rust handle_decode_error_utf16 */
static int apply_decode_strategy_utf16(int strategy, uint16_t unit, char *out) {
    switch (strategy) {
    case DS_STRICT:
        return -1;
    case DS_REPLACEMENT_FFFD:
        out[0] = 0xEF; out[1] = 0xBF; out[2] = 0xBD;
        return 3;
    case DS_REPLACEMENT_QUESTION:
        out[0] = '?'; return 1;
    case DS_REPLACEMENT_SUB:
        out[0] = 0x1A; return 1;
    case DS_SKIP:
        return 0;
    case DS_HEX_ESCAPE_X:
        return sprintf(out, "\\x%04x", unit);
    case DS_HEX_ESCAPE_PERCENT:
        return sprintf(out, "%%%02X%%%02X", (unit >> 8) & 0xFF, unit & 0xFF);
    case DS_UNICODE_ESCAPE_U:
        return sprintf(out, "\\u%04X", unit);
    default:
        /* All other strategies fall back to FFFD */
        out[0] = 0xEF; out[1] = 0xBF; out[2] = 0xBD;
        return 3;
    }
}

/* UTF-32 error handler: matches Rust handle_decode_error_utf32 */
static int apply_decode_strategy_utf32(int strategy, uint32_t codepoint, char *out) {
    switch (strategy) {
    case DS_STRICT:
        return -1;
    case DS_REPLACEMENT_FFFD:
        out[0] = 0xEF; out[1] = 0xBF; out[2] = 0xBD;
        return 3;
    case DS_REPLACEMENT_QUESTION:
        out[0] = '?'; return 1;
    case DS_REPLACEMENT_SUB:
        out[0] = 0x1A; return 1;
    case DS_SKIP:
        return 0;
    case DS_UNICODE_ESCAPE_U:
        return sprintf(out, "\\U%08X", codepoint);
    default:
        /* All other strategies fall back to FFFD */
        out[0] = 0xEF; out[1] = 0xBF; out[2] = 0xBD;
        return 3;
    }
}

/* ===== Punycode (RFC 3492) for single-character IDNA encoding ===== */
static int punycode_encode_idna(uint32_t *codepoints, int cplen, char *out) {
    #define PC_BASE 36
    #define PC_TMIN 1
    #define PC_TMAX 26
    #define PC_SKEW 38
    #define PC_DAMP 700
    #define PC_IBIAS 72
    #define PC_IN 0x80

    int pos = 0;

    /* ASCII-only check: return lowercase directly (matches Rust encode_idna) */
    int all_ascii = 1;
    for (int j = 0; j < cplen; j++)
        if (codepoints[j] >= 0x80) { all_ascii = 0; break; }
    if (all_ascii) {
        for (int j = 0; j < cplen; j++)
            out[pos++] = (codepoints[j] >= 'A' && codepoints[j] <= 'Z')
                ? (char)(codepoints[j] + 32) : (char)codepoints[j];
        out[pos] = 0;
        return pos;
    }

    pos += sprintf(out, "xn--");

    /* Copy basic code points */
    int b = 0;
    for (int j = 0; j < cplen; j++)
        if (codepoints[j] < 0x80) { out[pos++] = (char)codepoints[j]; b++; }
    uint32_t h = b;
    if (b > 0) out[pos++] = '-';

    uint32_t n = PC_IN, delta = 0, bias = PC_IBIAS;
    while (h < (uint32_t)cplen) {
        uint32_t m = 0xFFFFFFFF;
        for (int j = 0; j < cplen; j++)
            if (codepoints[j] >= n && codepoints[j] < m) m = codepoints[j];
        if (m - n > (0xFFFFFFFF - delta) / (h + 1)) break;
        delta += (m - n) * (h + 1);
        n = m;
        for (int j = 0; j < cplen; j++) {
            if (codepoints[j] < n) { delta++; if (delta == 0) break; }
            else if (codepoints[j] == n) {
                uint32_t q = delta;
                for (uint32_t k = PC_BASE; ; k += PC_BASE) {
                    uint32_t t = (k <= bias) ? PC_TMIN :
                                 (k >= bias + PC_TMAX) ? PC_TMAX : k - bias;
                    if (q < t) break;
                    uint32_t d = t + (q - t) % (PC_BASE - t);
                    out[pos++] = (d < 26) ? ('a' + d) : ('0' + d - 26);
                    q = (q - t) / (PC_BASE - t);
                }
                out[pos++] = (q < 26) ? ('a' + q) : ('0' + q - 26);
                /* adapt bias */
                uint32_t dd = (h == (uint32_t)b) ? delta / PC_DAMP : delta / 2;
                dd += dd / (h + 1);
                uint32_t kk = 0;
                while (dd > ((PC_BASE - PC_TMIN) * PC_TMAX) / 2) {
                    dd /= PC_BASE - PC_TMIN;
                    kk += PC_BASE;
                }
                bias = kk + ((PC_BASE - PC_TMIN + 1) * dd) / (dd + PC_SKEW);
                delta = 0;
                h++;
            }
        }
        delta++;
        n++;
    }
    out[pos] = 0;
    return pos;

    #undef PC_BASE
    #undef PC_TMIN
    #undef PC_TMAX
    #undef PC_SKEW
    #undef PC_DAMP
    #undef PC_IBIAS
    #undef PC_IN
}

/* ===== Encode strategy application ===== */
static int apply_encode_strategy(int strategy, uint32_t codepoint,
    const unsigned char *utf8_bytes, int utf8_len, char *out)
{
    switch (strategy) {
    case ES_STRICT:
        return -1;
    case ES_REPLACEMENT_QUESTION:
        out[0] = '?'; return 1;
    case ES_REPLACEMENT_SUB:
        out[0] = 0x1A; return 1;
    case ES_REPLACEMENT_SPACE:
        out[0] = ' '; return 1;
    case ES_REPLACEMENT_ZWSP:
        /* ZWSP is invisible, effectively skip (matches Rust behavior) */
        return 0;
    case ES_REPLACEMENT_UNDERSCORE:
        out[0] = '_'; return 1;
    case ES_SKIP:
        return 0;
    case ES_HTML_DECIMAL:
        return sprintf(out, "&#%u;", codepoint);
    case ES_HTML_HEX:
        return sprintf(out, "&#x%X;", codepoint);
    case ES_HTML_NAMED: {
        for (int i = 0; charconv_html_entities[i].name; i++) {
            if (charconv_html_entities[i].codepoint == codepoint)
                return sprintf(out, "&%s;", charconv_html_entities[i].name);
        }
        return sprintf(out, "&#%u;", codepoint);
    }
    case ES_XML_NUMERIC:
        return sprintf(out, "&#%u;", codepoint);
    case ES_URL_ENCODING: {
        int pos = 0;
        for (int i = 0; i < utf8_len; i++)
            pos += sprintf(out + pos, "%%%02X", utf8_bytes[i]);
        return pos;
    }
    case ES_DOUBLE_URL_ENCODING: {
        int pos = 0;
        for (int i = 0; i < utf8_len; i++)
            pos += sprintf(out + pos, "%%25%02X", utf8_bytes[i]);
        return pos;
    }
    case ES_HEX_ESCAPE_X: {
        int pos = 0;
        for (int i = 0; i < utf8_len; i++)
            pos += sprintf(out + pos, "\\x%02x", utf8_bytes[i]);
        return pos;
    }
    case ES_UNICODE_ESCAPE_U4:
        if (codepoint <= 0xFFFF)
            return sprintf(out, "\\u%04X", codepoint);
        else {
            uint32_t adj = codepoint - 0x10000;
            uint16_t hi = 0xD800 + (adj >> 10);
            uint16_t lo = 0xDC00 + (adj & 0x3FF);
            return sprintf(out, "\\u%04X\\u%04X", hi, lo);
        }
    case ES_UNICODE_ESCAPE_U8:
        return sprintf(out, "\\U%08X", codepoint);
    case ES_UNICODE_ESCAPE_XBRACE:
        return sprintf(out, "\\x{%X}", codepoint);
    case ES_UNICODE_ESCAPE_UPLUS:
        return sprintf(out, "U+%04X", codepoint);
    case ES_UNICODE_ESCAPE_UBRACE:
        return sprintf(out, "\\u{%X}", codepoint);
    case ES_PYTHON_NAMED_ESCAPE:
        return sprintf(out, "\\N{U+%04X}", codepoint);
    case ES_JAVA_SURROGATE_PAIRS:
        if (codepoint <= 0xFFFF)
            return sprintf(out, "\\u%04X", codepoint);
        else {
            uint32_t adj = codepoint - 0x10000;
            uint16_t hi = 0xD800 + (adj >> 10);
            uint16_t lo = 0xDC00 + (adj & 0x3FF);
            return sprintf(out, "\\u%04X\\u%04X", hi, lo);
        }
    case ES_CSS_ESCAPE:
        return sprintf(out, "\\%06X", codepoint);
    case ES_JSON_ESCAPE:
        if (codepoint <= 0xFFFF)
            return sprintf(out, "\\u%04x", codepoint);
        else {
            uint32_t adj = codepoint - 0x10000;
            uint16_t hi = 0xD800 + (adj >> 10);
            uint16_t lo = 0xDC00 + (adj & 0x3FF);
            return sprintf(out, "\\u%04x\\u%04x", hi, lo);
        }
    case ES_PUNYCODE: {
        /* RFC 3492 punycode with xn-- prefix */
        uint32_t cps[1] = { codepoint };
        return punycode_encode_idna(cps, 1, out);
    }
    case ES_TRANSLITERATION: {
        for (int i = 0; charconv_translit_table[i].ascii; i++) {
            if (charconv_translit_table[i].codepoint == codepoint) {
                int slen = strlen(charconv_translit_table[i].ascii);
                memcpy(out, charconv_translit_table[i].ascii, slen);
                return slen;
            }
        }
        out[0] = '?'; return 1;
    }
    case ES_BASE64_INLINE: {
        /* Base64 encode the UTF-8 bytes (RFC 4648) */
        static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        int pos = 0;
        pos += sprintf(out + pos, "[base64:");
        int i = 0;
        while (i < utf8_len) {
            int start = i;
            unsigned int b0 = utf8_bytes[i++];
            unsigned int b1 = (i < utf8_len) ? utf8_bytes[i++] : 0;
            unsigned int b2 = (i < utf8_len) ? utf8_bytes[i++] : 0;
            int nbytes = i - start;
            unsigned int triple = (b0 << 16) | (b1 << 8) | b2;
            out[pos++] = b64[(triple >> 18) & 0x3F];
            out[pos++] = b64[(triple >> 12) & 0x3F];
            out[pos++] = (nbytes > 1) ? b64[(triple >> 6) & 0x3F] : '=';
            out[pos++] = (nbytes > 2) ? b64[triple & 0x3F] : '=';
        }
        out[pos++] = ']';
        return pos;
    }
    case ES_QUOTED_PRINTABLE: {
        int pos = 0;
        for (int i = 0; i < utf8_len; i++)
            pos += sprintf(out + pos, "=%02X", utf8_bytes[i]);
        return pos;
    }
    case ES_NCR_DECIMAL:
        return sprintf(out, "&#%u;", codepoint);
    }
    return -1;
}

/* ===== Reverse map for single-byte encode ===== */

static int sb_reverse_cmp(const void *a, const void *b) {
    const struct sb_reverse_entry *ea = (const struct sb_reverse_entry *)a;
    const struct sb_reverse_entry *eb = (const struct sb_reverse_entry *)b;
    if (ea->codepoint < eb->codepoint) return -1;
    if (ea->codepoint > eb->codepoint) return 1;
    return 0;
}

void charconv_init_reverse_maps(struct CharEncoding *encodings, int count) {
    for (int e = 0; e < count; e++) {
        if (encodings[e].type != ENC_TYPE_SINGLE_BYTE) continue;
        if (!encodings[e].to_unicode) continue;
        if (encodings[e].reverse_map) continue; /* already built */

        const uint32_t *table = encodings[e].to_unicode;
        /* Count valid entries */
        int n = 0;
        for (int b = 0; b < 256; b++) {
            if (table[b] != 0xFFFD && table[b] != 0xFFFF) n++;
        }
        struct sb_reverse_entry *map = malloc(n * sizeof(struct sb_reverse_entry));
        if (!map) continue;
        int idx = 0;
        for (int b = 0; b < 256; b++) {
            if (table[b] != 0xFFFD && table[b] != 0xFFFF) {
                map[idx].codepoint = table[b];
                map[idx].byte = (uint8_t)b;
                idx++;
            }
        }
        qsort(map, n, sizeof(struct sb_reverse_entry), sb_reverse_cmp);
        /* Deduplicate: for duplicate codepoints, keep highest byte value
         * (matches Rust HashMap "last wins" behavior since table is 0..255) */
        int dedup = 0;
        for (int j = 0; j < n; j++) {
            if (dedup > 0 && map[dedup-1].codepoint == map[j].codepoint) {
                /* Same codepoint — keep higher byte (later in table) */
                if (map[j].byte > map[dedup-1].byte)
                    map[dedup-1].byte = map[j].byte;
            } else {
                map[dedup++] = map[j];
            }
        }
        encodings[e].reverse_map = map;
        encodings[e].reverse_map_size = dedup;
    }
}

/* Binary search in reverse map */
static int sb_reverse_lookup(const struct sb_reverse_entry *map, int size, uint32_t cp) {
    int lo = 0, hi = size - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (map[mid].codepoint == cp) return map[mid].byte;
        if (map[mid].codepoint < cp) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

/* ===== Single-byte decode ===== */
static int sb_decode(const struct CharEncoding *enc,
    const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors)
{
    const uint32_t *table = enc->to_unicode;
    int opos = 0;
    *had_errors = 0;

    for (int i = 0; i < inlen; i++) {
        uint32_t cp = table[in[i]];
        if (cp == 0xFFFD || cp == 0xFFFF) {
            *had_errors = 1;
            char strbuf[32];
            int slen = apply_decode_strategy(strategy, in[i], strbuf);
            if (slen < 0) return -1;
            if (opos + slen > outsize) return -1;
            memcpy(out + opos, strbuf, slen);
            opos += slen;
        } else {
            unsigned char buf[4];
            int n = charconv_utf8_encode(cp, buf);
            if (opos + n > outsize) return -1;
            memcpy(out + opos, buf, n);
            opos += n;
        }
    }
    return opos;
}

/* ===== Single-byte encode ===== */
static int sb_encode(const struct CharEncoding *enc,
    const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors)
{
    int opos = 0, i = 0;
    *had_errors = 0;

    while (i < inlen) {
        int consumed;
        uint32_t cp = charconv_utf8_decode(in + i, inlen - i, &consumed);
        if (consumed == 0) consumed = 1;

        int found = -1;
        if (cp != 0xFFFFFFFF && enc->reverse_map) {
            found = sb_reverse_lookup(enc->reverse_map, enc->reverse_map_size, cp);
        } else if (cp != 0xFFFFFFFF && enc->to_unicode) {
            /* Fallback linear scan if no reverse map */
            for (int b = 0; b < 256; b++) {
                if (enc->to_unicode[b] == cp) { found = b; break; }
            }
        }

        if (found >= 0) {
            if (opos >= outsize) return -1;
            out[opos++] = (unsigned char)found;
        } else {
            *had_errors = 1;
            char strbuf[64];
            int slen = apply_encode_strategy(strategy, cp, in + i, consumed, strbuf);
            if (slen < 0) return -1;
            if (opos + slen > outsize) return -1;
            memcpy(out + opos, strbuf, slen);
            opos += slen;
        }
        i += consumed;
    }
    return opos;
}

/* ===== UTF-8 decode (validate + apply strategy) ===== */
static int utf8_decode_conv(const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors)
{
    int opos = 0, i = 0;
    *had_errors = 0;

    while (i < inlen) {
        int consumed;
        uint32_t cp = charconv_utf8_decode(in + i, inlen - i, &consumed);
        if (cp == 0xFFFFFFFF) {
            *had_errors = 1;
            char strbuf[32];
            int slen = apply_decode_strategy(strategy, in[i], strbuf);
            if (slen < 0) return -1;
            if (opos + slen > outsize) return -1;
            memcpy(out + opos, strbuf, slen);
            opos += slen;
            if (consumed == 0) consumed = 1;
            i += consumed;
        } else {
            /* Valid UTF-8: copy bytes through */
            if (opos + consumed > outsize) return -1;
            memcpy(out + opos, in + i, consumed);
            opos += consumed;
            i += consumed;
        }
    }
    return opos;
}

/* ===== UTF-8 encode (passthrough — UTF-8 can encode all Unicode) ===== */
static int utf8_encode_conv(const unsigned char *in, int inlen,
    unsigned char *out, int outsize,
    int strategy __attribute__((unused)),
    int *had_errors)
{
    *had_errors = 0;
    if (inlen > outsize) return -1;
    memcpy(out, in, inlen);
    return inlen;
}

/* ===== UTF-16 decode ===== */
static int utf16_decode_impl(const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors, int big_endian)
{
    int opos = 0, i = 0;
    *had_errors = 0;

    while (i + 1 < inlen) {
        uint16_t unit = big_endian
            ? ((uint16_t)in[i] << 8) | in[i+1]
            : (uint16_t)in[i] | ((uint16_t)in[i+1] << 8);

        if (unit >= 0xD800 && unit <= 0xDBFF) {
            /* High surrogate */
            if (i + 3 < inlen) {
                uint16_t low = big_endian
                    ? ((uint16_t)in[i+2] << 8) | in[i+3]
                    : (uint16_t)in[i+2] | ((uint16_t)in[i+3] << 8);
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    uint32_t cp = 0x10000 + (((uint32_t)(unit - 0xD800) << 10) | (low - 0xDC00));
                    unsigned char buf[4];
                    int n = charconv_utf8_encode(cp, buf);
                    if (opos + n > outsize) return -1;
                    memcpy(out + opos, buf, n);
                    opos += n;
                    i += 4;
                    continue;
                }
            }
            /* Invalid surrogate */
            *had_errors = 1;
            char strbuf[32];
            int slen = apply_decode_strategy_utf16(strategy, unit, strbuf);
            if (slen < 0) return -1;
            if (opos + slen > outsize) return -1;
            memcpy(out + opos, strbuf, slen);
            opos += slen;
            i += 2;
        } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
            /* Orphan low surrogate */
            *had_errors = 1;
            char strbuf[32];
            int slen = apply_decode_strategy_utf16(strategy, unit, strbuf);
            if (slen < 0) return -1;
            if (opos + slen > outsize) return -1;
            memcpy(out + opos, strbuf, slen);
            opos += slen;
            i += 2;
        } else {
            unsigned char buf[4];
            int n = charconv_utf8_encode((uint32_t)unit, buf);
            if (opos + n > outsize) return -1;
            memcpy(out + opos, buf, n);
            opos += n;
            i += 2;
        }
    }
    /* Trailing byte — Rust passes byte as u16 (0x00XX) */
    if (i < inlen) {
        *had_errors = 1;
        char strbuf[32];
        int slen = apply_decode_strategy_utf16(strategy, (uint16_t)in[i], strbuf);
        if (slen < 0) return -1;
        if (opos + slen > outsize) return -1;
        memcpy(out + opos, strbuf, slen);
        opos += slen;
    }
    return opos;
}

/* UTF-16 with BOM detection */
static int utf16_decode_bom(const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors)
{
    if (inlen >= 2) {
        if (in[0] == 0xFE && in[1] == 0xFF)
            return utf16_decode_impl(in + 2, inlen - 2, out, outsize, strategy, had_errors, 1);
        if (in[0] == 0xFF && in[1] == 0xFE)
            return utf16_decode_impl(in + 2, inlen - 2, out, outsize, strategy, had_errors, 0);
    }
    /* Default: BE */
    return utf16_decode_impl(in, inlen, out, outsize, strategy, had_errors, 1);
}

/* ===== UTF-16 encode ===== */
static int utf16_encode_impl(const unsigned char *in, int inlen,
    unsigned char *out, int outsize,
    int strategy __attribute__((unused)),
    int *had_errors, int big_endian)
{
    int opos = 0, i = 0;
    *had_errors = 0;

    while (i < inlen) {
        int consumed;
        uint32_t cp = charconv_utf8_decode(in + i, inlen - i, &consumed);
        if (consumed == 0) consumed = 1;
        if (cp == 0xFFFFFFFF) { i += consumed; continue; }

        if (cp < 0x10000) {
            if (opos + 2 > outsize) return -1;
            uint16_t unit = (uint16_t)cp;
            if (big_endian) {
                out[opos++] = (unit >> 8) & 0xFF;
                out[opos++] = unit & 0xFF;
            } else {
                out[opos++] = unit & 0xFF;
                out[opos++] = (unit >> 8) & 0xFF;
            }
        } else {
            if (opos + 4 > outsize) return -1;
            uint32_t adj = cp - 0x10000;
            uint16_t hi = 0xD800 + (adj >> 10);
            uint16_t lo = 0xDC00 + (adj & 0x3FF);
            if (big_endian) {
                out[opos++] = (hi >> 8) & 0xFF;
                out[opos++] = hi & 0xFF;
                out[opos++] = (lo >> 8) & 0xFF;
                out[opos++] = lo & 0xFF;
            } else {
                out[opos++] = hi & 0xFF;
                out[opos++] = (hi >> 8) & 0xFF;
                out[opos++] = lo & 0xFF;
                out[opos++] = (lo >> 8) & 0xFF;
            }
        }
        i += consumed;
    }
    return opos;
}

/* UTF-16 with BOM encode (BE with BOM prefix) */
static int utf16_encode_bom(const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors)
{
    if (outsize < 2) return -1;
    out[0] = 0xFE; out[1] = 0xFF; /* BE BOM */
    int n = utf16_encode_impl(in, inlen, out + 2, outsize - 2, strategy, had_errors, 1);
    return n < 0 ? -1 : n + 2;
}

/* ===== UTF-32 decode ===== */
static int utf32_decode_impl(const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors, int big_endian)
{
    int opos = 0, i = 0;
    *had_errors = 0;

    while (i + 3 < inlen) {
        uint32_t cp;
        if (big_endian)
            cp = ((uint32_t)in[i] << 24) | ((uint32_t)in[i+1] << 16) |
                 ((uint32_t)in[i+2] << 8) | in[i+3];
        else
            cp = in[i] | ((uint32_t)in[i+1] << 8) |
                 ((uint32_t)in[i+2] << 16) | ((uint32_t)in[i+3] << 24);

        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
            *had_errors = 1;
            char strbuf[32];
            int slen = apply_decode_strategy_utf32(strategy, cp, strbuf);
            if (slen < 0) return -1;
            if (opos + slen > outsize) return -1;
            memcpy(out + opos, strbuf, slen);
            opos += slen;
        } else {
            unsigned char buf[4];
            int n = charconv_utf8_encode(cp, buf);
            if (opos + n > outsize) return -1;
            memcpy(out + opos, buf, n);
            opos += n;
        }
        i += 4;
    }
    /* Trailing bytes — Rust marks as errors but generates no output */
    if (i < inlen) {
        *had_errors = 1;
    }
    return opos;
}

/* UTF-32 with BOM detection */
static int utf32_decode_bom(const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors)
{
    if (inlen >= 4) {
        if (in[0] == 0x00 && in[1] == 0x00 && in[2] == 0xFE && in[3] == 0xFF)
            return utf32_decode_impl(in + 4, inlen - 4, out, outsize, strategy, had_errors, 1);
        if (in[0] == 0xFF && in[1] == 0xFE && in[2] == 0x00 && in[3] == 0x00)
            return utf32_decode_impl(in + 4, inlen - 4, out, outsize, strategy, had_errors, 0);
    }
    return utf32_decode_impl(in, inlen, out, outsize, strategy, had_errors, 1);
}

/* ===== UTF-32 encode ===== */
static int utf32_encode_impl(const unsigned char *in, int inlen,
    unsigned char *out, int outsize,
    int strategy __attribute__((unused)),
    int *had_errors, int big_endian)
{
    int opos = 0, i = 0;
    *had_errors = 0;

    while (i < inlen) {
        int consumed;
        uint32_t cp = charconv_utf8_decode(in + i, inlen - i, &consumed);
        if (consumed == 0) consumed = 1;
        if (cp == 0xFFFFFFFF) { i += consumed; continue; }

        if (opos + 4 > outsize) return -1;
        if (big_endian) {
            out[opos++] = (cp >> 24) & 0xFF;
            out[opos++] = (cp >> 16) & 0xFF;
            out[opos++] = (cp >> 8) & 0xFF;
            out[opos++] = cp & 0xFF;
        } else {
            out[opos++] = cp & 0xFF;
            out[opos++] = (cp >> 8) & 0xFF;
            out[opos++] = (cp >> 16) & 0xFF;
            out[opos++] = (cp >> 24) & 0xFF;
        }
        i += consumed;
    }
    return opos;
}

static int utf32_encode_bom(const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors)
{
    if (outsize < 4) return -1;
    out[0] = 0x00; out[1] = 0x00; out[2] = 0xFE; out[3] = 0xFF;
    int n = utf32_encode_impl(in, inlen, out + 4, outsize - 4, strategy, had_errors, 1);
    return n < 0 ? -1 : n + 4;
}

/* ===== UTF-7 decode ===== */
static int utf7_decode(const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors)
{
    static const int b64val[128] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1
    };

    int opos = 0, i = 0;
    *had_errors = 0;

    while (i < inlen) {
        if (in[i] == '+') {
            i++;
            if (i < inlen && in[i] == '-') {
                /* +- = literal + */
                if (opos >= outsize) return -1;
                out[opos++] = '+';
                i++;
            } else {
                /* Base64 encoded section */
                uint32_t accum = 0;
                int bits = 0;
                while (i < inlen && in[i] != '-') {
                    if (in[i] >= 128 || b64val[in[i]] < 0) {
                        *had_errors = 1;
                        break;
                    }
                    accum = (accum << 6) | b64val[in[i]];
                    bits += 6;
                    i++;
                    while (bits >= 16) {
                        bits -= 16;
                        uint16_t unit = (accum >> bits) & 0xFFFF;
                        /* Handle surrogate pairs */
                        if (unit >= 0xD800 && unit <= 0xDBFF) {
                            /* Need more data for low surrogate */
                            /* Continue accumulating */
                        } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
                            /* This shouldn't happen standalone - error */
                            *had_errors = 1;
                        } else {
                            unsigned char buf[4];
                            int n = charconv_utf8_encode((uint32_t)unit, buf);
                            if (opos + n > outsize) return -1;
                            memcpy(out + opos, buf, n);
                            opos += n;
                        }
                    }
                }
                if (i < inlen && in[i] == '-') i++;
            }
        } else if (in[i] >= 0x80) {
            /* Bytes >= 0x80 are not valid in UTF-7 direct mode */
            *had_errors = 1;
            char strbuf[32];
            int slen = apply_decode_strategy(strategy, in[i], strbuf);
            if (slen < 0) return -1;
            if (opos + slen > outsize) return -1;
            memcpy(out + opos, strbuf, slen);
            opos += slen;
            i++;
        } else {
            if (opos >= outsize) return -1;
            out[opos++] = in[i++];
        }
    }
    return opos;
}

/* UTF-7 encode */
static int utf7_encode(const unsigned char *in, int inlen,
    unsigned char *out, int outsize,
    int strategy __attribute__((unused)),
    int *had_errors)
{
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int opos = 0, i = 0;
    *had_errors = 0;

    while (i < inlen) {
        int consumed;
        uint32_t cp = charconv_utf8_decode(in + i, inlen - i, &consumed);
        if (consumed == 0) consumed = 1;
        if (cp == 0xFFFFFFFF) { i += consumed; continue; }

        if (cp == '+') {
            if (opos + 2 > outsize) return -1;
            out[opos++] = '+'; out[opos++] = '-';
            i += consumed;
        } else if (cp >= 0x20 && cp <= 0x7E) {
            if (opos >= outsize) return -1;
            out[opos++] = (unsigned char)cp;
            i += consumed;
        } else {
            /* Encode as base64 block */
            if (opos >= outsize) return -1;
            out[opos++] = '+';
            /* Collect codepoints for encoding */
            uint32_t accum = 0;
            int bits = 0;
            while (i < inlen) {
                uint32_t cp2 = charconv_utf8_decode(in + i, inlen - i, &consumed);
                if (consumed == 0) break;
                if (cp2 >= 0x20 && cp2 <= 0x7E && cp2 != '+') break;

                /* Encode as UTF-16 */
                if (cp2 < 0x10000) {
                    accum = (accum << 16) | cp2;
                    bits += 16;
                } else {
                    uint32_t adj = cp2 - 0x10000;
                    accum = (accum << 16) | (0xD800 + (adj >> 10));
                    bits += 16;
                    while (bits >= 6) {
                        bits -= 6;
                        if (opos >= outsize) return -1;
                        out[opos++] = b64[(accum >> bits) & 0x3F];
                    }
                    accum = (accum << 16) | (0xDC00 + (adj & 0x3FF));
                    bits += 16;
                }
                while (bits >= 6) {
                    bits -= 6;
                    if (opos >= outsize) return -1;
                    out[opos++] = b64[(accum >> bits) & 0x3F];
                }
                i += consumed;
            }
            /* Flush remaining bits */
            if (bits > 0) {
                if (opos >= outsize) return -1;
                out[opos++] = b64[(accum << (6 - bits)) & 0x3F];
            }
            /* Add terminating - only if next char could be confused with base64 */
            if (i < inlen) {
                int nc;
                charconv_utf8_decode(in + i, inlen - i, &nc);
                unsigned char c = in[i];
                int is_b64 = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                             (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '-';
                if (is_b64) {
                    if (opos >= outsize) return -1;
                    out[opos++] = '-';
                }
            }
        }
    }
    return opos;
}

/* ===== CESU-8 decode ===== */
static int cesu8_decode(const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors)
{
    int opos = 0, i = 0;
    *had_errors = 0;

    while (i < inlen) {
        /* Check for CESU-8 surrogate pair: ED Ax xx ED Bx xx */
        if (i + 5 < inlen && in[i] == 0xED &&
            (in[i+1] & 0xF0) == 0xA0 && (in[i+2] & 0xC0) == 0x80 &&
            in[i+3] == 0xED && (in[i+4] & 0xF0) == 0xB0 && (in[i+5] & 0xC0) == 0x80) {
            /* Decode surrogate pair */
            uint16_t hi = ((in[i+1] & 0x0F) << 6) | (in[i+2] & 0x3F);
            uint16_t lo = ((in[i+4] & 0x0F) << 6) | (in[i+5] & 0x3F);
            uint32_t cp = 0x10000 + ((hi << 10) | lo);
            unsigned char buf[4];
            int n = charconv_utf8_encode(cp, buf);
            if (opos + n > outsize) return -1;
            memcpy(out + opos, buf, n);
            opos += n;
            i += 6;
            continue;
        }

        /* Normal UTF-8 */
        int consumed;
        uint32_t cp = charconv_utf8_decode(in + i, inlen - i, &consumed);
        if (cp == 0xFFFFFFFF) {
            *had_errors = 1;
            char strbuf[32];
            int slen = apply_decode_strategy(strategy, in[i], strbuf);
            if (slen < 0) return -1;
            if (opos + slen > outsize) return -1;
            memcpy(out + opos, strbuf, slen);
            opos += slen;
            if (consumed == 0) consumed = 1;
            i += consumed;
        } else {
            if (opos + consumed > outsize) return -1;
            memcpy(out + opos, in + i, consumed);
            opos += consumed;
            i += consumed;
        }
    }
    return opos;
}

/* CESU-8 encode */
static int cesu8_encode(const unsigned char *in, int inlen,
    unsigned char *out, int outsize,
    int strategy __attribute__((unused)),
    int *had_errors)
{
    int opos = 0, i = 0;
    *had_errors = 0;

    while (i < inlen) {
        int consumed;
        uint32_t cp = charconv_utf8_decode(in + i, inlen - i, &consumed);
        if (consumed == 0) consumed = 1;
        if (cp == 0xFFFFFFFF) { i += consumed; continue; }

        if (cp >= 0x10000) {
            /* Encode as surrogate pair in CESU-8 (6 bytes) */
            if (opos + 6 > outsize) return -1;
            uint32_t adj = cp - 0x10000;
            uint16_t hi = 0xD800 + (adj >> 10);
            uint16_t lo = 0xDC00 + (adj & 0x3FF);
            out[opos++] = 0xED;
            out[opos++] = 0xA0 | ((hi >> 6) & 0x0F);
            out[opos++] = 0x80 | (hi & 0x3F);
            out[opos++] = 0xED;
            out[opos++] = 0xB0 | ((lo >> 6) & 0x0F);
            out[opos++] = 0x80 | (lo & 0x3F);
        } else {
            /* Normal UTF-8 */
            unsigned char buf[4];
            int n = charconv_utf8_encode(cp, buf);
            if (opos + n > outsize) return -1;
            memcpy(out + opos, buf, n);
            opos += n;
        }
        i += consumed;
    }
    return opos;
}

/* ===== CJK helper: binary search encode table ===== */
static int cjk_encode_lookup(const struct cjk_encode_entry *table, int size, uint32_t cp) {
    int lo = 0, hi = size - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (table[mid].codepoint == cp) return table[mid].pointer;
        if (table[mid].codepoint < cp) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

/* ===== Shift_JIS decode ===== */
static int shiftjis_decode(const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors)
{
    int opos = 0, i = 0;
    *had_errors = 0;

    while (i < inlen) {
        unsigned char b = in[i];
        if (b <= 0x7F) {
            /* ASCII (with WHATWG overrides) */
            uint32_t cp = b;
            if (b == 0x5C) cp = 0x00A5; /* yen sign */
            else if (b == 0x7E) cp = 0x203E; /* overline */
            unsigned char buf[4];
            int n = charconv_utf8_encode(cp, buf);
            if (opos + n > outsize) return -1;
            memcpy(out + opos, buf, n);
            opos += n;
            i++;
        } else if (b >= 0xA1 && b <= 0xDF) {
            /* Half-width katakana */
            uint32_t cp = 0xFF61 + (b - 0xA1);
            unsigned char buf[4];
            int n = charconv_utf8_encode(cp, buf);
            if (opos + n > outsize) return -1;
            memcpy(out + opos, buf, n);
            opos += n;
            i++;
        } else if ((b >= 0x81 && b <= 0x9F) || (b >= 0xE0 && b <= 0xFC)) {
            /* Lead byte */
            if (i + 1 >= inlen) {
                *had_errors = 1;
                char strbuf[32];
                int slen = apply_decode_strategy(strategy, b, strbuf);
                if (slen < 0) return -1;
                if (opos + slen > outsize) return -1;
                memcpy(out + opos, strbuf, slen);
                opos += slen;
                i++;
                continue;
            }
            unsigned char trail = in[i + 1];
            if (trail < 0x40 || trail == 0x7F || trail > 0xFC) {
                *had_errors = 1;
                char strbuf[32];
                int slen = apply_decode_strategy(strategy, b, strbuf);
                if (slen < 0) return -1;
                if (opos + slen > outsize) return -1;
                memcpy(out + opos, strbuf, slen);
                opos += slen;
                i++;
                continue;
            }

            int lead_offset = (b < 0xA0) ? 0x81 : 0xC1;
            int trail_offset = (trail < 0x7F) ? 0x40 : 0x41;
            int pointer = (b - lead_offset) * 188 + trail - trail_offset;

            if (pointer >= 0 && pointer < JIS0208_DECODE_SIZE) {
                uint32_t cp = jis0208_decode[pointer];
                if (cp != 0) {
                    unsigned char buf[4];
                    int n = charconv_utf8_encode(cp, buf);
                    if (opos + n > outsize) return -1;
                    memcpy(out + opos, buf, n);
                    opos += n;
                    i += 2;
                    continue;
                }
            }
            /* Unmapped */
            *had_errors = 1;
            char strbuf[32];
            int slen = apply_decode_strategy(strategy, b, strbuf);
            if (slen < 0) return -1;
            if (opos + slen > outsize) return -1;
            memcpy(out + opos, strbuf, slen);
            opos += slen;
            i++;
        } else {
            *had_errors = 1;
            char strbuf[32];
            int slen = apply_decode_strategy(strategy, b, strbuf);
            if (slen < 0) return -1;
            if (opos + slen > outsize) return -1;
            memcpy(out + opos, strbuf, slen);
            opos += slen;
            i++;
        }
    }
    return opos;
}

/* Shift_JIS encode */
static int shiftjis_encode(const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors)
{
    int opos = 0, i = 0;
    *had_errors = 0;

    while (i < inlen) {
        int consumed;
        uint32_t cp = charconv_utf8_decode(in + i, inlen - i, &consumed);
        if (consumed == 0) consumed = 1;
        if (cp == 0xFFFFFFFF) { i += consumed; continue; }

        /* WHATWG overrides */
        if (cp == 0x00A5) { if (opos >= outsize) return -1; out[opos++] = 0x5C; i += consumed; continue; }
        if (cp == 0x203E) { if (opos >= outsize) return -1; out[opos++] = 0x7E; i += consumed; continue; }

        /* ASCII */
        if (cp <= 0x7F) {
            if (opos >= outsize) return -1;
            out[opos++] = (unsigned char)cp;
            i += consumed;
            continue;
        }

        /* Half-width katakana */
        if (cp >= 0xFF61 && cp <= 0xFF9F) {
            if (opos >= outsize) return -1;
            out[opos++] = (unsigned char)(cp - 0xFF61 + 0xA1);
            i += consumed;
            continue;
        }

        /* JIS0208 lookup */
        int pointer = cjk_encode_lookup(jis0208_encode, JIS0208_ENCODE_SIZE, cp);
        if (pointer >= 0) {
            int lead = pointer / 188;
            int trail = pointer % 188;
            int lead_byte = lead + ((lead < 0x1F) ? 0x81 : 0xC1);
            int trail_byte = trail + ((trail < 0x3F) ? 0x40 : 0x41);
            if (opos + 2 > outsize) return -1;
            out[opos++] = (unsigned char)lead_byte;
            out[opos++] = (unsigned char)trail_byte;
            i += consumed;
            continue;
        }

        /* Unmappable */
        *had_errors = 1;
        char strbuf[64];
        int slen = apply_encode_strategy(strategy, cp, in + i, consumed, strbuf);
        if (slen < 0) return -1;
        if (opos + slen > outsize) return -1;
        memcpy(out + opos, strbuf, slen);
        opos += slen;
        i += consumed;
    }
    return opos;
}

/* ===== EUC-JP decode ===== */
static int eucjp_decode(const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors)
{
    int opos = 0, i = 0;
    *had_errors = 0;

    while (i < inlen) {
        unsigned char b = in[i];
        if (b <= 0x7F) {
            if (opos >= outsize) return -1;
            out[opos++] = b;
            i++;
        } else if (b == 0x8E) {
            /* Half-width katakana */
            if (i + 1 < inlen && in[i+1] >= 0xA1 && in[i+1] <= 0xDF) {
                uint32_t cp = 0xFF61 + (in[i+1] - 0xA1);
                unsigned char buf[4];
                int n = charconv_utf8_encode(cp, buf);
                if (opos + n > outsize) return -1;
                memcpy(out + opos, buf, n);
                opos += n;
                i += 2;
            } else {
                *had_errors = 1;
                char strbuf[32];
                int slen = apply_decode_strategy(strategy, b, strbuf);
                if (slen < 0) return -1;
                if (opos + slen > outsize) return -1;
                memcpy(out + opos, strbuf, slen);
                opos += slen;
                i++;
            }
        } else if (b == 0x8F) {
            /* JIS X 0212 */
            if (i + 2 < inlen && in[i+1] >= 0xA1 && in[i+1] <= 0xFE &&
                in[i+2] >= 0xA1 && in[i+2] <= 0xFE) {
                int pointer = (in[i+1] - 0xA1) * 94 + (in[i+2] - 0xA1);
                if (pointer >= 0 && pointer < JIS0212_DECODE_SIZE && jis0212_decode[pointer] != 0) {
                    uint32_t cp = jis0212_decode[pointer];
                    unsigned char buf[4];
                    int n = charconv_utf8_encode(cp, buf);
                    if (opos + n > outsize) return -1;
                    memcpy(out + opos, buf, n);
                    opos += n;
                    i += 3;
                    continue;
                }
            }
            *had_errors = 1;
            char strbuf[32];
            int slen = apply_decode_strategy(strategy, b, strbuf);
            if (slen < 0) return -1;
            if (opos + slen > outsize) return -1;
            memcpy(out + opos, strbuf, slen);
            opos += slen;
            i++;
        } else if (b >= 0xA1 && b <= 0xFE) {
            /* JIS X 0208 */
            if (i + 1 < inlen && in[i+1] >= 0xA1 && in[i+1] <= 0xFE) {
                int pointer = (b - 0xA1) * 94 + (in[i+1] - 0xA1);
                if (pointer >= 0 && pointer < JIS0208_DECODE_SIZE && jis0208_decode[pointer] != 0) {
                    uint32_t cp = jis0208_decode[pointer];
                    unsigned char buf[4];
                    int n = charconv_utf8_encode(cp, buf);
                    if (opos + n > outsize) return -1;
                    memcpy(out + opos, buf, n);
                    opos += n;
                    i += 2;
                    continue;
                }
            }
            *had_errors = 1;
            char strbuf[32];
            int slen = apply_decode_strategy(strategy, b, strbuf);
            if (slen < 0) return -1;
            if (opos + slen > outsize) return -1;
            memcpy(out + opos, strbuf, slen);
            opos += slen;
            i++;
        } else {
            *had_errors = 1;
            char strbuf[32];
            int slen = apply_decode_strategy(strategy, b, strbuf);
            if (slen < 0) return -1;
            if (opos + slen > outsize) return -1;
            memcpy(out + opos, strbuf, slen);
            opos += slen;
            i++;
        }
    }
    return opos;
}

/* EUC-JP encode */
static int eucjp_encode(const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors)
{
    int opos = 0, i = 0;
    *had_errors = 0;

    while (i < inlen) {
        int consumed;
        uint32_t cp = charconv_utf8_decode(in + i, inlen - i, &consumed);
        if (consumed == 0) consumed = 1;
        if (cp == 0xFFFFFFFF) { i += consumed; continue; }

        if (cp <= 0x7F) {
            if (opos >= outsize) return -1;
            out[opos++] = (unsigned char)cp;
            i += consumed; continue;
        }

        /* Half-width katakana */
        if (cp >= 0xFF61 && cp <= 0xFF9F) {
            if (opos + 2 > outsize) return -1;
            out[opos++] = 0x8E;
            out[opos++] = (unsigned char)(cp - 0xFF61 + 0xA1);
            i += consumed; continue;
        }

        /* JIS0208 */
        int pointer = cjk_encode_lookup(jis0208_encode, JIS0208_ENCODE_SIZE, cp);
        if (pointer >= 0) {
            int row = pointer / 94;
            int col = pointer % 94;
            if (opos + 2 > outsize) return -1;
            out[opos++] = (unsigned char)(row + 0xA1);
            out[opos++] = (unsigned char)(col + 0xA1);
            i += consumed; continue;
        }

        /* JIS0212 */
        pointer = cjk_encode_lookup(jis0212_encode, JIS0212_ENCODE_SIZE, cp);
        if (pointer >= 0) {
            int row = pointer / 94;
            int col = pointer % 94;
            if (opos + 3 > outsize) return -1;
            out[opos++] = 0x8F;
            out[opos++] = (unsigned char)(row + 0xA1);
            out[opos++] = (unsigned char)(col + 0xA1);
            i += consumed; continue;
        }

        *had_errors = 1;
        char strbuf[64];
        int slen = apply_encode_strategy(strategy, cp, in + i, consumed, strbuf);
        if (slen < 0) return -1;
        if (opos + slen > outsize) return -1;
        memcpy(out + opos, strbuf, slen);
        opos += slen;
        i += consumed;
    }
    return opos;
}

/* ===== ISO-2022-JP decode ===== */
static int iso2022jp_decode(const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors)
{
    int opos = 0, i = 0;
    int mode = 0; /* 0=ASCII, 1=JIS_Roman, 2=JIS0208 */
    *had_errors = 0;

    while (i < inlen) {
        if (in[i] == 0x1B) {
            /* Escape sequence */
            if (i + 2 < inlen) {
                if (in[i+1] == '(' && in[i+2] == 'B') { mode = 0; i += 3; continue; }
                if (in[i+1] == '(' && in[i+2] == 'J') { mode = 1; i += 3; continue; }
                if (in[i+1] == '$' && in[i+2] == '@') { mode = 2; i += 3; continue; }
                if (in[i+1] == '$' && in[i+2] == 'B') { mode = 2; i += 3; continue; }
            }
            *had_errors = 1;
            char strbuf[32];
            int slen = apply_decode_strategy(strategy, in[i], strbuf);
            if (slen < 0) return -1;
            if (opos + slen > outsize) return -1;
            memcpy(out + opos, strbuf, slen);
            opos += slen;
            i++;
            continue;
        }

        if (mode == 2) {
            /* JIS0208 mode: two bytes */
            if (i + 1 < inlen && in[i] >= 0x21 && in[i] <= 0x7E &&
                in[i+1] >= 0x21 && in[i+1] <= 0x7E) {
                int pointer = (in[i] - 0x21) * 94 + (in[i+1] - 0x21);
                if (pointer >= 0 && pointer < JIS0208_DECODE_SIZE && jis0208_decode[pointer] != 0) {
                    uint32_t cp = jis0208_decode[pointer];
                    unsigned char buf[4];
                    int n = charconv_utf8_encode(cp, buf);
                    if (opos + n > outsize) return -1;
                    memcpy(out + opos, buf, n);
                    opos += n;
                    i += 2;
                    continue;
                }
            }
            *had_errors = 1;
            char strbuf[32];
            int slen = apply_decode_strategy(strategy, in[i], strbuf);
            if (slen < 0) return -1;
            if (opos + slen > outsize) return -1;
            memcpy(out + opos, strbuf, slen);
            opos += slen;
            i++;
        } else {
            /* ASCII or JIS Roman */
            if (in[i] >= 0x80) {
                /* Bytes >= 0x80 are not valid in ISO-2022-JP.
                 * Use 0xFF as error byte to match encoding_rs behavior. */
                *had_errors = 1;
                char strbuf[32];
                int slen = apply_decode_strategy(strategy, 0xFF, strbuf);
                if (slen < 0) return -1;
                if (opos + slen > outsize) return -1;
                memcpy(out + opos, strbuf, slen);
                opos += slen;
                i++;
            } else {
                uint32_t cp = in[i];
                if (mode == 1) { /* JIS Roman overrides */
                    if (cp == 0x5C) cp = 0x00A5;
                    else if (cp == 0x7E) cp = 0x203E;
                }
                unsigned char buf[4];
                int n = charconv_utf8_encode(cp, buf);
                if (opos + n > outsize) return -1;
                memcpy(out + opos, buf, n);
                opos += n;
                i++;
            }
        }
    }
    return opos;
}

/* ISO-2022-JP encode */
static int iso2022jp_encode(const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors)
{
    int opos = 0, i = 0;
    int mode = 0; /* 0=ASCII, 2=JIS0208 */
    *had_errors = 0;

    while (i < inlen) {
        int consumed;
        uint32_t cp = charconv_utf8_decode(in + i, inlen - i, &consumed);
        if (consumed == 0) consumed = 1;
        if (cp == 0xFFFFFFFF) { i += consumed; continue; }

        if (cp <= 0x7F) {
            if (mode != 0) {
                if (opos + 3 > outsize) return -1;
                out[opos++] = 0x1B; out[opos++] = '('; out[opos++] = 'B';
                mode = 0;
            }
            if (opos >= outsize) return -1;
            out[opos++] = (unsigned char)cp;
            i += consumed; continue;
        }

        int pointer = cjk_encode_lookup(jis0208_encode, JIS0208_ENCODE_SIZE, cp);
        if (pointer >= 0) {
            if (mode != 2) {
                if (opos + 3 > outsize) return -1;
                out[opos++] = 0x1B; out[opos++] = '$'; out[opos++] = 'B';
                mode = 2;
            }
            int row = pointer / 94;
            int col = pointer % 94;
            if (opos + 2 > outsize) return -1;
            out[opos++] = (unsigned char)(row + 0x21);
            out[opos++] = (unsigned char)(col + 0x21);
            i += consumed; continue;
        }

        *had_errors = 1;
        if (mode != 0) {
            if (opos + 3 > outsize) return -1;
            out[opos++] = 0x1B; out[opos++] = '('; out[opos++] = 'B';
            mode = 0;
        }
        char strbuf[64];
        int slen = apply_encode_strategy(strategy, cp, in + i, consumed, strbuf);
        if (slen < 0) return -1;
        if (opos + slen > outsize) return -1;
        memcpy(out + opos, strbuf, slen);
        opos += slen;
        i += consumed;
    }
    /* Return to ASCII at end */
    if (mode != 0) {
        if (opos + 3 > outsize) return -1;
        out[opos++] = 0x1B; out[opos++] = '('; out[opos++] = 'B';
    }
    return opos;
}

/* ===== GBK decode ===== */
static int gbk_decode_fn(const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors)
{
    int opos = 0, i = 0;
    *had_errors = 0;

    while (i < inlen) {
        unsigned char b = in[i];
        if (b <= 0x7F) {
            if (opos >= outsize) return -1;
            out[opos++] = b;
            i++;
        } else if (b >= 0x81 && b <= 0xFE) {
            if (i + 1 >= inlen) {
                *had_errors = 1;
                char strbuf[32];
                int slen = apply_decode_strategy(strategy, b, strbuf);
                if (slen < 0) return -1;
                if (opos + slen > outsize) return -1;
                memcpy(out + opos, strbuf, slen);
                opos += slen;
                i++; continue;
            }
            unsigned char trail = in[i+1];
            int offset = (trail < 0x7F) ? 0x40 : 0x41;
            if ((trail >= 0x40 && trail <= 0x7E) || (trail >= 0x80 && trail <= 0xFE)) {
                int pointer = (b - 0x81) * 190 + trail - offset;
                if (pointer >= 0 && pointer < GB18030_DECODE_SIZE && gb18030_decode[pointer] != 0) {
                    uint32_t cp = gb18030_decode[pointer];
                    unsigned char buf[4];
                    int n = charconv_utf8_encode(cp, buf);
                    if (opos + n > outsize) return -1;
                    memcpy(out + opos, buf, n);
                    opos += n;
                    i += 2; continue;
                }
            }
            *had_errors = 1;
            char strbuf[32];
            int slen = apply_decode_strategy(strategy, b, strbuf);
            if (slen < 0) return -1;
            if (opos + slen > outsize) return -1;
            memcpy(out + opos, strbuf, slen);
            opos += slen;
            i++;
        } else {
            *had_errors = 1;
            char strbuf[32];
            int slen = apply_decode_strategy(strategy, b, strbuf);
            if (slen < 0) return -1;
            if (opos + slen > outsize) return -1;
            memcpy(out + opos, strbuf, slen);
            opos += slen;
            i++;
        }
    }
    return opos;
}

/* GBK encode */
static int gbk_encode_fn(const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors)
{
    int opos = 0, i = 0;
    *had_errors = 0;

    while (i < inlen) {
        int consumed;
        uint32_t cp = charconv_utf8_decode(in + i, inlen - i, &consumed);
        if (consumed == 0) consumed = 1;
        if (cp == 0xFFFFFFFF) { i += consumed; continue; }

        if (cp <= 0x7F) {
            if (opos >= outsize) return -1;
            out[opos++] = (unsigned char)cp;
            i += consumed; continue;
        }

        int pointer = cjk_encode_lookup(gb18030_encode, GB18030_ENCODE_SIZE, cp);
        if (pointer >= 0) {
            int lead = pointer / 190 + 0x81;
            int trail_idx = pointer % 190;
            int trail = trail_idx + ((trail_idx < 0x3F) ? 0x40 : 0x41);
            if (opos + 2 > outsize) return -1;
            out[opos++] = (unsigned char)lead;
            out[opos++] = (unsigned char)trail;
            i += consumed; continue;
        }

        *had_errors = 1;
        char strbuf[64];
        int slen = apply_encode_strategy(strategy, cp, in + i, consumed, strbuf);
        if (slen < 0) return -1;
        if (opos + slen > outsize) return -1;
        memcpy(out + opos, strbuf, slen);
        opos += slen;
        i += consumed;
    }
    return opos;
}

/* ===== GB18030 decode (GBK + four-byte sequences) ===== */
static uint32_t gb18030_ranges_lookup(uint32_t pointer) {
    /* Binary search in ranges for the codepoint */
    int lo = 0, hi = GB18030_RANGES_SIZE - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (mid + 1 < GB18030_RANGES_SIZE && pointer >= gb18030_ranges[mid].pointer &&
            pointer < gb18030_ranges[mid + 1].pointer) {
            return gb18030_ranges[mid].codepoint + (pointer - gb18030_ranges[mid].pointer);
        }
        if (pointer < gb18030_ranges[mid].pointer) hi = mid - 1;
        else lo = mid + 1;
    }
    /* Check last range */
    if (hi >= 0 && pointer >= gb18030_ranges[hi].pointer) {
        return gb18030_ranges[hi].codepoint + (pointer - gb18030_ranges[hi].pointer);
    }
    return 0xFFFFFFFF;
}

static int gb18030_decode_fn(const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors)
{
    int opos = 0, i = 0;
    *had_errors = 0;

    while (i < inlen) {
        unsigned char b = in[i];
        if (b <= 0x7F) {
            if (opos >= outsize) return -1;
            out[opos++] = b;
            i++;
        } else if (b >= 0x81 && b <= 0xFE) {
            /* Check for four-byte sequence */
            if (i + 3 < inlen && in[i+1] >= 0x30 && in[i+1] <= 0x39 &&
                in[i+2] >= 0x81 && in[i+2] <= 0xFE &&
                in[i+3] >= 0x30 && in[i+3] <= 0x39) {
                uint32_t pointer = ((uint32_t)(b - 0x81) * 10 + (in[i+1] - 0x30)) * 1260 +
                                   (uint32_t)(in[i+2] - 0x81) * 10 + (in[i+3] - 0x30);
                uint32_t cp = gb18030_ranges_lookup(pointer);
                if (cp != 0xFFFFFFFF && cp <= 0x10FFFF) {
                    unsigned char buf[4];
                    int n = charconv_utf8_encode(cp, buf);
                    if (opos + n > outsize) return -1;
                    memcpy(out + opos, buf, n);
                    opos += n;
                    i += 4;
                    continue;
                }
            }
            /* Try two-byte (GBK) */
            if (i + 1 < inlen) {
                unsigned char trail = in[i+1];
                int offset = (trail < 0x7F) ? 0x40 : 0x41;
                if ((trail >= 0x40 && trail <= 0x7E) || (trail >= 0x80 && trail <= 0xFE)) {
                    int pointer = (b - 0x81) * 190 + trail - offset;
                    if (pointer >= 0 && pointer < GB18030_DECODE_SIZE && gb18030_decode[pointer] != 0) {
                        uint32_t cp = gb18030_decode[pointer];
                        unsigned char buf[4];
                        int n = charconv_utf8_encode(cp, buf);
                        if (opos + n > outsize) return -1;
                        memcpy(out + opos, buf, n);
                        opos += n;
                        i += 2;
                        continue;
                    }
                }
            }
            *had_errors = 1;
            char strbuf[32];
            int slen = apply_decode_strategy(strategy, b, strbuf);
            if (slen < 0) return -1;
            if (opos + slen > outsize) return -1;
            memcpy(out + opos, strbuf, slen);
            opos += slen;
            i++;
        } else {
            *had_errors = 1;
            char strbuf[32];
            int slen = apply_decode_strategy(strategy, b, strbuf);
            if (slen < 0) return -1;
            if (opos + slen > outsize) return -1;
            memcpy(out + opos, strbuf, slen);
            opos += slen;
            i++;
        }
    }
    return opos;
}

/* GB18030 encode */
static int gb18030_encode_fn(const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors)
{
    int opos = 0, i = 0;
    *had_errors = 0;

    while (i < inlen) {
        int consumed;
        uint32_t cp = charconv_utf8_decode(in + i, inlen - i, &consumed);
        if (consumed == 0) consumed = 1;
        if (cp == 0xFFFFFFFF) { i += consumed; continue; }

        if (cp <= 0x7F) {
            if (opos >= outsize) return -1;
            out[opos++] = (unsigned char)cp;
            i += consumed; continue;
        }

        /* Try two-byte (GBK table) */
        int pointer = cjk_encode_lookup(gb18030_encode, GB18030_ENCODE_SIZE, cp);
        if (pointer >= 0) {
            int lead = pointer / 190 + 0x81;
            int trail_idx = pointer % 190;
            int trail = trail_idx + ((trail_idx < 0x3F) ? 0x40 : 0x41);
            if (opos + 2 > outsize) return -1;
            out[opos++] = (unsigned char)lead;
            out[opos++] = (unsigned char)trail;
            i += consumed; continue;
        }

        /* Try four-byte (ranges) */
        int found = 0;
        for (int r = GB18030_RANGES_SIZE - 1; r >= 0; r--) {
            if (cp >= gb18030_ranges[r].codepoint) {
                uint32_t offset = cp - gb18030_ranges[r].codepoint;
                uint32_t ptr = gb18030_ranges[r].pointer + offset;
                int b4 = ptr % 10; ptr /= 10;
                int b3 = ptr % 126; ptr /= 126;
                int b2 = ptr % 10; ptr /= 10;
                int b1 = ptr;
                if (opos + 4 > outsize) return -1;
                out[opos++] = (unsigned char)(b1 + 0x81);
                out[opos++] = (unsigned char)(b2 + 0x30);
                out[opos++] = (unsigned char)(b3 + 0x81);
                out[opos++] = (unsigned char)(b4 + 0x30);
                found = 1;
                break;
            }
        }
        if (found) { i += consumed; continue; }

        *had_errors = 1;
        char strbuf[64];
        int slen = apply_encode_strategy(strategy, cp, in + i, consumed, strbuf);
        if (slen < 0) return -1;
        if (opos + slen > outsize) return -1;
        memcpy(out + opos, strbuf, slen);
        opos += slen;
        i += consumed;
    }
    return opos;
}

/* ===== Big5 decode ===== */
static int big5_decode_fn(const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors)
{
    int opos = 0, i = 0;
    *had_errors = 0;

    while (i < inlen) {
        unsigned char b = in[i];
        if (b <= 0x7F) {
            if (opos >= outsize) return -1;
            out[opos++] = b;
            i++;
        } else if (b >= 0x81 && b <= 0xFE) {
            if (i + 1 >= inlen) {
                *had_errors = 1;
                char strbuf[32];
                int slen = apply_decode_strategy(strategy, b, strbuf);
                if (slen < 0) return -1;
                if (opos + slen > outsize) return -1;
                memcpy(out + opos, strbuf, slen);
                opos += slen;
                i++; continue;
            }
            unsigned char trail = in[i+1];
            int offset = (trail < 0x7F) ? 0x40 : 0x62;
            if ((trail >= 0x40 && trail <= 0x7E) || (trail >= 0xA1 && trail <= 0xFE)) {
                int pointer = (b - 0x81) * 157 + trail - offset;
                if (pointer >= 0 && pointer < BIG5_DECODE_SIZE && big5_decode[pointer] != 0) {
                    uint32_t cp = big5_decode[pointer];
                    unsigned char buf[4];
                    int n = charconv_utf8_encode(cp, buf);
                    if (opos + n > outsize) return -1;
                    memcpy(out + opos, buf, n);
                    opos += n;
                    i += 2; continue;
                }
            }
            *had_errors = 1;
            char strbuf[32];
            int slen = apply_decode_strategy(strategy, b, strbuf);
            if (slen < 0) return -1;
            if (opos + slen > outsize) return -1;
            memcpy(out + opos, strbuf, slen);
            opos += slen;
            i++;
        } else {
            *had_errors = 1;
            char strbuf[32];
            int slen = apply_decode_strategy(strategy, b, strbuf);
            if (slen < 0) return -1;
            if (opos + slen > outsize) return -1;
            memcpy(out + opos, strbuf, slen);
            opos += slen;
            i++;
        }
    }
    return opos;
}

/* Big5 encode */
static int big5_encode_fn(const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors)
{
    int opos = 0, i = 0;
    *had_errors = 0;

    while (i < inlen) {
        int consumed;
        uint32_t cp = charconv_utf8_decode(in + i, inlen - i, &consumed);
        if (consumed == 0) consumed = 1;
        if (cp == 0xFFFFFFFF) { i += consumed; continue; }

        if (cp <= 0x7F) {
            if (opos >= outsize) return -1;
            out[opos++] = (unsigned char)cp;
            i += consumed; continue;
        }

        int pointer = cjk_encode_lookup(big5_encode, BIG5_ENCODE_SIZE, cp);
        if (pointer >= 0) {
            int lead = pointer / 157 + 0x81;
            int trail_idx = pointer % 157;
            int trail = (trail_idx < 0x3F) ? (trail_idx + 0x40) : (trail_idx + 0x62);
            if (opos + 2 > outsize) return -1;
            out[opos++] = (unsigned char)lead;
            out[opos++] = (unsigned char)trail;
            i += consumed; continue;
        }

        *had_errors = 1;
        char strbuf[64];
        int slen = apply_encode_strategy(strategy, cp, in + i, consumed, strbuf);
        if (slen < 0) return -1;
        if (opos + slen > outsize) return -1;
        memcpy(out + opos, strbuf, slen);
        opos += slen;
        i += consumed;
    }
    return opos;
}

/* ===== EUC-KR decode ===== */
static int euckr_decode_fn(const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors)
{
    int opos = 0, i = 0;
    *had_errors = 0;

    while (i < inlen) {
        unsigned char b = in[i];
        if (b <= 0x7F) {
            if (opos >= outsize) return -1;
            out[opos++] = b;
            i++;
        } else if (b >= 0x81 && b <= 0xFE) {
            if (i + 1 >= inlen) {
                *had_errors = 1;
                char strbuf[32];
                int slen = apply_decode_strategy(strategy, b, strbuf);
                if (slen < 0) return -1;
                if (opos + slen > outsize) return -1;
                memcpy(out + opos, strbuf, slen);
                opos += slen;
                i++; continue;
            }
            unsigned char trail = in[i+1];
            if (trail >= 0x41 && trail <= 0xFE) {
                int pointer = (b - 0x81) * 190 + trail - 0x41;
                if (pointer >= 0 && pointer < EUC_KR_DECODE_SIZE && euc_kr_decode[pointer] != 0) {
                    uint32_t cp = euc_kr_decode[pointer];
                    unsigned char buf[4];
                    int n = charconv_utf8_encode(cp, buf);
                    if (opos + n > outsize) return -1;
                    memcpy(out + opos, buf, n);
                    opos += n;
                    i += 2; continue;
                }
            }
            *had_errors = 1;
            char strbuf[32];
            int slen = apply_decode_strategy(strategy, b, strbuf);
            if (slen < 0) return -1;
            if (opos + slen > outsize) return -1;
            memcpy(out + opos, strbuf, slen);
            opos += slen;
            i++;
        } else {
            *had_errors = 1;
            char strbuf[32];
            int slen = apply_decode_strategy(strategy, b, strbuf);
            if (slen < 0) return -1;
            if (opos + slen > outsize) return -1;
            memcpy(out + opos, strbuf, slen);
            opos += slen;
            i++;
        }
    }
    return opos;
}

/* EUC-KR encode */
static int euckr_encode_fn(const unsigned char *in, int inlen,
    unsigned char *out, int outsize, int strategy, int *had_errors)
{
    int opos = 0, i = 0;
    *had_errors = 0;

    while (i < inlen) {
        int consumed;
        uint32_t cp = charconv_utf8_decode(in + i, inlen - i, &consumed);
        if (consumed == 0) consumed = 1;
        if (cp == 0xFFFFFFFF) { i += consumed; continue; }

        if (cp <= 0x7F) {
            if (opos >= outsize) return -1;
            out[opos++] = (unsigned char)cp;
            i += consumed; continue;
        }

        int pointer = cjk_encode_lookup(euc_kr_encode, EUC_KR_ENCODE_SIZE, cp);
        if (pointer >= 0) {
            int lead = pointer / 190 + 0x81;
            int trail = pointer % 190 + 0x41;
            if (opos + 2 > outsize) return -1;
            out[opos++] = (unsigned char)lead;
            out[opos++] = (unsigned char)trail;
            i += consumed; continue;
        }

        *had_errors = 1;
        char strbuf[64];
        int slen = apply_encode_strategy(strategy, cp, in + i, consumed, strbuf);
        if (slen < 0) return -1;
        if (opos + slen > outsize) return -1;
        memcpy(out + opos, strbuf, slen);
        opos += slen;
        i += consumed;
    }
    return opos;
}

/* ===== Main dispatch: charconv_decode ===== */
int charconv_decode(const struct CharEncoding *enc,
    const unsigned char *in, int inlen,
    unsigned char *out, int outsize,
    int strategy, int *had_errors)
{
    switch (enc->type) {
    case ENC_TYPE_SINGLE_BYTE:
        return sb_decode(enc, in, inlen, out, outsize, strategy, had_errors);
    case ENC_TYPE_UTF8:
        return utf8_decode_conv(in, inlen, out, outsize, strategy, had_errors);
    case ENC_TYPE_UTF7:
        return utf7_decode(in, inlen, out, outsize, strategy, had_errors);
    case ENC_TYPE_UTF16:
        return utf16_decode_bom(in, inlen, out, outsize, strategy, had_errors);
    case ENC_TYPE_UTF16BE:
        return utf16_decode_impl(in, inlen, out, outsize, strategy, had_errors, 1);
    case ENC_TYPE_UTF16LE:
        return utf16_decode_impl(in, inlen, out, outsize, strategy, had_errors, 0);
    case ENC_TYPE_UTF32:
        return utf32_decode_bom(in, inlen, out, outsize, strategy, had_errors);
    case ENC_TYPE_UTF32BE:
        return utf32_decode_impl(in, inlen, out, outsize, strategy, had_errors, 1);
    case ENC_TYPE_UTF32LE:
        return utf32_decode_impl(in, inlen, out, outsize, strategy, had_errors, 0);
    case ENC_TYPE_CESU8:
        return cesu8_decode(in, inlen, out, outsize, strategy, had_errors);
    case ENC_TYPE_SHIFT_JIS:
        return shiftjis_decode(in, inlen, out, outsize, strategy, had_errors);
    case ENC_TYPE_EUC_JP:
        return eucjp_decode(in, inlen, out, outsize, strategy, had_errors);
    case ENC_TYPE_ISO2022JP:
        return iso2022jp_decode(in, inlen, out, outsize, strategy, had_errors);
    case ENC_TYPE_GBK:
        return gbk_decode_fn(in, inlen, out, outsize, strategy, had_errors);
    case ENC_TYPE_GB18030:
        return gb18030_decode_fn(in, inlen, out, outsize, strategy, had_errors);
    case ENC_TYPE_BIG5:
        return big5_decode_fn(in, inlen, out, outsize, strategy, had_errors);
    case ENC_TYPE_EUC_KR:
        return euckr_decode_fn(in, inlen, out, outsize, strategy, had_errors);
    default:
        return -1;
    }
}

/* ===== Main dispatch: charconv_encode ===== */
int charconv_encode(const struct CharEncoding *enc,
    const unsigned char *in, int inlen,
    unsigned char *out, int outsize,
    int strategy, int *had_errors)
{
    switch (enc->type) {
    case ENC_TYPE_SINGLE_BYTE:
        return sb_encode(enc, in, inlen, out, outsize, strategy, had_errors);
    case ENC_TYPE_UTF8:
        return utf8_encode_conv(in, inlen, out, outsize, strategy, had_errors);
    case ENC_TYPE_UTF7:
        return utf7_encode(in, inlen, out, outsize, strategy, had_errors);
    case ENC_TYPE_UTF16:
        return utf16_encode_bom(in, inlen, out, outsize, strategy, had_errors);
    case ENC_TYPE_UTF16BE:
        return utf16_encode_impl(in, inlen, out, outsize, strategy, had_errors, 1);
    case ENC_TYPE_UTF16LE:
        return utf16_encode_impl(in, inlen, out, outsize, strategy, had_errors, 0);
    case ENC_TYPE_UTF32:
        return utf32_encode_bom(in, inlen, out, outsize, strategy, had_errors);
    case ENC_TYPE_UTF32BE:
        return utf32_encode_impl(in, inlen, out, outsize, strategy, had_errors, 1);
    case ENC_TYPE_UTF32LE:
        return utf32_encode_impl(in, inlen, out, outsize, strategy, had_errors, 0);
    case ENC_TYPE_CESU8:
        return cesu8_encode(in, inlen, out, outsize, strategy, had_errors);
    case ENC_TYPE_SHIFT_JIS:
        return shiftjis_encode(in, inlen, out, outsize, strategy, had_errors);
    case ENC_TYPE_EUC_JP:
        return eucjp_encode(in, inlen, out, outsize, strategy, had_errors);
    case ENC_TYPE_ISO2022JP:
        return iso2022jp_encode(in, inlen, out, outsize, strategy, had_errors);
    case ENC_TYPE_GBK:
        return gbk_encode_fn(in, inlen, out, outsize, strategy, had_errors);
    case ENC_TYPE_GB18030:
        return gb18030_encode_fn(in, inlen, out, outsize, strategy, had_errors);
    case ENC_TYPE_BIG5:
        return big5_encode_fn(in, inlen, out, outsize, strategy, had_errors);
    case ENC_TYPE_EUC_KR:
        return euckr_encode_fn(in, inlen, out, outsize, strategy, had_errors);
    default:
        return -1;
    }
}
