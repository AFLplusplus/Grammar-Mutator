#include <stdlib.h>
#include <stdio.h>

#include "list.h"

list_t *list_create() {
  return calloc(1, sizeof(list_t));
}

void list_free(list_t *list) {
  if (!list) return;

  list_node_t *cur = list->head;
  list_node_t *next = NULL;

  while (cur) {
    next = cur->next;
    cur->next = NULL;
    cur->prev = NULL;
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
