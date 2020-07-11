#ifndef __LIST_H__
#define __LIST_H__

#include <unistd.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// a doubly linked list
typedef struct list_node list_node_t;
struct list_node {
  list_node_t *next, *prev;
  void *       data;
};

typedef struct list list_t;
struct list {
  list_node_t *head, *tail;
  size_t       size;
};

/**
 * Create a doubly linked list
 * @return A doubly linked list
 */
list_t *list_create();

/**
 * Destroy the linked list and free all memory
 * @param list A doubly linked list
 */
void list_free(list_t *list);

/**
 * Add a new node at the beginning of the list
 * @param list A doubly linked list
 * @param data The data to be added
 * @return     The newly created list node
 */
list_node_t *list_insert(list_t *list, void *data);

/**
 * Add a new node at the end of the list
 * @param list A doubly linked list
 * @param data The data to be added
 * @return     The newly created list node
 */
list_node_t *list_append(list_t *list, void *data);

/**
 * Remove a node, carrying the same `data`, from the list
 * @param list A doubly linked list
 * @param data The data to be removed
 * @return     True if the data exists in the list; otherwise, False
 */
bool list_remove(list_t *list, void *data);

#ifdef __cplusplus
}
#endif

#endif
