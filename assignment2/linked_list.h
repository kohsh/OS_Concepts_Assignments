#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <stdbool.h>

typedef void (*NodeOperation)(void *);

// returns true if the predicate is fulfilled
typedef bool (*Predicate)(void *);

// returns true if the two items are equal in value
typedef bool (*Comparator)(void *, void *);

typedef struct _Node {
    void *data;
    struct _Node *next;
} Node;

typedef struct _List {
    int length;
    size_t nodeSize;
    Node *head;
    Node *tail;
    Comparator comparator;
} List;

void    ll_init(List *list, size_t nodeSize, Comparator comparator);
void    ll_free(List *list);
void    ll_add(List *list, void *data);
void    ll_add_unique(List *list, void *data);
void    ll_forEach(List *list, NodeOperation operation);
void    ll_removeIf(List *list, Predicate operation);
int     ll_size(List *list);

#endif //LINKED_LIST_H

