#include <stdlib.h>
#include <string.h>
#include "linked_list.h"
#include "memwatch.h"

void ll_init(List *list, size_t nodeSize, Comparator comparator) {
    list->length = 0;
    list->head = NULL;
    list->tail = NULL;
    list->nodeSize = nodeSize;
    list->comparator = comparator;
}

void ll_free(List *list) {
    Node *temp;
    while(list->head != NULL) {
        temp = list->head;
        list->head = temp->next;
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

void ll_add_unique(List *list, void *data)  {
    if (list->comparator == NULL) {
        ll_add(list, data);
        return;
    }
    Node *node = list->head;
    bool hasDuplicate = false;
    while((node != NULL) && (hasDuplicate == false)) {
        hasDuplicate = list->comparator(node->data, data);
        node = node->next;
    }

    if (hasDuplicate == false) {
        ll_add(list, data);
    }
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

void ll_removeIf(List *list, Predicate operation) {
    if (operation == NULL) {
        return;
    }
    Node* node = list->head;
    Node* previousNode = list->head;
    while(node != NULL) {
        bool remove = operation(node->data);
        if (remove) {
            Node* temp = node;
            if (node == list->head) {
                list->head = node->next;
                previousNode = list->head;
                node = list->head;
            }
            else if(node == list->tail) {
                list->tail = previousNode;
                previousNode->next = NULL;
                node = NULL;
            }
            else {
                previousNode->next = node->next;
                node = previousNode->next;
            }
            list->length--;
            free(temp->data);
            free(temp);
        }
        else {
            previousNode = node;
            node = node->next;
        }
    }
}

void ll_remove(List *list, void *data) {
    if (list->comparator == NULL) {
        return;
    }
    Node* node = list->head;
    Node* previousNode = list->head;
    while(node != NULL) {
        bool remove = list->comparator(node->data, data);
        if (remove) {
            Node* temp = node;
            if (node == list->head) {
                list->head = node->next;
                previousNode = list->head;
                node = list->head;
            }
            else if(node == list->tail) {
                list->tail = previousNode;
                previousNode->next = NULL;
                node = NULL;
            }
            else {
                previousNode->next = node->next;
                node = previousNode->next;
            }
            list->length--;
            free(temp->data);
            free(temp);
        }
        else {
            previousNode = node;
            node = node->next;
        }
    }
}

int ll_size(List *list) {
    return list->length;
}

void* ll_getIf(List *list, Predicate operation) {
    if (operation == NULL) {
        return NULL;
    }
    Node *node = list->head;
    while(node != NULL) {
        if (operation(node->data) == true) {
            return node->data;
        }
        node = node->next;
    }

    return NULL;
}
