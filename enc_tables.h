/*
 * enc_tables.h -- Encoding registry (106 encodings matching Rust version)
 *
 * Each encoding entry embeds a CharEncoding struct (for charconv API)
 * plus aliases and runtime availability flag.
 *
 * Single-byte tables are in sb_tables.h (auto-generated from Rust source).
 * CJK tables are in cjk_data.h (auto-generated from WHATWG indexes).
 */

#ifndef ENC_TABLES_H
#define ENC_TABLES_H

#include <stddef.h>
#include "charconv.h"
#include "sb_tables.h"

#define MAX_ALIASES 8

struct Encoding {
    struct CharEncoding enc;       /* name, type, to_unicode, reverse_map, etc. */
    const char *aliases[MAX_ALIASES];
    int available;                 /* Set at runtime: 1=usable, 0=not */
};

/* ===== Encoding registry: 106 encodings ===== */
/* Order matches Rust encforce exactly:
 *   1-89:   Single-byte (ASCII, ISO-8859, Windows, DOS, DOS-DOC, KOI8, Mac, EBCDIC, Legacy)
 *   90-98:  UTF (UTF-8, UTF-7, UTF-16/BE/LE, UTF-32/BE/LE, CESU-8)
 *   99-106: CJK (GBK, GB18030, GB2312, Big5, Shift_JIS, EUC-JP, ISO-2022-JP, EUC-KR)
 */

static struct Encoding encodings[] = {
    /* === 1. ASCII === */
    { {"ASCII", ENC_TYPE_SINGLE_BYTE, ascii_to_unicode, NULL, 0, 1},
      {"US-ASCII", "ANSI_X3.4-1968", "iso-ir-6", "csASCII", "us-ascii", "646", NULL}, 0 },

    /* === 2-16. ISO 8859 family === */
    { {"ISO-8859-1", ENC_TYPE_SINGLE_BYTE, iso_8859_1_to_unicode, NULL, 0, 1},
      {"Latin-1", "latin1", "iso-ir-100", "csISOLatin1", "l1", "IBM819", "iso88591", NULL}, 0 },
    { {"ISO-8859-2", ENC_TYPE_SINGLE_BYTE, iso_8859_2_to_unicode, NULL, 0, 1},
      {"Latin-2", "latin2", "iso-ir-101", "csISOLatin2", "l2", "iso88592", NULL}, 0 },
    { {"ISO-8859-3", ENC_TYPE_SINGLE_BYTE, iso_8859_3_to_unicode, NULL, 0, 1},
      {"Latin-3", "latin3", "iso-ir-109", "csISOLatin3", "l3", "iso88593", NULL}, 0 },
    { {"ISO-8859-4", ENC_TYPE_SINGLE_BYTE, iso_8859_4_to_unicode, NULL, 0, 1},
      {"Latin-4", "latin4", "iso-ir-110", "csISOLatin4", "l4", "iso88594", NULL}, 0 },
    { {"ISO-8859-5", ENC_TYPE_SINGLE_BYTE, iso_8859_5_to_unicode, NULL, 0, 1},
      {"Cyrillic", "iso-ir-144", "csISOLatinCyrillic", "iso88595", NULL}, 0 },
    { {"ISO-8859-6", ENC_TYPE_SINGLE_BYTE, iso_8859_6_to_unicode, NULL, 0, 1},
      {"Arabic", "iso-ir-127", "csISOLatinArabic", "ECMA-114", "ASMO-708", "iso88596", NULL}, 0 },
    { {"ISO-8859-7", ENC_TYPE_SINGLE_BYTE, iso_8859_7_to_unicode, NULL, 0, 1},
      {"Greek", "iso-ir-126", "csISOLatinGreek", "ECMA-118", "ELOT_928", "iso88597", NULL}, 0 },
    { {"ISO-8859-8", ENC_TYPE_SINGLE_BYTE, iso_8859_8_to_unicode, NULL, 0, 1},
      {"Hebrew", "iso-ir-138", "csISOLatinHebrew", "iso88598", NULL}, 0 },
    { {"ISO-8859-9", ENC_TYPE_SINGLE_BYTE, iso_8859_9_to_unicode, NULL, 0, 1},
      {"Latin-5", "latin5", "iso-ir-148", "csISOLatin5", "l5", "Turkish", "iso88599", NULL}, 0 },
    { {"ISO-8859-10", ENC_TYPE_SINGLE_BYTE, iso_8859_10_to_unicode, NULL, 0, 1},
      {"Latin-6", "latin6", "iso-ir-157", "csISOLatin6", "l6", "Nordic", "iso885910", NULL}, 0 },
    { {"ISO-8859-11", ENC_TYPE_SINGLE_BYTE, iso_8859_11_to_unicode, NULL, 0, 1},
      {"Thai", "TIS-620", "windows-874", "CP874", "iso885911", NULL}, 0 },
    { {"ISO-8859-13", ENC_TYPE_SINGLE_BYTE, iso_8859_13_to_unicode, NULL, 0, 1},
      {"Latin-7", "latin7", "Baltic", "iso885913", NULL}, 0 },
    { {"ISO-8859-14", ENC_TYPE_SINGLE_BYTE, iso_8859_14_to_unicode, NULL, 0, 1},
      {"Latin-8", "latin8", "iso-ir-199", "Celtic", "iso-celtic", "iso885914", NULL}, 0 },
    { {"ISO-8859-15", ENC_TYPE_SINGLE_BYTE, iso_8859_15_to_unicode, NULL, 0, 1},
      {"Latin-9", "latin9", "latin0", "iso-ir-203", "csISOLatin9", "iso885915", NULL}, 0 },
    { {"ISO-8859-16", ENC_TYPE_SINGLE_BYTE, iso_8859_16_to_unicode, NULL, 0, 1},
      {"Latin-10", "latin10", "iso-ir-226", "Romanian", "iso885916", NULL}, 0 },

    /* === 17-25. Windows code pages === */
    { {"Windows-1250", ENC_TYPE_SINGLE_BYTE, windows_1250_to_unicode, NULL, 0, 1},
      {"CP1250", "cp1250", "x-cp1250", "win1250", "windows1250", NULL}, 0 },
    { {"Windows-1251", ENC_TYPE_SINGLE_BYTE, windows_1251_to_unicode, NULL, 0, 1},
      {"CP1251", "cp1251", "x-cp1251", "win1251", "windows1251", NULL}, 0 },
    { {"Windows-1252", ENC_TYPE_SINGLE_BYTE, windows_1252_to_unicode, NULL, 0, 1},
      {"CP1252", "cp1252", "x-cp1252", "win1252", "windows1252", NULL}, 0 },
    { {"Windows-1253", ENC_TYPE_SINGLE_BYTE, windows_1253_to_unicode, NULL, 0, 1},
      {"CP1253", "cp1253", "x-cp1253", "win1253", "windows1253", NULL}, 0 },
    { {"Windows-1254", ENC_TYPE_SINGLE_BYTE, windows_1254_to_unicode, NULL, 0, 1},
      {"CP1254", "cp1254", "x-cp1254", "win1254", "windows1254", NULL}, 0 },
    { {"Windows-1255", ENC_TYPE_SINGLE_BYTE, windows_1255_to_unicode, NULL, 0, 1},
      {"CP1255", "cp1255", "x-cp1255", "win1255", "windows1255", NULL}, 0 },
    { {"Windows-1256", ENC_TYPE_SINGLE_BYTE, windows_1256_to_unicode, NULL, 0, 1},
      {"CP1256", "cp1256", "x-cp1256", "win1256", "windows1256", NULL}, 0 },
    { {"Windows-1257", ENC_TYPE_SINGLE_BYTE, windows_1257_to_unicode, NULL, 0, 1},
      {"CP1257", "cp1257", "x-cp1257", "win1257", "windows1257", NULL}, 0 },
    { {"Windows-1258", ENC_TYPE_SINGLE_BYTE, windows_1258_to_unicode, NULL, 0, 1},
      {"CP1258", "cp1258", "x-cp1258", "win1258", "windows1258", NULL}, 0 },

    /* === 26-41. DOS code pages (standard) === */
    { {"CP437", ENC_TYPE_SINGLE_BYTE, cp437_to_unicode, NULL, 0, 1},
      {"IBM437", "ibm437", "437", "csPC8CodePage437", "DOS-US", NULL}, 0 },
    { {"CP437-DOC", ENC_TYPE_SINGLE_BYTE, cp437_doc_to_unicode, NULL, 0, 0},
      {"IBM437-DOC", "437-DOC", "DOS-US-DOC", NULL}, 0 },
    { {"CP850", ENC_TYPE_SINGLE_BYTE, cp850_to_unicode, NULL, 0, 1},
      {"IBM850", "ibm850", "850", "csPC850Multilingual", "DOS-Latin-1", NULL}, 0 },
    { {"CP852", ENC_TYPE_SINGLE_BYTE, cp852_to_unicode, NULL, 0, 1},
      {"IBM852", "ibm852", "852", "DOS-Latin-2", NULL}, 0 },
    { {"CP866", ENC_TYPE_SINGLE_BYTE, cp866_to_unicode, NULL, 0, 1},
      {"IBM866", "ibm866", "866", "csIBM866", "DOS-Cyrillic", NULL}, 0 },
    { {"CP737", ENC_TYPE_SINGLE_BYTE, cp737_to_unicode, NULL, 0, 1},
      {"IBM737", "ibm737", "737", "DOS-Greek", NULL}, 0 },
    { {"CP855", ENC_TYPE_SINGLE_BYTE, cp855_to_unicode, NULL, 0, 1},
      {"IBM855", "ibm855", "855", "csIBM855", NULL}, 0 },
    { {"CP857", ENC_TYPE_SINGLE_BYTE, cp857_to_unicode, NULL, 0, 1},
      {"IBM857", "ibm857", "857", "csIBM857", "DOS-Turkish", NULL}, 0 },
    { {"CP865", ENC_TYPE_SINGLE_BYTE, cp865_to_unicode, NULL, 0, 1},
      {"IBM865", "ibm865", "865", "csIBM865", "DOS-Nordic", NULL}, 0 },
    { {"CP858", ENC_TYPE_SINGLE_BYTE, cp858_to_unicode, NULL, 0, 1},
      {"IBM858", "ibm858", "858", NULL}, 0 },
    { {"CP860", ENC_TYPE_SINGLE_BYTE, cp860_to_unicode, NULL, 0, 1},
      {"IBM860", "ibm860", "860", "csIBM860", "DOS-Portuguese", NULL}, 0 },
    { {"CP861", ENC_TYPE_SINGLE_BYTE, cp861_to_unicode, NULL, 0, 1},
      {"IBM861", "ibm861", "861", "csIBM861", "DOS-Icelandic", NULL}, 0 },
    { {"CP862", ENC_TYPE_SINGLE_BYTE, cp862_to_unicode, NULL, 0, 1},
      {"IBM862", "ibm862", "862", "DOS-Hebrew", NULL}, 0 },
    { {"CP863", ENC_TYPE_SINGLE_BYTE, cp863_to_unicode, NULL, 0, 1},
      {"IBM863", "ibm863", "863", "csIBM863", "DOS-Canadian-French", NULL}, 0 },
    { {"CP864", ENC_TYPE_SINGLE_BYTE, cp864_to_unicode, NULL, 0, 1},
      {"IBM864", "ibm864", "864", "csIBM864", "DOS-Arabic", NULL}, 0 },
    { {"CP869", ENC_TYPE_SINGLE_BYTE, cp869_to_unicode, NULL, 0, 1},
      {"IBM869", "ibm869", "869", "csIBM869", "DOS-Greek-2", NULL}, 0 },

    /* === 42-55. DOS code pages (DOC graphical variants) === */
    { {"CP850-DOC", ENC_TYPE_SINGLE_BYTE, cp850_doc_to_unicode, NULL, 0, 0},
      {"IBM850-DOC", "850-DOC", "DOS-Latin-1-DOC", NULL}, 0 },
    { {"CP852-DOC", ENC_TYPE_SINGLE_BYTE, cp852_doc_to_unicode, NULL, 0, 0},
      {"IBM852-DOC", "852-DOC", "DOS-Latin-2-DOC", NULL}, 0 },
    { {"CP866-DOC", ENC_TYPE_SINGLE_BYTE, cp866_doc_to_unicode, NULL, 0, 0},
      {"IBM866-DOC", "866-DOC", "DOS-Cyrillic-DOC", NULL}, 0 },
    { {"CP737-DOC", ENC_TYPE_SINGLE_BYTE, cp737_doc_to_unicode, NULL, 0, 0},
      {"IBM737-DOC", "737-DOC", "DOS-Greek-DOC", NULL}, 0 },
    { {"CP855-DOC", ENC_TYPE_SINGLE_BYTE, cp855_doc_to_unicode, NULL, 0, 0},
      {"IBM855-DOC", "855-DOC", NULL}, 0 },
    { {"CP857-DOC", ENC_TYPE_SINGLE_BYTE, cp857_doc_to_unicode, NULL, 0, 0},
      {"IBM857-DOC", "857-DOC", "DOS-Turkish-DOC", NULL}, 0 },
    { {"CP865-DOC", ENC_TYPE_SINGLE_BYTE, cp865_doc_to_unicode, NULL, 0, 0},
      {"IBM865-DOC", "865-DOC", "DOS-Nordic-DOC", NULL}, 0 },
    { {"CP858-DOC", ENC_TYPE_SINGLE_BYTE, cp858_doc_to_unicode, NULL, 0, 0},
      {"IBM858-DOC", "858-DOC", NULL}, 0 },
    { {"CP860-DOC", ENC_TYPE_SINGLE_BYTE, cp860_doc_to_unicode, NULL, 0, 0},
      {"IBM860-DOC", "860-DOC", "DOS-Portuguese-DOC", NULL}, 0 },
    { {"CP861-DOC", ENC_TYPE_SINGLE_BYTE, cp861_doc_to_unicode, NULL, 0, 0},
      {"IBM861-DOC", "861-DOC", "DOS-Icelandic-DOC", NULL}, 0 },
    { {"CP862-DOC", ENC_TYPE_SINGLE_BYTE, cp862_doc_to_unicode, NULL, 0, 0},
      {"IBM862-DOC", "862-DOC", "DOS-Hebrew-DOC", NULL}, 0 },
    { {"CP863-DOC", ENC_TYPE_SINGLE_BYTE, cp863_doc_to_unicode, NULL, 0, 0},
      {"IBM863-DOC", "863-DOC", "DOS-Canadian-French-DOC", NULL}, 0 },
    { {"CP864-DOC", ENC_TYPE_SINGLE_BYTE, cp864_doc_to_unicode, NULL, 0, 0},
      {"IBM864-DOC", "864-DOC", "DOS-Arabic-DOC", NULL}, 0 },
    { {"CP869-DOC", ENC_TYPE_SINGLE_BYTE, cp869_doc_to_unicode, NULL, 0, 0},
      {"IBM869-DOC", "869-DOC", "DOS-Greek-2-DOC", NULL}, 0 },

    /* === 56-57. KOI8 === */
    { {"KOI8-R", ENC_TYPE_SINGLE_BYTE, koi8_r_to_unicode, NULL, 0, 1},
      {"koi8-r", "koi8r", "csKOI8R", NULL}, 0 },
    { {"KOI8-U", ENC_TYPE_SINGLE_BYTE, koi8_u_to_unicode, NULL, 0, 1},
      {"koi8-u", "koi8u", NULL}, 0 },

    /* === 58-68. Mac === */
    { {"MacRoman", ENC_TYPE_SINGLE_BYTE, mac_roman_to_unicode, NULL, 0, 1},
      {"macintosh", "mac", "x-mac-roman", "csMacintosh", "macroman", NULL}, 0 },
    { {"MacCyrillic", ENC_TYPE_SINGLE_BYTE, mac_cyrillic_to_unicode, NULL, 0, 1},
      {"x-mac-cyrillic", "mac-cyrillic", "maccyrillic", NULL}, 0 },
    { {"MacGreek", ENC_TYPE_SINGLE_BYTE, mac_greek_to_unicode, NULL, 0, 1},
      {"x-mac-greek", "mac-greek", "macgreek", NULL}, 0 },
    { {"MacTurkish", ENC_TYPE_SINGLE_BYTE, mac_turkish_to_unicode, NULL, 0, 1},
      {"x-mac-turkish", "mac-turkish", "macturkish", NULL}, 0 },
    { {"MacCentralEurope", ENC_TYPE_SINGLE_BYTE, mac_central_europe_to_unicode, NULL, 0, 1},
      {"x-mac-centraleurroman", "x-mac-ce", "mac-centraleurope", "macce", NULL}, 0 },
    { {"MacIcelandic", ENC_TYPE_SINGLE_BYTE, mac_icelandic_to_unicode, NULL, 0, 1},
      {"x-mac-icelandic", "mac-icelandic", "maciceland", NULL}, 0 },
    { {"MacCroatian", ENC_TYPE_SINGLE_BYTE, mac_croatian_to_unicode, NULL, 0, 1},
      {"x-mac-croatian", "mac-croatian", "maccroatian", NULL}, 0 },
    { {"MacRomanian", ENC_TYPE_SINGLE_BYTE, mac_romanian_to_unicode, NULL, 0, 1},
      {"x-mac-romanian", "mac-romanian", "macromania", NULL}, 0 },
    { {"MacArabic", ENC_TYPE_SINGLE_BYTE, mac_arabic_to_unicode, NULL, 0, 1},
      {"x-mac-arabic", "mac-arabic", "macarabic", NULL}, 0 },
    { {"MacHebrew", ENC_TYPE_SINGLE_BYTE, mac_hebrew_to_unicode, NULL, 0, 1},
      {"x-mac-hebrew", "mac-hebrew", "machebrew", NULL}, 0 },
    { {"MacThai", ENC_TYPE_SINGLE_BYTE, mac_thai_to_unicode, NULL, 0, 1},
      {"x-mac-thai", "mac-thai", "macthai", NULL}, 0 },

    /* === 69-78. EBCDIC === */
    { {"CP037", ENC_TYPE_SINGLE_BYTE, cp037_to_unicode, NULL, 0, 0},
      {"IBM037", "ebcdic-cp-us", "ebcdic-cp-ca", "csIBM037", "cp037", NULL}, 0 },
    { {"CP500", ENC_TYPE_SINGLE_BYTE, cp500_to_unicode, NULL, 0, 0},
      {"IBM500", "ebcdic-international", "ebcdic-cp-be", "csIBM500", "cp500", NULL}, 0 },
    { {"CP875", ENC_TYPE_SINGLE_BYTE, cp875_to_unicode, NULL, 0, 0},
      {"IBM875", "ebcdic-greek", "cp875", NULL}, 0 },
    { {"CP1026", ENC_TYPE_SINGLE_BYTE, cp1026_to_unicode, NULL, 0, 0},
      {"IBM1026", "ebcdic-cp-tr", "cp1026", NULL}, 0 },
    { {"CP1140", ENC_TYPE_SINGLE_BYTE, cp1140_to_unicode, NULL, 0, 0},
      {"IBM1140", "ebcdic-us-37+euro", "cp1140", NULL}, 0 },
    { {"CP1141", ENC_TYPE_SINGLE_BYTE, cp1141_to_unicode, NULL, 0, 0},
      {"IBM1141", "ebcdic-de-273+euro", "cp1141", NULL}, 0 },
    { {"CP1142", ENC_TYPE_SINGLE_BYTE, cp1142_to_unicode, NULL, 0, 0},
      {"IBM1142", "ebcdic-dk-277+euro", "ebcdic-no-277+euro", "cp1142", NULL}, 0 },
    { {"CP1143", ENC_TYPE_SINGLE_BYTE, cp1143_to_unicode, NULL, 0, 0},
      {"IBM1143", "ebcdic-fi-278+euro", "ebcdic-se-278+euro", "cp1143", NULL}, 0 },
    { {"CP1144", ENC_TYPE_SINGLE_BYTE, cp1144_to_unicode, NULL, 0, 0},
      {"IBM1144", "ebcdic-it-280+euro", "cp1144", NULL}, 0 },
    { {"CP1145", ENC_TYPE_SINGLE_BYTE, cp1145_to_unicode, NULL, 0, 0},
      {"IBM1145", "ebcdic-es-284+euro", "cp1145", NULL}, 0 },

    /* === 79-89. Legacy === */
    { {"HP-Roman8", ENC_TYPE_SINGLE_BYTE, hp_roman8_to_unicode, NULL, 0, 1},
      {"hp-roman8", "roman8", "r8", "csHPRoman8", "hproman8", NULL}, 0 },
    { {"DEC-MCS", ENC_TYPE_SINGLE_BYTE, dec_mcs_to_unicode, NULL, 0, 1},
      {"dec-mcs", "csDECMCS", "dec", NULL}, 0 },
    { {"JIS_X0201", ENC_TYPE_SINGLE_BYTE, jis_x0201_to_unicode, NULL, 0, 1},
      {"JIS_X0201-1976", "x0201", "csHalfWidthKatakana", NULL}, 0 },
    { {"KZ-1048", ENC_TYPE_SINGLE_BYTE, kz_1048_to_unicode, NULL, 0, 1},
      {"STRK1048-2002", "RK1048", "csKZ1048", "kz1048", NULL}, 0 },
    { {"GSM-03.38", ENC_TYPE_SINGLE_BYTE, gsm_03_38_to_unicode, NULL, 0, 0},
      {"GSM", "gsm-default-alphabet", "gsm7", "gsm0338", NULL}, 0 },
    { {"VISCII", ENC_TYPE_SINGLE_BYTE, viscii_to_unicode, NULL, 0, 1},
      {"viscii", "csVISCII", "viscii1.1-1", NULL}, 0 },
    { {"ATASCII", ENC_TYPE_SINGLE_BYTE, atascii_to_unicode, NULL, 0, 0},
      {"atascii", "atari-ascii", "atari", NULL}, 0 },
    { {"PETSCII", ENC_TYPE_SINGLE_BYTE, petscii_to_unicode, NULL, 0, 0},
      {"petscii", "commodore", "c64", NULL}, 0 },
    { {"Adobe-Standard-Encoding", ENC_TYPE_SINGLE_BYTE, adobe_standard_to_unicode, NULL, 0, 1},
      {"adobe-standard", "csAdobeStandardEncoding", NULL}, 0 },
    { {"Adobe-Symbol-Encoding", ENC_TYPE_SINGLE_BYTE, adobe_symbol_to_unicode, NULL, 0, 0},
      {"adobe-symbol", "symbol", NULL}, 0 },
    { {"T.61-8bit", ENC_TYPE_SINGLE_BYTE, t61_to_unicode, NULL, 0, 1},
      {"T.61", "iso-ir-102", "csISO102T617bit", "t61", NULL}, 0 },

    /* === 90-98. UTF encodings === */
    { {"UTF-8", ENC_TYPE_UTF8, NULL, NULL, 0, 1},
      {"utf8", "utf-8", NULL}, 0 },
    { {"UTF-7", ENC_TYPE_UTF7, NULL, NULL, 0, 1},
      {"utf7", "utf-7", NULL}, 0 },
    { {"UTF-16", ENC_TYPE_UTF16, NULL, NULL, 0, 0},
      {"utf16", "utf-16", NULL}, 0 },
    { {"UTF-16BE", ENC_TYPE_UTF16BE, NULL, NULL, 0, 0},
      {"utf16be", "utf-16be", NULL}, 0 },
    { {"UTF-16LE", ENC_TYPE_UTF16LE, NULL, NULL, 0, 0},
      {"utf16le", "utf-16le", NULL}, 0 },
    { {"UTF-32", ENC_TYPE_UTF32, NULL, NULL, 0, 0},
      {"utf32", "utf-32", NULL}, 0 },
    { {"UTF-32BE", ENC_TYPE_UTF32BE, NULL, NULL, 0, 0},
      {"utf32be", "utf-32be", NULL}, 0 },
    { {"UTF-32LE", ENC_TYPE_UTF32LE, NULL, NULL, 0, 0},
      {"utf32le", "utf-32le", NULL}, 0 },
    { {"CESU-8", ENC_TYPE_CESU8, NULL, NULL, 0, 1},
      {"cesu8", "cesu-8", NULL}, 0 },

    /* === 99-106. CJK encodings === */
    { {"GBK", ENC_TYPE_GBK, NULL, NULL, 0, 1},
      {"gbk", "CP936", "MS936", "windows-936", "chinese", "cp936", NULL}, 0 },
    { {"GB18030", ENC_TYPE_GB18030, NULL, NULL, 0, 1},
      {"gb18030", "gb18030-2000", "GB18030-2005", NULL}, 0 },
    { {"GB2312", ENC_TYPE_GBK, NULL, NULL, 0, 1},
      {"gb2312", "csGB2312", "EUC-CN", "x-euc-cn", "euccn", NULL}, 0 },
    { {"Big5", ENC_TYPE_BIG5, NULL, NULL, 0, 1},
      {"big5", "csBig5", "Big5-HKSCS", "cn-big5", "x-x-big5", "cp950", NULL}, 0 },
    { {"Shift_JIS", ENC_TYPE_SHIFT_JIS, NULL, NULL, 0, 1},
      {"shift_jis", "sjis", "shift-jis", "csShiftJIS", "MS_Kanji", "CP932", "ms932", NULL}, 0 },
    { {"EUC-JP", ENC_TYPE_EUC_JP, NULL, NULL, 0, 1},
      {"euc-jp", "eucjp", "x-euc-jp", "csEUCPkdFmtJapanese", NULL}, 0 },
    { {"ISO-2022-JP", ENC_TYPE_ISO2022JP, NULL, NULL, 0, 1},
      {"iso-2022-jp", "csISO2022JP", "jis", "iso2022jp", NULL}, 0 },
    { {"EUC-KR", ENC_TYPE_EUC_KR, NULL, NULL, 0, 1},
      {"euc-kr", "euckr", "csEUCKR", "KS_C_5601-1987", "korean", "iso-ir-149", NULL}, 0 },

    { {NULL, 0, NULL, NULL, 0, 0}, {NULL}, 0 }  /* Sentinel */
};

#define NUM_ENCODINGS_MAX (sizeof(encodings) / sizeof(encodings[0]) - 1)

#endif /* ENC_TABLES_H */
