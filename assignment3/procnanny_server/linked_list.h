/**
 * Copyright 2015 Kyle O'Shaughnessy
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

// if no comparator is supplied in ll_init, an ll_add will be performed
void    ll_add_unique(List *list, void *data);

void    ll_forEach(List *list, NodeOperation operation);

void*   ll_getIf(List *list, Predicate operation);

// if no comparator is supplied in ll_init, no Node will be removed
void    ll_remove(List *list, void* data);

void    ll_removeIf(List *list, Predicate operation);

int     ll_size(List *list);

#endif //LINKED_LIST_H

