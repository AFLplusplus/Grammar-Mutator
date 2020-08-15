#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "helpers.h"
#include "tree.h"
#include "custom_mutator.h"
#include "f1_c_fuzz.h"
#include "tree_mutation.h"
#include "tree_trimming.h"
#include "chunk_store.h"

my_mutator_t *afl_custom_init(afl_t *afl, unsigned int seed) {
  srandom(seed);

  chunk_store_init();

  my_mutator_t *data = (my_mutator_t *)calloc(1, sizeof(my_mutator_t));
  if (!data) {
    perror("afl_custom_init alloc");
    return NULL;
  }

  data->afl = afl;

  return data;
}

void afl_custom_deinit(my_mutator_t *data) {
  if (data->tree_cur) tree_free(data->tree_cur);
  if (data->mutated_tree) tree_free(data->mutated_tree);
  if (data->trimmed_tree) tree_free(data->trimmed_tree);

  data->cur_trimming_stage = 0;
  data->cur_subtree_trimming_step = 0;
  data->total_subtree_trimming_steps = 0;
  data->cur_recursive_trimming_step = 0;
  data->total_recursive_trimming_steps = 0;

  free(data->fuzz_buf);
  free(data);

  chunk_store_clear();
}

// For each interesting test case in the queue
uint8_t afl_custom_queue_get(my_mutator_t *data, const uint8_t *filename) {
  const char *fn = (const char *)filename;
  size_t      fn_len = strlen(fn);
  data->filename_cur = filename;
  if (data->tree_cur) {
    // Clear the previous tree
    tree_free(data->tree_cur);
  }
  data->tree_cur = NULL;

  // Check the tree output directory
  if (unlikely(data->tree_out_dir_exist == 0)) {
    const char *last_slash = strrchr(fn, '/');
    if (unlikely(!last_slash)) {
      // Should not reach here
      perror("Invalid filename (afl_custom_queue_get)");
      return 0;
    }

    // Set the tree output directory
    char *tree_out_dir = strndup(fn, last_slash - fn);
    char *found = strstr(tree_out_dir, "/queue");
    if (unlikely(!found)) {
      // Should not reach here
      perror("Invalid filename (afl_custom_queue_get)");
      return 0;
    }
    // Replace "queue" with "trees"
    strncpy(found + 1, "trees", 5);

    // Check whether the directory exists
    struct stat info;
    if (stat(tree_out_dir, &info) != 0) {
      if (mkdir(tree_out_dir, 0700) != 0) {
        // error
        perror("Cannot create the directory");
        return 0;
      }
      data->tree_out_dir_exist = 1;
    } else if (info.st_mode & S_IFDIR) {
      data->tree_out_dir_exist = 1;
    } else {
      // Not a directory
      perror("Wrong tree output path (stat)");
      return 0;
    }

    free(tree_out_dir);
  }

  strncpy(data->tree_fn_cur, fn, fn_len);
  char *found = strstr(data->tree_fn_cur, "/queue/");
  if (unlikely(!found)) {
    // Should not reach here
    perror("Invalid filename (afl_custom_queue_get)");
    return 0;
  }
  // Replace "queue" with "trees"
  strncpy(found + 1, "trees", 5);

  // Read the corresponding serialized tree from file
  int fd = open(data->tree_fn_cur, O_RDONLY);
  if (unlikely(fd < 0)) {
    // TODO: no tree file
    // Parse the tree from a test case
    return 1;
  }
  struct stat info;
  if (unlikely(fstat(fd, &info) != 0)) {
    // error
    perror("Cannot get file information");
    return 0;
  }
  size_t   tree_file_size = info.st_size;
  uint8_t *tree_buf = (uint8_t *)mmap(0, tree_file_size, PROT_READ | PROT_WRITE,
                                      MAP_PRIVATE, fd, 0);
  if (unlikely(tree_buf == MAP_FAILED)) {
    perror("Cannot map the tree file to the memory");
    return 0;
  }
  close(fd);

  // Deserialize the data to recover the tree
  data->tree_cur = tree_deserialize(tree_buf, tree_file_size);
  munmap(tree_buf, tree_file_size);
  if (unlikely(!data->tree_cur)) {
    perror("Cannot deserialize the data");
    return 0;
  }

  tree_get_size(data->tree_cur);

  return 1;
}

// Trimming
int32_t afl_custom_init_trim(my_mutator_t *data, uint8_t *buf,
                             size_t buf_size) {
  if (!data->tree_cur) return 0;

  tree_get_non_terminal_nodes(data->tree_cur);
  tree_get_recursion_edges(data->tree_cur);

  data->cur_trimming_stage = 0;

  data->cur_subtree_trimming_step = 0;
  data->total_subtree_trimming_steps = data->tree_cur->root->non_term_size;

  data->cur_recursive_trimming_step = 0;
  data->total_recursive_trimming_steps =
      data->tree_cur->root->recursion_edge_size;

  return data->total_subtree_trimming_steps +
         data->total_recursive_trimming_steps;
}

size_t afl_custom_trim(my_mutator_t *data, uint8_t **out_buf) {
  tree_t *trimmed_tree = NULL;
  size_t  trimmed_size = 0;
  tree_t *tree_cur = data->tree_cur;

  if (data->cur_trimming_stage == 0) {
    // subtree trimming
    node_t *node = (node_t *)list_pop_front(tree_cur->non_terminal_node_list);
    trimmed_tree = subtree_trimming(tree_cur, node);

    ++data->cur_subtree_trimming_step;
  } else if (data->cur_trimming_stage == 1) {
    // recursive trimming
    edge_t *edge = (edge_t *)list_pop_front(tree_cur->recursion_edge_list);
    trimmed_tree = recursive_trimming(tree_cur, *edge);
    free(edge);

    ++data->cur_recursive_trimming_step;
  } else {
    // should not reach here
    *out_buf = NULL;
    perror("wrong trimming stage");
    return 0;
  }

  tree_to_buf(trimmed_tree);
  tree_get_size(trimmed_tree);
  data->trimmed_tree = trimmed_tree;

  // maybe_grow is optimized to be quick for reused buffers.
  trimmed_size = trimmed_tree->data_len;
  uint8_t *trimmed_out =
      (uint8_t *)maybe_grow(BUF_PARAMS(data, fuzz), trimmed_size);
  if (!trimmed_out) {
    *out_buf = NULL;
    perror("custom mutator allocation (maybe_grow)");
    return 0; /* afl-fuzz will very likely error out after this. */
  }

  memcpy(trimmed_out, trimmed_tree->data_buf, trimmed_size);
  *out_buf = trimmed_out;

  return trimmed_size;
}

int32_t afl_custom_post_trim(my_mutator_t *data, int success) {
  if (success) {
    // Update the trimming step
    size_t remaining_node_size = data->tree_cur->non_terminal_node_list->size;
    size_t remaining_edge_size = data->tree_cur->recursion_edge_list->size;

    // Update the corresponding tree file
    tree_serialize(data->trimmed_tree);
    size_t   ser_len = data->trimmed_tree->ser_len;
    uint8_t *ser_buf = data->trimmed_tree->ser_buf;
    write_tree_to_file(data->tree_fn_cur, ser_buf, ser_len, 1);

    tree_free(data->tree_cur);
    data->tree_cur = data->trimmed_tree;

    // Update the non-terminal node list
    tree_get_non_terminal_nodes(data->tree_cur);
    for (int i = 0; i < data->cur_subtree_trimming_step; ++i)
      list_pop_front(data->tree_cur->non_terminal_node_list);
    size_t new_node_size = data->tree_cur->non_terminal_node_list->size;

    data->cur_subtree_trimming_step += (remaining_node_size - new_node_size);

    // update the recursion edge list
    tree_get_recursion_edges(data->tree_cur);
    size_t new_edge_size = data->tree_cur->recursion_edge_list->size;

    data->cur_recursive_trimming_step += (remaining_edge_size - new_edge_size);
  } else {
    // the trimmed tree will not be saved, so destroy it
    tree_free(data->trimmed_tree);
  }
  data->trimmed_tree = NULL;

  // update trimming stage
  if (data->cur_trimming_stage == 0) {
    if (data->cur_subtree_trimming_step >= data->total_subtree_trimming_steps)
      ++data->cur_trimming_stage;
  }

  if (data->cur_trimming_stage == 1) {
    if (data->cur_recursive_trimming_step >=
        data->total_recursive_trimming_steps)
      ++data->cur_trimming_stage;
  }

  return data->cur_subtree_trimming_step + data->cur_recursive_trimming_step;
}

// Fuzz the given test case several times, which is defined by the
// `custom_mutator_stage` in `afl-fuzz-one.c`
size_t afl_custom_fuzz(my_mutator_t *data, uint8_t *buf, size_t buf_size,
                       uint8_t **out_buf, uint8_t *add_buf,
                       size_t add_buf_size,  // add_buf can be NULL
                       size_t max_size) {
  tree_t *tree = NULL;
  size_t  mutated_size = 0;

  if (data->mutated_tree) {
    /* `data->mutated_tree` is not NULL, meaning that this is not an interesting
      mutation (`afl_custom_queue_new_entry` is not invoked). Therefore, we
      need to free the memory. */
    tree_free(data->mutated_tree);
    data->mutated_tree = NULL;
  }

  tree = data->tree_cur;
  int mutation_choice = -1;
  if (!tree) {
    // Generation
    // Randomly generate a JSON string
    tree = gen_init__(1000);
  } else {
    mutation_choice = random() % 3;
    switch (mutation_choice) {
      case 0:
        // random mutation
        tree = random_mutation(tree);
        break;
      case 1:
        // random recursive mutation
        tree = random_recursive_mutation(tree, random() % 6);
        break;
      case 2:
        // splicing mutation
        tree = splicing_mutation(tree);
        break;
      default:
        perror("mutation error (invalid choice)");
        break;
    }

    if (!tree) {
      perror("mutation error");
      return 0;
    }
  }

  tree_to_buf(tree);
  tree_get_size(tree);
  data->mutated_tree = tree;
  mutated_size = tree->data_len <= max_size ? tree->data_len : max_size;

  // maybe_grow is optimized to be quick for reused buffers.
  uint8_t *mutated_out =
      (uint8_t *)maybe_grow(BUF_PARAMS(data, fuzz), mutated_size);
  if (!mutated_out) {
    *out_buf = NULL;
    perror("custom mutator allocation (maybe_grow)");
    return 0; /* afl-fuzz will very likely error out after this. */
  }

  memcpy(mutated_out, tree->data_buf, mutated_size);
  *out_buf = mutated_out;
  return mutated_size;
}

// Save interesting mutated test cases
void afl_custom_queue_new_entry(my_mutator_t * data,
                                const uint8_t *filename_new_queue,
                                const uint8_t *filename_orig_queue) {
  // Skip if we read from initial test cases (i.e., from input directory)
  if (!filename_orig_queue) return;

  const char *fn = (const char *)filename_new_queue;
  size_t      fn_len = strlen(fn);
  strncpy(data->new_tree_fn, fn, fn_len);
  char *found = strstr(data->new_tree_fn, "/queue/");
  if (unlikely(!found)) {
    // Should not reach here
    perror("Invalid filename_new_queue (afl_custom_queue_new_entry)");
    return;
  }
  // Replace "queue" with "trees"
  strncpy(found + 1, "trees", 5);

  // Serialize the mutated tree
  tree_serialize(data->mutated_tree);

  // Write the serialized tree to the file
  size_t   ser_len = data->mutated_tree->ser_len;
  uint8_t *ser_buf = data->mutated_tree->ser_buf;
  write_tree_to_file(data->new_tree_fn, ser_buf, ser_len, 0);

  // Store all subtrees in the newly added tree
  chunk_store_add_tree(data->mutated_tree);

  /* Once the test case is added into the queue, we will clear `mutated_tree` */
  tree_free(data->mutated_tree);
  data->mutated_tree = NULL;
}
