#include <stdlib.h>
#include <string.h>
#include "linked_list.h"
#include "memwatch.h"

void ll_init(List *list, size_t nodeSize) {
    list->length = 0;
    list->head = NULL;
    list->tail = NULL;
    list->nodeSize = nodeSize;
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
            // remove the node
            // requires the node prior to node being removed
            // if the first node, set list->head to list->head->next
            // if last node, set list->tail to previousNode
            // if other node, set previousNode->next to node->next
            // move to next node, keep previousNode the same
            // free(node->data); free(node);
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

            free(temp->data);
            free(temp);
        }
        else {
            previousNode = node;
            node = node->next;
        }
    }
}
