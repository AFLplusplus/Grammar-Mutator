/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Written by Shengtuo Hu

   Copyright 2020 AFLplusplus Project. All rights reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

   A grammar-based custom mutator written for GSoC '20.

 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "list.h"

list_t *list_create() {

  return calloc(1, sizeof(list_t));

}

void list_free(list_t *list) {

  list_free_with_data_free_func(list, NULL);

}

void list_free_with_data_free_func(list_t *list, data_free_t free_func) {

  if (!list) return;

  list_node_t *cur = list->head;
  list_node_t *next = NULL;

  while (cur) {

    next = cur->next;
    cur->next = NULL;
    cur->prev = NULL;
    if (free_func) free_func(cur->data);
    cur->data = NULL;
    free(cur);
    cur = next;

  }

  list->head = NULL;
  list->tail = NULL;
  list->size = 0;
  free(list);

}

list_node_t *list_insert(list_t *list, void *data) {

  if (!list) return NULL;

  list_node_t *node = malloc(sizeof(list_node_t));
  if (!node) {

    perror("list_insert (malloc)");
    return NULL;

  }

  node->data = data;
  node->next = list->head;
  node->prev = NULL;

  if (!list->tail) {

    list->tail = node;

  } else {

    list->head->prev = node;

  }

  list->head = node;
  ++list->size;

  return node;

}

list_node_t *list_append(list_t *list, void *data) {

  if (!list) return NULL;

  list_node_t *node = malloc(sizeof(list_node_t));
  if (!node) {

    perror("list_insert (malloc)");
    return NULL;

  }

  node->data = data;
  node->next = NULL;
  node->prev = list->tail;

  if (!list->head) {

    list->head = node;

  } else {

    list->tail->next = node;

  }

  list->tail = node;
  ++list->size;

  return node;

}

bool list_remove(list_t *list, void *data) {

  if (!list) return false;
  if (list->size == 0 || list->head == NULL) return false;

  list_node_t *cur = list->head;
  list_node_t *next = NULL;

  while (cur) {

    next = cur->next;
    if (cur->data == data) {

      if (cur == list->head) list->head = cur->next;
      if (cur == list->tail) list->tail = cur->prev;

      if (cur->prev) cur->prev->next = cur->next;
      if (cur->next) cur->next->prev = cur->prev;

      cur->next = NULL;
      cur->prev = NULL;
      cur->data = NULL;
      free(cur);

      --list->size;
      return true;

    }

    cur = next;

  }

  return false;

}

void *list_pop_front(list_t *list) {

  if (!list) return NULL;
  if (list->size == 0 || list->head == NULL) return NULL;

  list_node_t *head = list->head;
  void *       data = head->data;

  list->head = head->next;
  if (head == list->tail) {

    // only one element
    list->tail = NULL;

  } else {

    // more than one element
    head->next->prev = NULL;

  }

  head->next = NULL;
  head->prev = NULL;
  head->data = NULL;
  free(head);

  --list->size;

  return data;

}

void *list_get(list_t *list, size_t i) {

  if (!list) return NULL;
  if (list->size == 0 || list->head == NULL) return NULL;
  if (i >= list->size) return NULL;

  list_node_t *cur = list->head;

  for (uint32_t j = 0; j < i; ++j) {

    cur = cur->next;

  }

  return cur->data;

}

inline bool list_empty(list_t *list) {

  if (!list) return true;
  if (list->size == 0 || list->head == NULL) return true;
  return false;

}
