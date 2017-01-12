#include "../common/List.h"
#include "../common/Que.h"
#include "CritBitTree.h"
#include <stddef.h>
#include <stdio.h>

#define is_inner(p) adr_is_spec(p)
#define normal(p) adr_de_spec(p)
#define special(p) adr_mk_spec(p)

int CBTInner::get_direct(void * node) {
    if (node == normal(this->crit_node_arr[0])) {
        return 0;
    } else {
        assert(node == normal(this->crit_node_arr[1]));
        return 1;
    }
};

int CritBitTree::setitem(PiXiuStr * src, PiXiuChunk * ctx, uint16_t chunk_idx) {
    auto sign = 0;
    if (this->root == NULL) {
        this->root = ctx;
        this->chunk_idx = chunk_idx;
    } else {
        auto ret = this->find_best_match(src);
        auto pa = (CBTInner *) ret.pa;
        auto crit_chunk = (PiXiuChunk *) ret.crit_node;

        auto pa_direct = pa->get_direct(crit_chunk);
        auto crit_chunk_idx = pa->chunk_idx_arr[pa_direct];
        auto crit_pxs = crit_chunk->getitem(crit_chunk_idx);

        auto crit_gen = crit_pxs->parse(0, PXSG_MAX_TO, crit_chunk);
        auto src_gen = src->parse(0, PXSG_MAX_TO, NULL);

        uint16_t diff_at = 0;
        uint8_t crit_rv, src_rv;

        auto replace = [&]() {
            sign = CBT_SET_REPLACE;
            crit_chunk->delitem(crit_chunk_idx);
            pa->crit_node_arr[pa_direct] = ctx;
            pa->chunk_idx_arr[pa_direct] = chunk_idx;
        };

        auto insert = [&]() {
            uint8_t mask = (crit_rv ^ src_rv);
            mask |= mask >> 1;
            mask |= mask >> 2;
            mask |= mask >> 4;
            // 0b0100_1000 => 0b0111_1111

            // 0b0111_1111 => 0b0100_0000
            mask = (mask & ~(mask >> 1)) ^ (uint8_t) UINT8_MAX;
            auto direct = (1 + (mask | src->data[diff_at])) >> 8;

            auto inner_node = CBTInner_init();
            inner_node->crit_node_arr[direct] = ctx;
            inner_node->chunk_idx_arr[direct] = chunk_idx;
            inner_node->diff_at = diff_at;
            inner_node->mask = mask;

            CBTInner * parent = NULL;
            auto parent_direct = -1;
            auto replace_ptr = this->root;
            auto replace_node = (CBTInner *) normal(replace_ptr);
            while (true) {
                if (!is_inner(replace_ptr)
                    || replace_node->diff_at > diff_at
                    || (replace_node->diff_at == diff_at && replace_node->mask > inner_node->mask)) {
                    break;
                }

                uint8_t crit_byte = src->len > replace_node->diff_at ? src->data[replace_node->diff_at] : (uint8_t) 0;
                parent_direct = (1 + (replace_node->mask | crit_byte)) >> 8;

                parent = replace_node;
                replace_ptr = replace_node->crit_node_arr[parent_direct];
                replace_node = (CBTInner *) normal(replace_ptr);
            }

            if (parent == NULL) {
                this->root = special(inner_node);
            } else {
                assert(parent_direct >= 0 && pa_direct <= 1);
                parent->crit_node_arr[parent_direct] = special(inner_node);
            }
            inner_node->crit_node_arr[(direct + 1) % 2] = replace_ptr;
        };

        auto spec_mode = false;
        while (crit_gen->operator()(crit_rv) && src_gen->operator()(src_rv) && crit_rv == src_rv) {
            if (!spec_mode && crit_rv == PXS_UNIQUE) { spec_mode = true; }
            else if (spec_mode) {
                if (crit_rv == PXS_KEY) {
                    replace();
                    break;
                } else { spec_mode = false; }
            }
            diff_at++;
        }
        if (!spec_mode) { insert(); };

        PXSGen_free(crit_gen);
        PXSGen_free(src_gen);
    }
    return sign;
}

char * CritBitTree::repr(void) {
    List_init(char, output);

    auto print = [&](void * ptr, int lv) {
        int intent = 4 * lv;
        for (int i = 0; i < intent; ++i) {
            List_append(char, output, ' ');
        }

        if (!is_inner(ptr)) {
            auto pxs = (PiXiuStr *) ptr;
            for (int i = 0; i < pxs->len; ++i) {
                List_append(char, output, pxs->data[i]);
            }
            List_append(char, output, '\n');
            return;
        }

        auto inner = (CBTInner *) normal(ptr);
        char temp[50];
        sprintf(temp, "diff at: %i, mask: %i", inner->diff_at, inner->mask);
        for (int i = 0; temp[i] != '\0'; ++i) {
            List_append(char, output, temp[i]);
        }
        List_append(char, output, '\n');

        lv++;
        for (int i = 0; i < 2; ++i) {
            auto sub_ptr = inner->crit_node_arr[i];
            if (is_inner(sub_ptr)) { print(sub_ptr, lv); }
            else { print(((PiXiuChunk *) sub_ptr)->getitem(inner->chunk_idx_arr[i]), lv); }
        }
    };

    print(this->root, 0);
    List_append(char, output, '\0');
    return output;
}

CritBitTree::fbm_ret CritBitTree::find_best_match(PiXiuStr * src) {
    void * q[] = {NULL, NULL, this->root};
    auto q_len = lenOf(q);
    auto q_cursor = 0;

    auto ptr = Que_get(q, 2);
    while (is_inner(ptr)) {
        auto inner = (CBTInner *) normal(ptr);

        uint8_t crit_byte = src->len > inner->diff_at ? src->data[inner->diff_at] : (uint8_t) 0;
        uint8_t direct = ((uint8_t) 1 + (inner->mask | crit_byte)) >> 8;
        ptr = inner->crit_node_arr[direct];
        Que_push(q, ptr);
    }
    return CritBitTree::fbm_ret{normal(Que_get(q, 0)), normal(Que_get(q, 1)), normal(Que_get(q, 2))};
}

CBTInner * CBTInner_init(void) {
    return (CBTInner *) malloc(offsetof(CBTInner, mask) + sizeof(CBTInner::mask));
}

void CBTInner_free(CBTInner * inner) {
    free(inner);
}