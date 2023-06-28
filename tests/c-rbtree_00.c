/*
    Makes a pretty dot Graphviz image of a red black tree

    Uses c-rbtree from https://github.com/c-util/c-rbtree
    Many thanks to the AUTHORS of c-rbtree.

  run%  make
  run%  ./c-rbtree_00 | display

*/
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "../lib/debug.h"
#include "../lib/c-rbtree.h"



struct Node {
        const char *key;
        CRBNode rb;
};


static
void link(CRBTree *tree, struct Node *node) {

    CRBNode **i = &tree->root;
    CRBNode *parent = 0;

    while(*i) {

        parent = *i;

        struct Node *entry = c_rbnode_entry(*i, struct Node, rb);

        int cmp = strcmp(node->key, entry->key);
        if(cmp < 0)
            i = &parent->left;
        else if(cmp > 0)
            i = &parent->right;
        else {
            fprintf(stderr, "Removing old \"%s\"\n", entry->key);
            c_rbnode_unlink(&entry->rb);
            // Try again, it should work now given we removed the entry
            // that had that key.  We don't try to use a previous i and
            // parent, because the c_rbnode_unlink() above may have
            // changed the tree to fix the red black tree balancing
            // requirements; moving many node around.
            link(tree, node);
            return;
        }
    }

    fprintf(stderr, "Adding \"%s\"\n", node->key);

    c_rbtree_add(tree, parent, i, &node->rb);
}


static
bool unlink(CRBTree *tree, const char *key) {

    CRBNode *i = tree->root;
    struct Node *entry;

    while(i) {

        entry = c_rbnode_entry(i, struct Node, rb);

        int cmp = strcmp(key, entry->key);
        if(cmp < 0)
            i = i->left;
        else if(cmp > 0)
            i = i->right;
        else if(cmp == 0)
            break;
    }
    if(!i) return false;

    fprintf(stderr, "Removing \"%s\"\n", entry->key);

    c_rbnode_unlink(&entry->rb);

    return true;
}


static
struct Node *find(CRBTree *tree, const char *key) {
        CRBNode *i;
        struct Node *entry;

        i = tree->root;
        while (i) {
                entry = c_rbnode_entry(i, struct Node, rb);

                int cmp = strcmp(key, entry->key);
                if (cmp < 0)
                        i = i->left;
                else if (cmp > 0)
                        i = i->right;
                else
                        return entry;
        }

        return NULL;
}


static
void Catcher(int sig) {

    ERROR("Caught signal %d", sig);
    ASSERT(0);
}


// Recursive Print().
static
void Print(const struct Node *node) {

    if(node->rb.left) {
        struct Node *left = c_rbnode_entry(node->rb.left, struct Node, rb);
        printf("  %s -> %s;\n", node->key, left->key);
        Print(left);
    }
    if(node->rb.right) {
        struct Node *right = c_rbnode_entry(node->rb.right, struct Node, rb);
        printf("  %s -> %s;\n", node->key, right->key);
        Print(right);
    }
}


static
bool CheckEntry(CRBTree *tree, const char *key) {

    struct Node *n = find(tree, key);
    return (n && strcmp(n->key, key) == 0)?true:false;
}



int main(void) {

    signal(SIGSEGV, Catcher);
    signal(SIGABRT, Catcher);

    const char * const keys[] = {
        "a", "b", "c", "a", "b", "b", "b",
        "d", "ee", "f", "g", "h",
        "i", "jj", "k", "l", "m",
        "n", "o", "p", "q", "r", "s",
        "t", "u", "vv", "w", "x", "y",
        "zzzzzzz",
        0};

    size_t num = 0;
    for(const char * const *k = keys; *k; ++k)
        fprintf(stderr, "key[%zu]=%s\n", num++, *k);

    struct Node nodes[num];
    memset(nodes, 0, num*sizeof(*nodes));
    CRBTree tree = C_RBTREE_INIT;

    // Put data in the rbtree nodes and
    // build the red black tree.
    for(size_t i = 0; i<num; ++i) {
        nodes[i].key = keys[i];
        link(&tree, nodes + i);
    }

    ASSERT(unlink(&tree, "p") == true);
    // See if the "p" node goes away.
    ASSERT(unlink(&tree, "p") == false);
    
    ASSERT(CheckEntry(&tree, "a"));
    ASSERT(CheckEntry(&tree, "y"));
    ASSERT(CheckEntry(&tree, "p") == false);


    printf("digraph {\n");

    struct Node *entry = c_rbnode_entry(tree.root, struct Node, rb);
    printf("  %s;\n", entry->key);
    Print(entry);
    printf("}\n");


    return 0;
}
