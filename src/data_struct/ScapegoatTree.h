#ifndef SCAPEGOAT_TREE_H
#define SCAPEGOAT_TREE_H

#include "../common/MemPool.h"
#include <functional>
#include <math.h>

#define SGT_FACTOR 0.618

template<typename T>
struct SGTNode {
    SGTNode<T> * small;
    SGTNode<T> * big;
    T * obj;
};

template<typename T, int max_size = 256>
struct ScapegoatTree {
    typedef SGTNode<T> SGTN;

    SGTN * root = NULL;
    int size = 0;

    void setitem(T * obj, MemPool * pool) {
        auto mk_node = [&]() {
            auto node = (SGTN *) pool->p_malloc(sizeof(SGTN));
            node->small = node->big = NULL;
            node->obj = obj;
            return node;
        };

        if (this->root == NULL) {
            this->root = mk_node();
            this->size++;
            return;
        }

        SGTN * path[max_size];
        auto path_len = 0;
        auto & height = path_len;

        auto cursor = this->root;
        while (true) {
            if (cursor->obj->operator==(obj)) {
                cursor->obj = obj;
                return;
            }

            path[path_len] = cursor;
            path_len++;
            if (obj->operator<(cursor->obj)) {
                cursor = cursor->small;

                if (cursor == NULL) {
                    cursor = path[path_len - 1]->small = mk_node();
                    this->size++;
                    break;
                }
            } else {
                cursor = cursor->big;

                if (cursor == NULL) {
                    cursor = path[path_len - 1]->big = mk_node();
                    this->size++;
                    break;
                }
            }
        }
        assert(this->size <= max_size);

        if (height > (log(this->size) / log(1 / SGT_FACTOR))) {
            this->rebuild(this->find_scapegoat(path, path_len, cursor));
        }
    };

    T * getitem(T * obj) {
        auto cursor = this->root;
        while (true) {
            if (cursor == NULL) {
                return NULL;
            }
            if (cursor->obj->operator==(obj)) {
                return cursor->obj;
            }

            if (obj->operator<(cursor->obj)) {
                cursor = cursor->small;
            } else {
                cursor = cursor->big;
            }
        }
    };

    struct fsg_ret {
        SGTN * pa;
        SGTN * scapegoat;
        int size;
    };

    fsg_ret find_scapegoat(SGTN * path[], int path_len, SGTN * cursor) {
        fsg_ret ret;
        auto size = 1;
        auto height = 0;

        while (path_len) {
            auto parent = path[path_len - 1];
            path_len--;

            SGTN * sibling;
            if (parent->small == cursor) {
                sibling = parent->big;
            } else {
                assert(parent->big == cursor);
                sibling = parent->small;
            }

            height++;
            size += this->get_size(sibling) + 1;
            if (height > (log(size) / log(1 / SGT_FACTOR))) {
                ret.scapegoat = parent;
                break;
            }
            cursor = parent;
        }

        ret.size = size;
        ret.pa = path_len ? path[path_len - 1] : NULL;
        return ret;
    }

    void rebuild(fsg_ret ret) {
        auto pa = ret.pa;
        auto scapegoat = ret.scapegoat;
        auto size = ret.size;
        assert(this->get_size(scapegoat) == size);

        SGTN * ordered_nodes[max_size];
        auto i = 0;
        std::function<void(SGTN *)> add = [&](SGTN * node) {
            if (node->small != NULL) {
                add(node->small);
            }
            ordered_nodes[i] = node;
            i++;
            if (node->big != NULL) {
                add(node->big);
            }
        };
        add(scapegoat);
        assert(i == size);

        std::function<SGTN *(int, int)> build_tree = [&](int op, int ed) -> SGTN * {
            if (op > ed) { return NULL; }
            if (op == ed) {
                ordered_nodes[op]->small = ordered_nodes[op]->big = NULL;
                return ordered_nodes[op];
            }

            int mid = (op + ed) / 2;
            auto mid_node = ordered_nodes[mid];
            mid_node->small = build_tree(op, mid - 1);
            mid_node->big = build_tree(mid + 1, ed);
            return mid_node;
        };

        auto sub = build_tree(0, size - 1);
        if (pa == NULL) {
            this->root = sub;
        } else if (pa->small == scapegoat) {
            pa->small = sub;
        } else {
            assert(pa->big = scapegoat);
            pa->big = sub;
        }
    }

    int get_size(SGTN * node) {
        if (node == NULL) {
            return 0;
        }
        auto size_small = this->get_size(node->small);
        auto size_big = this->get_size(node->big);
        return size_small + size_big + 1;
    }
};

#endif