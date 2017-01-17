#include "SuffixTree.h"

static MemPool * Glob_Pool = NULL;
static PiXiuChunk * Glob_Ctx = NULL;

void STNode::set_sub(STNode * node) {
    assert(Glob_Pool != NULL);
    this->subs.setitem(node, Glob_Pool);
}

STNode * STNode::get_sub(uint8_t key) {
    STNode cmp{.from=1, .to=0};
    cmp.chunk_idx = key;
    return this->subs.getitem(adrOf(cmp));
}

#define SET_AB_CHAR \
assert(Glob_Ctx != NULL); \
a_char = Glob_Ctx->getitem(this->chunk_idx)->data[this->from]; \
if (another->from > another->to) { \
    b_char = (uint8_t) another->chunk_idx; \
} else { \
    b_char = Glob_Ctx->getitem(another->chunk_idx)->data[another->from]; \
}

bool STNode::operator<(STNode * another) {
    uint8_t a_char, b_char;
    SET_AB_CHAR;
    return a_char < b_char;
}

bool STNode::operator==(STNode * another) {
    uint8_t a_char, b_char;
    SET_AB_CHAR;
    return a_char == b_char;
}

bool STNode::is_root() {
    return this->successor == this;
}

bool STNode::is_inner() {
    return !this->is_root() && this->subs.root != NULL;
}

bool STNode::is_leaf() {
    return !this->is_root() && this->subs.root == NULL;
}

STNode * STNode_p_init(void) {
    assert(Glob_Pool != NULL);
    auto ret = (STNode *) Glob_Pool->p_malloc(sizeof(STNode));
    ret->successor = NULL;
    ret->subs.root = NULL;
    ret->subs.size = 0;
    return ret;
}

void SuffixTree::init_prop() {
    assert(this->local_chunk.used_num == 0);
    assert(this->local_pool.curr_pool == NULL);

    Glob_Ctx = adrOf(this->local_chunk);
    Glob_Pool = adrOf(this->local_pool);

    this->root = STNode_p_init();
    this->root->successor = this->root;
    this->remainder = this->counter = 0;

    this->act_node = this->root;
    this->act_chunk_idx = this->act_direct = this->act_offset = 0;

    this->cbt_chunk = (PiXiuChunk *) this->local_pool.p_malloc(sizeof(PiXiuChunk));
    this->cbt_chunk->used_num = 0;
}

void SuffixTree::free_prop() {
    this->local_chunk.free_prop();
    this->local_pool.free_prop();
}

void SuffixTree::reset() {
    this->remainder = 0;
    this->counter = 0;
    this->act_node = this->root;
    this->act_chunk_idx = 0;
    this->act_direct = 0;
    this->act_offset = 0;
}

char * SuffixTree::repr() {
    List_init(char, output);

    std::function<void(STNode *)> print_node = [&](STNode * node) {
        auto pxs = this->local_chunk.getitem(node->chunk_idx);
        for (int i = node->from; i < node->to; ++i) {
            if (char_visible(pxs->data[i])) {
                List_append(char, output, pxs->data[i]);
            }
        }
    };

    std::function<void(STNode *, int)> print_tree = [&](STNode * node, int lv) {
        if (lv > 0) {
            for (int i = 0; i < (lv - 1) * 2; ++i) {
                List_append(char, output, ' ');
            }
            for (int i = 0; i < 2; ++i) {
                List_append(char, output, '-');
            }
            print_node(node);
        } else {
            List_append(char, output, '#');
        }
        List_append(char, output, '\n');

        if (node->subs.root != NULL) {
            lv++;
            STNode * sub_node;
            for (uint8_t i = 0; i <= UINT8_MAX; ++i) {
                if ((sub_node = node->get_sub(i)) != NULL) {
                    print_tree(sub_node, lv);
                }
            }
        }
    };

    List_append(char, output, '\0');
    return output;
}

#define MSG_NO_COMPRESS PiXiuStr_init_stream((PXSMsg) {.chunk_idx__cmd=PXS_STREAM_PASS, .val=msg_char})
#define MSG_COMPRESS(c_idx, p_idx) \
PiXiuStr_init_stream((PXSMsg) { \
    .chunk_idx__cmd=c_idx, \
    .pxs_idx=p_idx, \
    .val=msg_char \
})

static void s_case_root(SuffixTree * self, uint16_t chunk_idx, uint8_t msg_char) {
    auto collapse_node = self->root->get_sub(msg_char);
    if (collapse_node == NULL) { // 无法坍缩, 新建叶结点
        auto leaf_node = STNode_p_init();
        leaf_node->chunk_idx = chunk_idx;
        leaf_node->from = self->counter;
        leaf_node->to = Glob_Ctx->getitem(chunk_idx)->len;
        self->root->set_sub(leaf_node);
        self->remainder--;
        MSG_NO_COMPRESS;
    } else { // 开始坍缩
        self->act_chunk_idx = collapse_node->chunk_idx;
        self->act_direct = collapse_node->from;
        self->act_offset++;
        MSG_COMPRESS(collapse_node->chunk_idx, collapse_node->from);
    }
}

static void s_overflow_fix(SuffixTree * self, uint16_t chunk_idx, uint16_t remainder) {
    auto temp_uchar = Glob_Ctx->getitem(self->act_chunk_idx)->data[self->act_direct];
    auto collapse_node = self->act_node->get_sub(temp_uchar);

    // counter - remainder + 1 = collapse_node.op
    auto supply = collapse_node->to - collapse_node->from;
    if (self->act_offset > supply) {
        self->act_node = collapse_node;
        remainder -= supply;
        temp_uchar = Glob_Ctx->getitem(chunk_idx)->data[self->counter - remainder + 1];

        auto next_collapse_node = collapse_node->get_sub(temp_uchar);
        self->act_chunk_idx = next_collapse_node->chunk_idx;
        self->act_direct = next_collapse_node->from;
        self->act_offset -= supply;
        return s_overflow_fix(self, chunk_idx, remainder);
    }
}

static void s_split_grow(SuffixTree * self, uint16_t chunk_idx, STNode * collapse_node) {

}

static void s_insert_char(SuffixTree * self, uint16_t chunk_idx, uint8_t msg_char) {
    self->remainder++;
    uint8_t temp_uchar;

    if (self->act_node->is_root() && self->act_offset == 0) {
        s_insert_char(self, chunk_idx, msg_char);
    } else { // 已坍缩
        temp_uchar = Glob_Ctx->getitem(self->act_chunk_idx)->data[self->act_direct];
        auto collapse_node = self->act_node->get_sub(temp_uchar);
        assert(Glob_Ctx->getitem(collapse_node->chunk_idx)->data[collapse_node->from] == temp_uchar);

        // edge 扩大?
        if (collapse_node->from + self->act_offset == collapse_node->to) {
            auto next_collapse_node = collapse_node->get_sub(msg_char);
            if (next_collapse_node != NULL) { // YES
                self->act_node = collapse_node; // 推移 act_node
                self->act_chunk_idx = next_collapse_node->chunk_idx;
                self->act_direct = next_collapse_node->from;
                self->act_offset = 1;
                MSG_COMPRESS(next_collapse_node->chunk_idx, next_collapse_node->from);
                goto end;
            } else { // NO
                goto explode;
            }
        }

        temp_uchar = Glob_Ctx->getitem(collapse_node->chunk_idx)->data[collapse_node->from + self->act_offset];
        if (temp_uchar == msg_char) { // YES
            MSG_COMPRESS(collapse_node->chunk_idx, collapse_node->from + self->act_offset);
            self->act_offset++;
        } else { // NO
            explode: // 炸开累积后缀
            MSG_NO_COMPRESS;
            while (self->remainder > 0) {
                if (!self->act_node->is_inner()) {
                    s_split_grow(self, chunk_idx, collapse_node);
                    // 状态转移
                    self->act_offset--;
                    self->act_direct++;

                    if (self->act_offset > 0) {
                        s_overflow_fix(self, chunk_idx, self->remainder);
                        temp_uchar = Glob_Ctx->getitem(self->act_chunk_idx)->data[self->act_direct];
                        auto next_collapse_node = self->act_node->get_sub(temp_uchar);
                        collapse_node->successor = next_collapse_node;
                        collapse_node = next_collapse_node;
                    } else { // 后缀已用完, 回到 case root
                        collapse_node->successor = self->root;
                        s_case_root(self, chunk_idx, msg_char);
                        break;
                    }
                } else { // 需要使用 suffix link
                    s_split_grow(self, chunk_idx, collapse_node);
                    self->act_node = self->act_node->successor;
                    s_overflow_fix(self, chunk_idx, self->remainder);

                    temp_uchar = Glob_Ctx->getitem(self->act_chunk_idx)->data[self->act_direct];
                    auto next_collapse_node = self->act_node->get_sub(temp_uchar);
                    collapse_node->successor = next_collapse_node;
                    collapse_node = next_collapse_node;
                }
            }
        }
    }

    end:
    self->counter++;
}

SuffixTree::s_ret SuffixTree::setitem(PiXiuStr * src) {
    assert(Glob_Ctx != NULL && Glob_Pool != NULL);
    auto idx = this->local_chunk.used_num;
    assert(idx == this->cbt_chunk->used_num);

    this->local_chunk.strs[idx] = src;
    PiXiuStr_init_stream((PXSMsg) {.chunk_idx__cmd=PXS_STREAM_ON});
    for (int i = 0; i < src->len; ++i) {
        s_insert_char(this, idx, src->data[i]);
    }
    this->cbt_chunk->strs[idx] = PiXiuStr_init_stream((PXSMsg) {.chunk_idx__cmd=PXS_STREAM_OFF});

    this->local_chunk.used_num++;
    this->cbt_chunk->used_num++;
    this->reset();
    return s_ret{this->cbt_chunk, idx};
}