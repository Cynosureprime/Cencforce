/*
 * encforce.c -- Text encoding forensics tool
 *
 * Discovers how text was corrupted by exhaustively trying all
 * encoding/decoding transformations on input strings.
 *
 * Uses yarn.c thread pool and rling.c-style block I/O for high
 * throughput on large inputs.
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>
#include <unistd.h>
#include <strings.h>

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

#include "yarn.h"
#include "charconv.h"
#include "enc_tables.h"

/* ===== Constants ===== */
#define VERSION "1.0.0"
#define MAXCHUNK (50*1024*1024)
#define MAXLINE  (256*1024)
#define MAXLINEPERCHUNK (MAXCHUNK/2/8)
#define RINDEXSIZE MAXLINEPERCHUNK
#define OUTBUFSIZE (2*1024*1024)
#define SCRATCH_SIZE (13*MAXLINE)   /* worst case: base64_inline encode = 13:1 */
#define DEDUP_CAPACITY 8192
#define MAX_SINGLE_OUTPUT SCRATCH_SIZE

/* ===== Modes ===== */
enum Mode {
    MODE_DECODE = 1,
    MODE_ENCODE = 2,
    MODE_BOTH = 3,
    MODE_TRANSCODE = 4,
    MODE_ALL = 7
};

/* ===== Output formats ===== */
enum OutputFormat {
    FMT_LINES = 0,
    FMT_JSON = 1,
    FMT_TSV = 2
};

/* ===== Job structure ===== */
#define JOB_PROCESS 1
#define JOB_DONE 99

struct LineInfo {
    unsigned int offset;
    unsigned int len;
};

struct JOB {
    struct JOB *next;
    char *readbuf;
    struct LineInfo *readindex;
    int numline;
    int startline;
    int func;
    /* Per-thread output buffer */
    char *outbuf;
    int outlen;
    int outsize;
    /* Per-thread dedup hash table */
    uint64_t *dedup_hashes;
    int dedup_capacity;
    int dedup_count;
    /* Per-thread scratch space */
    char *scratch;
    int scratch_size;
};

/* ===== Globals ===== */
static int Num_encodings;
static int Maxt = 1;
static int Workthread;

/* Job queue - rling.c pattern */
static struct JOB *Jobs;
static struct JOB *FreeHead, **FreeTail;
static struct JOB *WorkHead, **WorkTail;
static lock *FreeWaiting, *WorkWaiting;
static lock *ReadBuf0, *ReadBuf1;
static lock *Output_lock;

/* Block I/O */
static char *Readbuf;
static struct LineInfo *Readindex;
static int Cacheindex;

/* Options */
static enum Mode OpMode = MODE_BOTH;
static enum OutputFormat OutFormat = FMT_LINES;
static int DoHex = 1;
static int DoVerbose = 0;
static int DoUnique = 1;
static int DoNoErrors = 0;
static int DoSuggest = 0;
static int MaxDepth = 1;
static char *FilterPattern __attribute__((unused)) = NULL;

/* Encoding filter lists */
static char **IncludeEncodings = NULL;
static int NumInclude = 0;
static char **ExcludeEncodings = NULL;
static int NumExclude = 0;

/* ===== Platform ===== */
static int get_nprocs(void) {
#ifdef __APPLE__
    int numCPUs;
    size_t len = sizeof(numCPUs);
    int mib[2] = { CTL_HW, HW_NCPU };
    if (sysctl(mib, 2, &numCPUs, &len, NULL, 0))
        return 1;
    return numCPUs;
#elif defined(_SC_NPROCESSORS_ONLN)
    int n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? n : 1;
#else
    return 1;
#endif
}

/* ===== findeol: find newline in buffer ===== */
static inline char *findeol(const char *s, int64_t l) {
    if (l <= 0) return NULL;
    return memchr(s, '\n', l);
}

/* ===== FNV-1a hash for dedup ===== */
static inline uint64_t fnv1a(const unsigned char *data, int len) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (int i = 0; i < len; i++) {
        h ^= data[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

/* ===== Hex utilities ===== */
static const char hexdigits[] = "0123456789abcdef";

static inline int hexval(unsigned char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Check if output bytes need $HEX[] encoding */
static int needs_hex(const unsigned char *data, int len) {
    for (int i = 0; i < len; i++)
        if (data[i] < 0x20 || data[i] > 0x7E || data[i] == ':')
            return 1;
    return 0;
}


/* ===== Dedup ===== */
static void dedup_reset(struct JOB *job) {
    job->dedup_count = 0;
    memset(job->dedup_hashes, 0, sizeof(uint64_t) * job->dedup_capacity);
}

/* Returns 1 if hash is new (inserted), 0 if already seen */
static int dedup_insert(struct JOB *job, uint64_t hash) {
    if (!DoUnique) return 1;
    int cap = job->dedup_capacity;
    int idx = (int)(hash % (uint64_t)cap);
    for (int probe = 0; probe < cap; probe++) {
        int pos = (idx + probe) % cap;
        if (job->dedup_hashes[pos] == 0) {
            job->dedup_hashes[pos] = hash;
            job->dedup_count++;
            return 1;
        }
        if (job->dedup_hashes[pos] == hash) {
            return 0;  /* Already seen */
        }
    }
    /* Table full, just accept */
    return 1;
}

/* ===== Output buffering ===== */
static void flush_output(struct JOB *job) {
    if (job->outlen == 0) return;
    possess(Output_lock);
    fwrite(job->outbuf, 1, job->outlen, stdout);
    release(Output_lock);
    job->outlen = 0;
}

static void output_append(struct JOB *job, const char *data, int len) {
    if (len <= 0) return;
    if (job->outlen + len >= job->outsize) {
        flush_output(job);
        if (len >= job->outsize) {
            /* Single line bigger than buffer: write directly */
            possess(Output_lock);
            fwrite(data, 1, len, stdout);
            release(Output_lock);
            return;
        }
    }
    memcpy(job->outbuf + job->outlen, data, len);
    job->outlen += len;
}

/* ===== Format and output one result ===== */
/* Helper: append small string to job output */
static void emit_str(struct JOB *job, const char *s) {
    output_append(job, s, strlen(s));
}

/* Helper: append data, using $HEX[] encoding if needed */
static void emit_data(struct JOB *job, const unsigned char *data, int len) {
    if (DoHex && needs_hex(data, len)) {
        char hexbuf[8192];
        /* For large data, write in chunks */
        output_append(job, "$HEX[", 5);
        int pos = 0;
        for (int i = 0; i < len; i++) {
            hexbuf[pos++] = hexdigits[data[i] >> 4];
            hexbuf[pos++] = hexdigits[data[i] & 0x0F];
            if (pos >= 8000) {
                output_append(job, hexbuf, pos);
                pos = 0;
            }
        }
        if (pos > 0) output_append(job, hexbuf, pos);
        output_append(job, "]", 1);
    } else {
        output_append(job, (const char *)data, len);
    }
}

/* Helper: append JSON-escaped string to job output */
static void emit_json_str(struct JOB *job, const unsigned char *s, int slen) {
    char buf[4096];
    int pos = 0;
    buf[pos++] = '"';
    for (int i = 0; i < slen; i++) {
        if (pos >= 4000) {
            output_append(job, buf, pos);
            pos = 0;
        }
        unsigned char c = s[i];
        if (c == '"') { buf[pos++] = '\\'; buf[pos++] = '"'; }
        else if (c == '\\') { buf[pos++] = '\\'; buf[pos++] = '\\'; }
        else if (c == '\n') { buf[pos++] = '\\'; buf[pos++] = 'n'; }
        else if (c == '\r') { buf[pos++] = '\\'; buf[pos++] = 'r'; }
        else if (c == '\t') { buf[pos++] = '\\'; buf[pos++] = 't'; }
        else if (c < 0x20) {
            pos += sprintf(buf + pos, "\\u%04X", c);
        } else {
            buf[pos++] = c;
        }
    }
    buf[pos++] = '"';
    output_append(job, buf, pos);
}

/* Helper: append hex string to job output */
static void emit_hex_str(struct JOB *job, const unsigned char *data, int len) {
    char buf[4096];
    int pos = 0;
    buf[pos++] = '"';
    for (int i = 0; i < len; i++) {
        if (pos >= 4000) {
            output_append(job, buf, pos);
            pos = 0;
        }
        buf[pos++] = hexdigits[data[i] >> 4];
        buf[pos++] = hexdigits[data[i] & 0x0F];
    }
    buf[pos++] = '"';
    output_append(job, buf, pos);
}

static void emit_result(struct JOB *job,
    const unsigned char *input, int input_len,
    const unsigned char *output, int output_len,
    const char *operation, const char *enc_name,
    const char *target_enc, const char *strategy_name,
    int had_errors, int is_first_for_line, int is_json_array)
{
    char tmp[256];
    (void)had_errors;  /* Filtering done in caller */

    switch (OutFormat) {
    case FMT_LINES:
        if (is_first_for_line && !is_json_array) {
            emit_str(job, "[input: ");
            emit_data(job, input, input_len);
            emit_str(job, "]\n");
        }
        sprintf(tmp, "  %s %s", operation, enc_name);
        emit_str(job, tmp);
        if (target_enc) {
            sprintf(tmp, " -> %s", target_enc);
            emit_str(job, tmp);
        }
        if (had_errors && strategy_name) {
            sprintf(tmp, " (%s)", strategy_name);
            emit_str(job, tmp);
        }
        emit_str(job, ": ");
        emit_data(job, output, output_len);
        output_append(job, "\n", 1);
        break;

    case FMT_JSON:
        emit_str(job, "{\"op\":");
        emit_json_str(job, (const unsigned char *)operation, strlen(operation));
        emit_str(job, ",\"enc\":");
        emit_json_str(job, (const unsigned char *)enc_name, strlen(enc_name));
        if (target_enc) {
            emit_str(job, ",\"target\":");
            emit_json_str(job, (const unsigned char *)target_enc, strlen(target_enc));
        }
        if (strategy_name) {
            emit_str(job, ",\"strategy\":");
            emit_json_str(job, (const unsigned char *)strategy_name, strlen(strategy_name));
        }
        emit_str(job, ",\"output\":");
        emit_json_str(job, output, output_len);
        emit_str(job, "}");
        break;

    case FMT_TSV:
        output_append(job, (const char *)input, input_len);
        output_append(job, "\t", 1);
        emit_hex_str(job, input, input_len);
        output_append(job, "\t", 1);
        emit_str(job, operation);
        output_append(job, "\t", 1);
        emit_str(job, enc_name);
        output_append(job, "\t", 1);
        emit_str(job, target_enc ? target_enc : "");
        output_append(job, "\t", 1);
        emit_str(job, strategy_name ? strategy_name : "");
        output_append(job, "\t", 1);
        output_append(job, (const char *)output, output_len);
        output_append(job, "\t", 1);
        emit_hex_str(job, output, output_len);
        output_append(job, "\n", 1);
        break;
    }
}

/* ===== Process one line through the transform pipeline ===== */
static void process_line(struct JOB *job, const unsigned char *input, int input_len) {
    unsigned char *scratch = (unsigned char *)job->scratch;
    int scratch_size = job->scratch_size;
    int is_utf8 = charconv_is_valid_utf8(input, input_len);
    int first_result = 1;
    int result_count = 0;

    dedup_reset(job);

    /* JSON: emit header for this input line */
    if (OutFormat == FMT_JSON) {
        emit_str(job, "{\"input\":");
        emit_json_str(job, input, input_len);
        emit_str(job, ",\"input_hex\":");
        emit_hex_str(job, input, input_len);
        emit_str(job, ",\"results\":[");
    }

    /* DECODE mode */
    if (OpMode & MODE_DECODE) {
        for (int e = 0; e < Num_encodings; e++) {
            if (!encodings[e].available) continue;

            /* Try with each strategy */
            for (int s = 0; s < DS_COUNT; s++) {
                int had_errors = 0;
                int out_len = charconv_decode(&encodings[e].enc, input, input_len,
                    scratch, scratch_size, s, &had_errors);

                if (out_len < 0) continue;

                /* If strict succeeded without errors, skip remaining strategies */
                if (s == DS_STRICT && !had_errors) {
                    /* Also skip identity results (output == input) */
                    if (out_len == input_len && memcmp(scratch, input, out_len) == 0)
                        break;

                    /* Dedup */
                    uint64_t hash = fnv1a(scratch, out_len);
                    if (!dedup_insert(job, hash)) break;

                    const char *strat_name = NULL;
                    if (OutFormat == FMT_JSON && result_count > 0)
                        output_append(job, ",", 1);
                    emit_result(job, input, input_len, scratch, out_len,
                        "decode", encodings[e].enc.name, NULL, strat_name,
                        had_errors, first_result, OutFormat == FMT_JSON);
                    first_result = 0;
                    result_count++;
                    break;
                }

                /* Filter errors if requested */
                if (DoNoErrors && had_errors) continue;

                /* Dedup */
                uint64_t hash = fnv1a(scratch, out_len);
                if (!dedup_insert(job, hash)) continue;

                /* Output */
                const char *strat_name = charconv_decode_strategy_names[s];
                if (OutFormat == FMT_JSON && result_count > 0)
                    output_append(job, ",", 1);

                emit_result(job, input, input_len, scratch, out_len,
                    "decode", encodings[e].enc.name, NULL, strat_name,
                    had_errors, first_result, OutFormat == FMT_JSON);
                first_result = 0;
                result_count++;
            }
        }
    }

    /* ENCODE mode */
    if ((OpMode & MODE_ENCODE) && is_utf8) {
        for (int e = 0; e < Num_encodings; e++) {
            if (!encodings[e].available) continue;

            for (int s = 0; s < ES_COUNT; s++) {
                int had_errors = 0;
                int out_len = charconv_encode(&encodings[e].enc, input, input_len,
                    scratch, scratch_size, s, &had_errors);

                if (out_len < 0) continue;

                /* If strict succeeded without errors, skip remaining strategies */
                if (s == ES_STRICT && !had_errors) {
                    if (out_len == input_len && memcmp(scratch, input, out_len) == 0)
                        break;

                    uint64_t hash = fnv1a(scratch, out_len);
                    if (!dedup_insert(job, hash)) break;

                    if (OutFormat == FMT_JSON && result_count > 0)
                        output_append(job, ",", 1);
                    emit_result(job, input, input_len, scratch, out_len,
                        "encode", encodings[e].enc.name, NULL, NULL,
                        had_errors, first_result, OutFormat == FMT_JSON);
                    first_result = 0;
                    result_count++;
                    break;
                }

                if (DoNoErrors && had_errors) continue;

                uint64_t hash = fnv1a(scratch, out_len);
                if (!dedup_insert(job, hash)) continue;

                const char *strat_name = charconv_encode_strategy_names[s];
                if (OutFormat == FMT_JSON && result_count > 0)
                    output_append(job, ",", 1);

                emit_result(job, input, input_len, scratch, out_len,
                    "encode", encodings[e].enc.name, NULL, strat_name,
                    had_errors, first_result, OutFormat == FMT_JSON);
                first_result = 0;
                result_count++;
            }
        }
    }

    /* TRANSCODE mode */
    if (OpMode & MODE_TRANSCODE) {
        unsigned char *mid = malloc(scratch_size);
        if (!mid) goto end_transcode;

        for (int src = 0; src < Num_encodings; src++) {
            if (!encodings[src].available) continue;

            /* Decode input as source encoding with FFFD replacement */
            int had_dec_errors = 0;
            int mid_len = charconv_decode(&encodings[src].enc, input, input_len,
                mid, scratch_size, DS_REPLACEMENT_FFFD, &had_dec_errors);
            if (mid_len < 0) continue;

            /* Re-encode decoded text into each target encoding */
            for (int tgt = 0; tgt < Num_encodings; tgt++) {
                if (tgt == src) continue;
                if (!encodings[tgt].available) continue;

                for (int s = 0; s < ES_COUNT; s++) {
                    int had_enc_errors = 0;
                    int out_len = charconv_encode(&encodings[tgt].enc, mid, mid_len,
                        scratch, scratch_size, s, &had_enc_errors);

                    if (out_len < 0) continue;

                    if (out_len == input_len && memcmp(scratch, input, out_len) == 0
                        && s == ES_STRICT)
                        continue;

                    if (DoNoErrors && (had_dec_errors || had_enc_errors)) continue;

                    uint64_t hash = fnv1a(scratch, out_len);
                    if (!dedup_insert(job, hash)) continue;

                    const char *strat_name = (s == ES_STRICT) ? NULL :
                        charconv_encode_strategy_names[s];
                    if (OutFormat == FMT_JSON && result_count > 0)
                        output_append(job, ",", 1);

                    emit_result(job, input, input_len, scratch, out_len,
                        "transcode", encodings[src].enc.name,
                        encodings[tgt].enc.name,
                        strat_name, had_dec_errors || had_enc_errors,
                        first_result, OutFormat == FMT_JSON);
                    first_result = 0;
                    result_count++;

                    if (s == ES_STRICT && !had_enc_errors) break;
                }
            }
        }
        free(mid);
    }
end_transcode:

    /* Close JSON array for this line */
    if (OutFormat == FMT_JSON) {
        output_append(job, "]}\n", 3);
    }
}

/* ===== Worker thread function ===== */
static void procjob(void *dummy) {
    struct JOB *job;
    (void)dummy;

    while (1) {
        possess(WorkWaiting);
        wait_for(WorkWaiting, NOT_TO_BE, 0);
        job = WorkHead;
        if (!job) {
            release(WorkWaiting);
            continue;
        }
        if (job->func == JOB_DONE) {
            release(WorkWaiting);
            return;
        }
        WorkHead = job->next;
        if (WorkHead == NULL)
            WorkTail = &WorkHead;
        twist(WorkWaiting, BY, -1);
        job->next = NULL;

        if (job->func == JOB_PROCESS) {
            for (int i = 0; i < job->numline; i++) {
                unsigned char *line = (unsigned char *)&job->readbuf[job->readindex[i].offset];
                int len = job->readindex[i].len;
                process_line(job, line, len);
            }
            flush_output(job);
        }

        /* Release read buffer */
        if (job->readbuf == Readbuf) {
            possess(ReadBuf0);
            twist(ReadBuf0, BY, -1);
        } else {
            possess(ReadBuf1);
            twist(ReadBuf1, BY, -1);
        }

        /* Return job to free list */
        possess(FreeWaiting);
        *FreeTail = job;
        FreeTail = &(job->next);
        twist(FreeWaiting, BY, +1);
    }
}

/* ===== cacheline: block I/O with double buffering ===== */
static unsigned int cacheline(FILE *fi, char **mybuf, struct LineInfo **myindex) {
    char *curpos, *readbuf, *f;
    unsigned int len, Linecount, rlen;
    struct LineInfo *readindex;
    int cacheindex;
    static char *Lastleft;
    static int Lastcnt;
    int curcnt, curindex, doneline;

    cacheindex = Cacheindex;
    curpos = Readbuf;
    readindex = Readindex;
    if (cacheindex) {
        possess(ReadBuf1);
        wait_for(ReadBuf1, TO_BE, 0);
        release(ReadBuf1);
        curpos += MAXCHUNK / 2;
        readindex += RINDEXSIZE;
    } else {
        possess(ReadBuf0);
        wait_for(ReadBuf0, TO_BE, 0);
        release(ReadBuf0);
    }
    readbuf = curpos;
    curcnt = 0;
    Linecount = 0;
    *mybuf = readbuf;
    *myindex = readindex;
    if (Lastcnt) {
        memmove(curpos, Lastleft, Lastcnt);
        curcnt = Lastcnt;
        curpos += Lastcnt;
        Lastcnt = 0;
        Lastleft = NULL;
    }
    curcnt += fread(curpos, 1, (MAXCHUNK / 2) - curcnt - 1, fi);
    curpos = readbuf;
    curindex = 0;

    while (curindex < curcnt) {
        readindex[Linecount].offset = curindex;
        len = 0;
        doneline = 0;
        f = findeol(&curpos[curindex], curcnt - curindex - 1);
        if (f) {
            doneline = 1;
            rlen = len = f - &curpos[curindex];
            if (len > 0 && f[-1] == '\r') {
                f[-1] = '\n';
                rlen--;
            }
            if (rlen > MAXLINE) rlen = MAXLINE;
            readindex[Linecount].len = rlen;

            /* $HEX[] decode if enabled */
            if (DoHex && rlen >= 6 &&
                curpos[curindex] == '$' && curpos[curindex+1] == 'H' &&
                curpos[curindex+2] == 'E' && curpos[curindex+3] == 'X' &&
                curpos[curindex+4] == '[') {
                /* Decode hex in place */
                int src = curindex + 5;
                int dst = curindex;
                int end = curindex + rlen;
                while (src < end && curpos[src] != ']') {
                    int h1 = hexval(curpos[src]);
                    int h2 = (src + 1 < end) ? hexval(curpos[src + 1]) : -1;
                    if (h1 >= 0 && h2 >= 0) {
                        curpos[dst++] = (h1 << 4) | h2;
                        src += 2;
                    } else {
                        break;
                    }
                }
                readindex[Linecount].len = dst - curindex;
            }

            curindex += len + 1;
        } else {
            if (feof(fi)) {
                curpos[curcnt] = '\n';
                rlen = len = (curcnt - curindex);
                if (len > 1) rlen--;
                if (rlen > 0 && curpos[curindex + rlen - 1] == '\n') rlen--;
                if (rlen > 0 && curpos[curindex + rlen - 1] == '\r') rlen--;
                if (rlen > MAXLINE) rlen = MAXLINE;
                readindex[Linecount].len = rlen;

                /* $HEX[] decode for last line */
                if (DoHex && rlen >= 6 &&
                    curpos[curindex] == '$' && curpos[curindex+1] == 'H' &&
                    curpos[curindex+2] == 'E' && curpos[curindex+3] == 'X' &&
                    curpos[curindex+4] == '[') {
                    int src = curindex + 5;
                    int dst = curindex;
                    int end = curindex + rlen;
                    while (src < end && curpos[src] != ']') {
                        int h1 = hexval(curpos[src]);
                        int h2 = (src + 1 < end) ? hexval(curpos[src + 1]) : -1;
                        if (h1 >= 0 && h2 >= 0) {
                            curpos[dst++] = (h1 << 4) | h2;
                            src += 2;
                        } else {
                            break;
                        }
                    }
                    readindex[Linecount].len = dst - curindex;
                }

                if (rlen > 0 && rlen < MAXLINE) { Linecount++; }
                break;
            }
            Lastleft = &curpos[curindex];
            Lastcnt = curcnt - curindex;
            if (Lastcnt >= MAXLINE) {
                Lastcnt = 0;
            }
            break;
        }
        if (len >= MAXLINE) continue;
        if (doneline) {
            if (++Linecount >= RINDEXSIZE) {
                if (curindex < curcnt) {
                    Lastleft = &curpos[curindex];
                    Lastcnt = curcnt - curindex;
                }
                break;
            }
        }
    }
    Cacheindex ^= 1;
    return Linecount;
}

/* ===== Validate encodings at startup ===== */
static void validate_encodings(void) {
    Num_encodings = 0;
    for (int i = 0; encodings[i].enc.name != NULL; i++) {
        Num_encodings = i + 1;
        encodings[i].available = 0;

        /* Check include/exclude filters */
        if (NumInclude > 0) {
            int found = 0;
            for (int j = 0; j < NumInclude; j++) {
                if (strcasecmp(IncludeEncodings[j], encodings[i].enc.name) == 0) {
                    found = 1; break;
                }
                for (int k = 0; encodings[i].aliases[k]; k++) {
                    if (strcasecmp(IncludeEncodings[j], encodings[i].aliases[k]) == 0) {
                        found = 1; break;
                    }
                }
                if (found) break;
            }
            if (!found) continue;
        }
        if (NumExclude > 0) {
            int excluded = 0;
            for (int j = 0; j < NumExclude; j++) {
                if (strcasecmp(ExcludeEncodings[j], encodings[i].enc.name) == 0) {
                    excluded = 1; break;
                }
                for (int k = 0; encodings[i].aliases[k]; k++) {
                    if (strcasecmp(ExcludeEncodings[j], encodings[i].aliases[k]) == 0) {
                        excluded = 1; break;
                    }
                }
                if (excluded) break;
            }
            if (excluded) continue;
        }

        /* All encodings are available (no iconv dependency) */
        encodings[i].available = 1;
    }

    int avail = 0;
    for (int i = 0; i < Num_encodings; i++)
        if (encodings[i].available) avail++;
    fprintf(stderr, "encforce %s: %d/%d encodings available, %d threads\n",
        VERSION, avail, Num_encodings, Maxt);
}

/* ===== Process a file ===== */
static void process_file(FILE *fi) {
    char *readbuf;
    struct LineInfo *readindex;
    unsigned int numline;

    /* TSV header */
    if (OutFormat == FMT_TSV) {
        possess(Output_lock);
        fprintf(stdout, "input\tinput_hex\toperation\tencoding\ttarget\tstrategy\toutput\toutput_hex\n");
        release(Output_lock);
    }

    while ((numline = cacheline(fi, &readbuf, &readindex)) > 0) {
        /* Get a free job */
        possess(FreeWaiting);
        wait_for(FreeWaiting, NOT_TO_BE, 0);
        struct JOB *job = FreeHead;
        FreeHead = job->next;
        if (FreeHead == NULL) FreeTail = &FreeHead;
        twist(FreeWaiting, BY, -1);
        job->next = NULL;

        /* Set up job */
        job->func = JOB_PROCESS;
        job->readbuf = readbuf;
        job->readindex = readindex;
        job->numline = numline;
        job->startline = 0;

        /* Track which buffer this is from */
        if (Cacheindex == 1) {
            /* We just flipped to 1, so this data is in buffer 0 */
            possess(ReadBuf0);
            twist(ReadBuf0, BY, +1);
        } else {
            possess(ReadBuf1);
            twist(ReadBuf1, BY, +1);
        }

        /* Launch thread if needed */
        if (Workthread < Maxt) {
            launch(procjob, NULL);
            Workthread++;
        }

        /* Queue job */
        possess(WorkWaiting);
        *WorkTail = job;
        WorkTail = &(job->next);
        twist(WorkWaiting, BY, +1);
    }

    /* Wait for all jobs to complete */
    possess(FreeWaiting);
    wait_for(FreeWaiting, TO_BE, Maxt);
    release(FreeWaiting);

    /* Send done signal */
    struct JOB done_job;
    memset(&done_job, 0, sizeof(done_job));
    done_job.func = JOB_DONE;
    done_job.next = NULL;
    possess(WorkWaiting);
    *WorkTail = &done_job;
    WorkTail = &(done_job.next);
    twist(WorkWaiting, BY, +1);

    join_all();
}

/* ===== Process command-line string arguments ===== */
static void process_strings(int argc, char **argv) {
    /* For string arguments, process them single-threaded for simplicity */
    struct JOB job;
    memset(&job, 0, sizeof(job));
    job.outbuf = malloc(OUTBUFSIZE);
    job.outlen = 0;
    job.outsize = OUTBUFSIZE;
    job.dedup_hashes = calloc(DEDUP_CAPACITY, sizeof(uint64_t));
    job.dedup_capacity = DEDUP_CAPACITY;
    job.scratch = malloc(SCRATCH_SIZE);
    job.scratch_size = SCRATCH_SIZE;
    if (!job.outbuf || !job.dedup_hashes || !job.scratch) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    /* TSV header */
    if (OutFormat == FMT_TSV) {
        fprintf(stdout, "input\tinput_hex\toperation\tencoding\ttarget\tstrategy\toutput\toutput_hex\n");
    }

    for (int i = 0; i < argc; i++) {
        process_line(&job, (unsigned char *)argv[i], strlen(argv[i]));
    }

    /* Flush remaining output */
    if (job.outlen > 0) {
        fwrite(job.outbuf, 1, job.outlen, stdout);
    }

    free(job.outbuf);
    free(job.dedup_hashes);
    free(job.scratch);
}

/* ===== Usage ===== */
static void usage(void) {
    fprintf(stderr,
        "Usage: encforce [OPTIONS] [INPUT...]\n"
        "\n"
        "Text encoding forensics tool. Discovers how text was corrupted\n"
        "by exhaustively trying encoding/decoding transformations.\n"
        "\n"
        "Arguments:\n"
        "  INPUT...               Input strings to process\n"
        "\n"
        "Options:\n"
        "  -f, --file FILE        Read inputs from file (one per line)\n"
        "  -m, --mode MODE        Operation mode: decode|encode|both|transcode|all\n"
        "                         (default: both)\n"
        "  -e, --encoding ENC     Only use these encodings (repeatable)\n"
        "  -x, --exclude ENC      Exclude these encodings (repeatable)\n"
        "  -j, --jobs N           Worker threads (default: CPU count)\n"
        "  -F, --format FMT       Output format: lines|json|tsv (default: lines)\n"
        "      --depth N          Max transcode chain depth (default: 1)\n"
        "      --raw              Disable $HEX[] input parsing and output encoding\n"
        "      --unique           Deduplicate output (default: on)\n"
        "      --no-unique        Disable deduplication\n"
        "      --no-errors        Hide results with errors\n"
        "  -v, --verbose          Show detailed output\n"
        "  -s, --suggest          Show mojibake suggestions\n"
        "  -h, --help             Show help\n"
        "  -V, --version          Show version\n"
    );
}

/* ===== Main ===== */
int main(int argc, char **argv) {
    char *input_file = NULL;

    static struct option long_options[] = {
        {"file", required_argument, 0, 'f'},
        {"mode", required_argument, 0, 'm'},
        {"encoding", required_argument, 0, 'e'},
        {"exclude", required_argument, 0, 'x'},
        {"jobs", required_argument, 0, 'j'},
        {"format", required_argument, 0, 'F'},
        {"depth", required_argument, 0, 'd'},
        {"raw", no_argument, 0, 'r'},
        {"unique", no_argument, 0, 'u'},
        {"no-unique", no_argument, 0, 'U'},
        {"no-errors", no_argument, 0, 'E'},
        {"verbose", no_argument, 0, 'v'},
        {"suggest", no_argument, 0, 's'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'V'},
        {0, 0, 0, 0}
    };

    Maxt = get_nprocs();
    if (Maxt < 1) Maxt = 1;
    if (Maxt > 64) Maxt = 64;

    int opt;
    while ((opt = getopt_long(argc, argv, "f:m:e:x:j:F:d:ruUEvshV", long_options, NULL)) != -1) {
        switch (opt) {
        case 'f':
            input_file = optarg;
            break;
        case 'm':
            if (strcmp(optarg, "decode") == 0) OpMode = MODE_DECODE;
            else if (strcmp(optarg, "encode") == 0) OpMode = MODE_ENCODE;
            else if (strcmp(optarg, "both") == 0) OpMode = MODE_BOTH;
            else if (strcmp(optarg, "transcode") == 0) OpMode = MODE_TRANSCODE;
            else if (strcmp(optarg, "all") == 0) OpMode = MODE_ALL;
            else {
                fprintf(stderr, "Unknown mode: %s\n", optarg);
                exit(1);
            }
            break;
        case 'e':
            IncludeEncodings = realloc(IncludeEncodings, (NumInclude + 1) * sizeof(char *));
            IncludeEncodings[NumInclude++] = optarg;
            break;
        case 'x':
            ExcludeEncodings = realloc(ExcludeEncodings, (NumExclude + 1) * sizeof(char *));
            ExcludeEncodings[NumExclude++] = optarg;
            break;
        case 'j':
            Maxt = atoi(optarg);
            if (Maxt < 1) Maxt = 1;
            if (Maxt > 256) Maxt = 256;
            break;
        case 'F':
            if (strcmp(optarg, "lines") == 0) OutFormat = FMT_LINES;
            else if (strcmp(optarg, "json") == 0) OutFormat = FMT_JSON;
            else if (strcmp(optarg, "tsv") == 0) OutFormat = FMT_TSV;
            else {
                fprintf(stderr, "Unknown format: %s\n", optarg);
                exit(1);
            }
            break;
        case 'd':
            MaxDepth = atoi(optarg);
            if (MaxDepth < 1) MaxDepth = 1;
            break;
        case 'r':
            DoHex = 0;
            break;
        case 'u':
            DoUnique = 1;
            break;
        case 'U':
            DoUnique = 0;
            break;
        case 'E':
            DoNoErrors = 1;
            break;
        case 'v':
            DoVerbose = 1;
            break;
        case 's':
            DoSuggest = 1;
            break;
        case 'h':
            usage();
            exit(0);
        case 'V':
            fprintf(stderr, "encforce %s\n", VERSION);
            exit(0);
        default:
            usage();
            exit(1);
        }
    }

    /* Validate encodings */
    validate_encodings();

    /* Build reverse maps for single-byte encode */
    {
        struct CharEncoding *enc_array = &encodings[0].enc;
        /* We need to pass the array with proper stride.
         * Since CharEncoding is embedded in Encoding, we iterate manually. */
        for (int i = 0; i < Num_encodings; i++) {
            if (encodings[i].enc.type == ENC_TYPE_SINGLE_BYTE &&
                encodings[i].enc.to_unicode != NULL &&
                encodings[i].available) {
                /* charconv_init_reverse_maps expects a contiguous CharEncoding array,
                 * but our array has stride sizeof(struct Encoding). Call individually. */
                charconv_init_reverse_maps(&encodings[i].enc, 1);
            }
        }
        (void)enc_array;
    }

    /* If we have remaining argv arguments and no -f, process them as strings */
    if (optind < argc && !input_file) {
        process_strings(argc - optind, argv + optind);
        fflush(stdout);
        free(IncludeEncodings);
        free(ExcludeEncodings);
        return 0;
    }

    /* Set up block I/O and threading for file processing */
    FILE *fi = stdin;
    if (input_file) {
        if (strcmp(input_file, "-") != 0) {
            fi = fopen(input_file, "rb");
            if (!fi) {
                fprintf(stderr, "Can't open: %s\n", input_file);
                exit(1);
            }
        }
    }

    /* Allocate buffers */
    Readbuf = malloc(MAXCHUNK + 16);
    Readindex = malloc(MAXLINEPERCHUNK * 2 * sizeof(struct LineInfo) + 16);
    Jobs = calloc(Maxt, sizeof(struct JOB));

    FreeWaiting = new_lock(Maxt);
    WorkWaiting = new_lock(0);
    ReadBuf0 = new_lock(0);
    ReadBuf1 = new_lock(0);
    Output_lock = new_lock(0);

    if (!Readbuf || !Readindex || !Jobs || !FreeWaiting || !WorkWaiting ||
        !ReadBuf0 || !ReadBuf1 || !Output_lock) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    /* Initialize job queue */
    WorkTail = &WorkHead;
    FreeTail = &FreeHead;
    Workthread = 0;

    for (int x = 0; x < Maxt; x++) {
        *FreeTail = &Jobs[x];
        FreeTail = &(Jobs[x].next);
        Jobs[x].outbuf = malloc(OUTBUFSIZE);
        Jobs[x].outlen = 0;
        Jobs[x].outsize = OUTBUFSIZE;
        Jobs[x].dedup_hashes = calloc(DEDUP_CAPACITY, sizeof(uint64_t));
        Jobs[x].dedup_capacity = DEDUP_CAPACITY;
        Jobs[x].scratch = malloc(SCRATCH_SIZE);
        Jobs[x].scratch_size = SCRATCH_SIZE;
        if (!Jobs[x].outbuf || !Jobs[x].dedup_hashes || !Jobs[x].scratch) {
            fprintf(stderr, "Memory allocation failed for job %d\n", x);
            exit(1);
        }
    }

    process_file(fi);

    if (fi != stdin) fclose(fi);

    /* Cleanup */
    for (int x = 0; x < Maxt; x++) {
        free(Jobs[x].outbuf);
        free(Jobs[x].dedup_hashes);
        free(Jobs[x].scratch);
    }
    free(Jobs);
    free(Readbuf);
    free(Readindex);
    free_lock(FreeWaiting);
    free_lock(WorkWaiting);
    free_lock(ReadBuf0);
    free_lock(ReadBuf1);
    free_lock(Output_lock);
    free(IncludeEncodings);
    free(ExcludeEncodings);

    fflush(stdout);
    return 0;
}
