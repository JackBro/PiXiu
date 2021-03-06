#ifndef PIXIU_STR_H
#define PIXIU_STR_H

#include "../common/gen.h"
#include "../common/List.h"
#include "../common/style.h"
#include "../common/util.h"
#include <assert.h>
#include <stdint.h>

#define PXS_UNIQUE 251
#define PXS_KEY 0
#define PXS_KEY_SEC 2
#define PXS_COMPRESS 1

#define PXS_STREAM_ON -1
#define PXS_STREAM_OFF -2
#define PXS_STREAM_PASS -3

#define PXC_STR_NUM 65535
#define PXSG_MAX_TO 65535

#define PXSG_SEE_KEY_BREAK(val, cmd) \
if (!spec_mode && val == PXS_UNIQUE) { spec_mode = true; } \
else if (spec_mode) { \
    if (val == PXS_KEY) { \
        cmd; \
        break; \
    } else { spec_mode = false; } \
}

struct PiXiuChunk;
struct PXSGen;

struct PXSRecordSmall {
    uint8_t head;
    uint8_t len;
    uint16_t idx;
    uint16_t to;
};

struct PXSRecordBig {
    uint8_t head;
    uint8_t sign;
    uint16_t idx;
    uint16_t to;
    uint16_t from;
};

union PXSRecord {
    PXSRecordSmall small;
    PXSRecordBig big;
};

struct PXSMsg {
    int chunk_idx_Cmd;
    int pxs_idx;
    uint8_t val;
};

struct PiXiuStr {
    uint16_t len;
    uint8_t data[];

    PXSGen * parse(int, int, PiXiuChunk *);

    PiXiuStr * concat(PiXiuStr *);

    bool key_eq(PiXiuStr *, PiXiuChunk *);

    bool startswith(PiXiuStr *, PiXiuChunk *);
};

struct PiXiuChunk {
    PiXiuStr * strs[PXC_STR_NUM];
    uint16_t used_num = 0;
    uint16_t total_num;

    PiXiuStr * getitem(int);

    void delitem(int);

    bool is_delitem(int);

    void free_prop(void);
};

PiXiuStr * PiXiuStr_init(uint8_t[], int);

PiXiuStr * PiXiuStr_init_key(uint8_t[], int);

PiXiuStr * PiXiuStr_init_stream(PXSMsg, bool = true);

PiXiuChunk * PiXiuChunk_init(void);

void PiXiuStr_free(PiXiuStr *);

void PiXiuChunk_free(PiXiuChunk *);

void PXSGen_free(PXSGen *);


#define PXSG_TRY_WRITE \
if (src_cursor >= from) { \
    $yield(cmd); \
    ret_cursor++; \
} \
src_cursor++;

$gen(PXSGen) {
    PiXiuStr * self;
    PiXiuChunk * ctx;
    PXSGen * sub_gen;

    int from;
    int to;
    int len;
    int ret_cursor;
    int src_cursor;
    int i, j;
    int sub_from, sub_to;
    int supply;

    uint8_t cmd;
    uint8_t next_cmd;
    // <var/>

    // <body>
    $emit(uint8_t)
            PXSRecord record;

            assert(from >= 0 && to >= from);
            len = to - from;
            src_cursor = ret_cursor = 0;

            for (i = 0; ret_cursor < len && i < self->len; ++i) {
                cmd = self->data[i];

                if (cmd == PXS_UNIQUE) {
                    next_cmd = self->data[i + 1];

                    if (next_cmd == PXS_KEY || next_cmd == PXS_UNIQUE ||
                        next_cmd == PXS_KEY_SEC) {
                        PXSG_TRY_WRITE;
                        i++;
                        cmd = next_cmd;
                        PXSG_TRY_WRITE;
                    } else if (next_cmd == PXS_COMPRESS || next_cmd > sizeof(PXSRecordSmall)) {
                        if (next_cmd == PXS_COMPRESS) {
                            record.big = valIn((PXSRecordBig *) adrOf(self->data[i]));
                            assert(record.big.head == PXS_UNIQUE);
                            assert(record.big.sign == PXS_COMPRESS);
                            i += sizeof(PXSRecordBig) - 1;
                        } else {
                            record.small = valIn((PXSRecordSmall *) adrOf(self->data[i]));
                            assert(record.small.head == PXS_UNIQUE);
                            assert(record.small.idx == record.big.idx);
                            i += sizeof(PXSRecordSmall) - 1;
                            record.big.from = record.small.to - record.small.len;
                        }
                        supply = record.big.to - record.big.from;

                        if (src_cursor - 1 + supply >= from) {
                            sub_from = record.big.from + max(0, from - src_cursor);
                            sub_to = min<int>(record.big.to, sub_from + (len - ret_cursor));
                            uint8_t rv;

                            if (sub_from < ret_cursor && ret_cursor < sub_to
                                && ctx->getitem(record.big.idx) == self) {
                                j = 0;
                                while (j != sub_to - sub_from) {
                                    sub_gen = self->parse(sub_from, ret_cursor, ctx);
                                    while (sub_gen->operator()(rv)) {
                                        $yield(rv);
                                        if (++j == sub_to - sub_from) {
                                            break;
                                        }
                                    }
                                    PXSGen_free(sub_gen);
                                    sub_gen = NULL;
                                }
                            } else {
                                sub_gen = ctx->getitem(record.big.idx)->parse(sub_from, sub_to, ctx);
                                while (sub_gen->operator()(rv)) {
                                    $yield(rv);
                                }
                                PXSGen_free(sub_gen);
                                sub_gen = NULL;
                            }
                            ret_cursor += sub_to - sub_from;
                        }
                        src_cursor += supply;
                    } else { assert(false); }
                } else {
                    PXSG_TRY_WRITE;
                }
            }
    $stop;

    char * consume_repr(void) {
        List_init(char, output);
        uint8_t rv;
        while (this->operator()(rv)) {
            if (char_visible(rv)) {
                List_append(char, output, rv);
            }
        }
        List_append(char, output, '\0');
        PXSGen_free(this);
        return output;
    }
};

#endif