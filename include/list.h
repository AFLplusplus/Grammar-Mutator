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

typedef void (*data_free_t)(void *data);

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
 * Destroy the linked list and free all stored data
 * @param list      A doubly linked list
 * @param free_func A pointer to the function for destroying the stored data in
 *                  each list node
 */
void list_free_with_data_free_func(list_t *list, data_free_t free_func);

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

/**
 * Remove the first node in the list and return the corresponding data
 * @param list A doubly linked list
 * @return     The data in the first node
 */
void *list_pop_front(list_t *list);

/**
 * Get the data of the i-th node in the list
 * @param list A doubly linked list
 * @param i    The index of the required node. The range is [0, list->size)
 * @return     The data in the i-th node
 */
void *list_get(list_t *list, size_t i);

/**
 * Is the given list empty?
 * @param list A doubly linked list
 * @return     True if the list is empty; otherwise, False
 */
bool list_empty(list_t *list);

#ifdef __cplusplus
}
#endif

#endif
