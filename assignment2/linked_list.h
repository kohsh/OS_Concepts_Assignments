#ifndef LINKED_LIST_H
#define LINKED_LIST_H


typedef void (*FreeFunction)(void *);
typedef void (*NodeOperation)(void *);

typedef struct _Node {
    void *data;
    struct _Node *next;
} Node;

typedef struct _List {
    int length;
    size_t nodeSize;
    Node *head;
    Node *tail;
    FreeFunction freeFun;
} List;

void ll_malloc(List *list, size_t nodeSize, FreeFunction freeFun);
void ll_free(List *list);
void ll_add(List *list, void *data);
void ll_forEach(List *list, NodeOperation operation);

#endif //LINKED_LIST_H
