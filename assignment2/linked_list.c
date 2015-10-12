#include <stdlib.h>
#include <string.h>
#include "linked_list.h"
#include "memwatch.h"

void ll_malloc(List *list, size_t nodeSize, FreeFunction freeFun) {
    list->length = 0;
    list->head = NULL;
    list->tail = NULL;
    list->freeFun = freeFun;
    list->nodeSize = nodeSize;
}

void ll_free(List *list) {
    Node *temp;
    while(list->head != NULL) {
        temp = list->head;
        list->head = temp->next;
        if(list->freeFun) {
            list->freeFun(temp->data);
        }
        free(temp->data);
        free(temp);
    }
}

void ll_add(List *list, void *data) {
    Node *node = malloc(sizeof(Node));
    node->data = malloc(list->nodeSize);
    node->next = NULL;

    memcpy(node->data, data, list->nodeSize);

    if(list->length == 0) {
        list->head = list->tail = node;
    } else {
        list->tail->next = node;
        list->tail = node;
    }

    list->length++;
}

void ll_forEach(List *list, NodeOperation operation) {
    if (operation == NULL) {
        return;
    }
    Node *node = list->head;
    while(node != NULL) {
        operation(node->data);
        node = node->next;
    }
}

