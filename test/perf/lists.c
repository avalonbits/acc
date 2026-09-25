/* Linked structures: a sorted linked list and a binary search tree built
 * from a static pool of nodes, walked, searched and summed -- structs
 * reached through pointers, recursion, and pointers to pointers. */
#include "perf.h"

#define NODES 600

struct node {
    int key;
    struct node *next;
    struct node *left, *right;
};

static struct node pool[NODES];

static void insert_sorted(struct node **head, struct node *n)
{
    while (*head && (*head)->key < n->key)
        head = &(*head)->next;
    n->next = *head;
    *head = n;
}

static void insert_tree(struct node **root, struct node *n)
{
    while (*root)
        root = n->key < (*root)->key ? &(*root)->left : &(*root)->right;
    *root = n;
}

static unsigned long walk(const struct node *n, int depth)
{
    if (!n)
        return 0;

    return walk(n->left, depth + 1) + (unsigned long) (n->key * depth)
           + walk(n->right, depth + 1);
}

static int find(const struct node *n, int key)
{
    int steps = 0;

    while (n && n->key != key) {
        n = key < n->key ? n->left : n->right;
        steps++;
    }

    return n ? steps : -1;
}

int main(void)
{
    unsigned long state = perf_seed, check = 0;
    struct node *head = 0, *root = 0;

    for (int i = 0; i < NODES; i++) {
        state = state * 1103515245UL + 12345UL;
        pool[i].key = (int) (state >> 12 & 0x3fff);
    }

    perf_start();
    for (int i = 0; i < NODES; i++) {
        insert_sorted(&head, &pool[i]);
        insert_tree(&root, &pool[i]);
    }
    for (const struct node *n = head; n; n = n->next)
        check = check * 3 + (unsigned long) n->key;
    check += walk(root, 1);
    for (int i = 0; i < NODES; i += 3)
        check += (unsigned long) find(root, pool[i].key);
    perf_stop();

    perf_check(check);

    return 0;
}
