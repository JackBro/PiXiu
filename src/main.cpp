#include "common/gen.h"
#include "common/List.h"
#include "common/Que.h"
#include "data_struct/CritBitTree.h"
#include "data_struct/ScapegoatTree.h"
#include "proj/PiXiuCtrl.h"
#include <iostream>
#include <vector>

void t_CritBitTree(void);

void t_gen(void);

void t_List(void);

void t_MemPool(void);

void t_PiXiuCtrl(void);

void t_PiXiuStr(void);

void t_Que(void);

void t_ScapegoatTree(void);

void t_SuffixTree(void);

int main() {
#ifndef NDEBUG
    t_CritBitTree();
    t_gen();
    t_List();
    t_MemPool();
    t_PiXiuStr();
    t_Que();
    t_ScapegoatTree();
    t_SuffixTree();
    t_PiXiuCtrl();
#else
    PiXiuCtrl ctrl;
    ctrl.init_prop();

    std::string cmd;
    long total_diff = 0;
    while (true) {
        std::cout << "Command: ";
        std::getline(std::cin, cmd);
        if (cmd[0] == '~') {
            break;
        }

        constexpr int token_len = 4;
        if (start_with(cmd.c_str(), "GET ", token_len)) {
            auto gen = ctrl.getitem((uint8_t *) (cmd.c_str() + token_len), (int) (cmd.size() - token_len));
            if (gen != NULL) {
                auto res = gen->consume_repr();
                std::cout << res << std::endl;
                free(res);
            }
        } else if (start_with(cmd.c_str(), "SET ", token_len)) {
            auto pos = cmd.find("::");
            if (pos != std::string::npos) {
                auto k = cmd.substr(token_len, pos - token_len);
                auto v = cmd.substr(pos);
                ctrl.setitem((uint8_t *) k.c_str(), (int) k.size(), (uint8_t *) v.c_str(), (int) v.size());

                int diff = (int) cmd.size() - ctrl.st.cbt_chunk->getitem(ctrl.st.local_chunk.used_num - 1)->len;
                std::cout << "本次节约内存数 " << diff << std::endl;
                total_diff += diff;
                std::cout << "总共节约内存数 " << total_diff << std::endl;
            }
        }
    }

    ctrl.free_prop();
#endif
    return 0;
}

void t_ScapegoatTree(void) {
    struct int_cmp {
        int val;

        bool operator<(int_cmp * another) {
            return val < another->val;
        }

        bool operator==(int_cmp * another) {
            return val == another->val;
        }
    };

    MemPool pool;
    ScapegoatTree<int_cmp, 512> sgt;
    for (int i = 0; i < 256; ++i) {
        auto obj = (int_cmp *) pool.p_malloc(sizeof(int_cmp));
        obj->val = i;
        assert(sgt.getitem(obj) == NULL);
        sgt.setitem(obj, adrOf(pool));
        assert(sgt.getitem(obj)->val == i);
    }
    for (int i = 512 - 1; i >= 256; --i) {
        auto obj = (int_cmp *) pool.p_malloc(sizeof(int_cmp));
        obj->val = i;
        assert(sgt.getitem(obj) == NULL);
        sgt.setitem(obj, adrOf(pool));
        assert(sgt.getitem(obj)->val == i);
    }

    int_cmp cmp;
    for (int i = 256 - 1; i >= 0; --i) {
        cmp.val = i;
        assert(sgt.getitem(adrOf(cmp))->val == i);
    }
    cmp.val = -1;
    assert(sgt.getitem(adrOf(cmp)) == NULL);
    cmp.val = 1024;
    assert(sgt.getitem(adrOf(cmp)) == NULL);
    pool.free_prop();
    PRINT_FUNC;
};

void t_Que(void) {
    int q_len = 3;
    int q[q_len];
    int q_cursor = 0;
    std::vector<int> ctrl_q;

    for (int i = 0; i < 10; ++i) {
        auto val = rand();
        Que_push(q, val);
        ctrl_q.push_back(val);
    }

    for (int i = 0; i < q_len; ++i) {
        assert(Que_get(q, i) == ctrl_q[ctrl_q.size() - (q_len - i)]);
    }
    PRINT_FUNC;
}

$gen(range0_10) {
    // var
    int i;

    // body
    $emit(int)
            for (i = 0; i < 10; ++i) {
                $yield(i);
            }
    $stop;
};

void t_gen(void) {
    range0_10 gen;
    int rv;
    for (int i = 0; i < 10; ++i) {
        assert(gen(rv));
        assert(rv == i);
    }
    assert(!gen(rv));
    PRINT_FUNC;
}

void t_List(void) {
    List_init(int, test_list);
    std::vector<int> ctrl_list;

    for (int i = 0; i < 100; ++i) {
        List_append(int, test_list, i);
        ctrl_list.push_back(i);

        auto idx = rand() % (ctrl_list.size() + 1);
        List_insert(int, test_list, idx, i);
        ctrl_list.insert(ctrl_list.begin() + idx, i);

        idx = rand() % ctrl_list.size();
        List_del(int, test_list, idx);
        ctrl_list.erase(ctrl_list.begin() + idx);
    }

    assert(test_list_len == ctrl_list.size());
    for (int i = 0; i < ctrl_list.size(); ++i) {
        assert(test_list[i] == ctrl_list[i]);
    }

    List_free(test_list);
    PRINT_FUNC;
}
