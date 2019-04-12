#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "bplus_tree.h"

/*
 * forward declarations
 */
bplus_tree_node_t *
adjust_parent (bplus_tree_node_t *root,
             bplus_tree_node_t *leaf,
             bplus_tree_node_t *new_leaf,
             int parent_key);

static bplus_tree_node_t *
delete_key_from_node (bplus_tree_node_t *root,
                      bplus_tree_node_t *node,
                      bplus_tree_node_t *child,
                      int key);

/********************
 * global variables *
 ********************/
bplus_tree_t    *tree;

/************************
 * Queue data structure *
 ************************/
static inline qnode_t*
new_qnode (void *data)
{
  qnode_t *new_node = NULL;

  new_node = malloc(sizeof(qnode_t));
  if (!new_node) {
    printf ("%s: Error: Could not allocate memory\n", __FUNCTION__);
    return (NULL);
  }

  new_node->data = data;
  new_node->next = NULL;
  return new_node;
}

queue_t *
create_queue ()
{
  queue_t *new_queue = NULL;

  new_queue = malloc(sizeof(queue_t));
  if (!new_queue) {
    printf ("%s: Error: Could not allocate memory\n", __FUNCTION__);
    return (NULL);
  }

  new_queue->head = NULL;
  new_queue->tail = NULL;
  new_queue->is_empty = 1;
  new_queue->size = 0;

  return new_queue;
}

int
q_isempty (queue_t *q)
{
  return (q->is_empty);
}

void
enqueue (queue_t *q, void *data)
{
  qnode_t *new_node = NULL;

  if (!q)
    return;

  new_node = new_qnode(data);
  if (!new_node) {
    printf("%s: Error: could not create queue node\n", __FUNCTION__);
    return;
  }

  if (!q->tail) {
    q->is_empty = 0;
    q->head = new_node;
    q->tail = new_node;
    q->size++;
    return;
  }

  q->tail->next = new_node;
  q->tail = new_node;
  q->size++;
  return;
}

void *
dequeue (queue_t *q)
{
  void *data = NULL;
  qnode_t *node = NULL;

  if (!q)
    return (NULL);

  if (!q->head)
    return (NULL);

  node = q->head;
  q->head = q->head->next;

  if (!q->head) {
    q->is_empty = 1;
    q->tail = NULL;
  }

  data = node->data;
  q->size--;
  free(node);

  return (data);
}

/*****************************
 * Print tree in level order *
 *****************************/

/*
 * helper to print an index node
 */
void
print_index_node (bplus_tree_node_t *node)
{
  int num = 0;
  int i = 0;
  int *keys = NULL;

  if (!node)
    return;

  keys = node->u.index->keys;
  num = node->u.index->num;

  //printf("<< (%p : %p : %d) (", node->parent, node, num);
  printf("<<");
  for (i = 0; i < num ; i++) {

    printf("%d ", keys[i]);
  }
  printf(">> ");

  return;
}

/*
 * helper to print a leaf node
 */
void
print_leaf_node (bplus_tree_node_t *node)
{
  int i = 0;
  int num = 0;
  pair_t *pairs = NULL;

  if (!node)
    return;

  num = node->u.leaf->num;
  pairs = node->u.leaf->pairs;
  
  //printf("< (%p : %p : %d) ", node->parent, node, num);
  printf("<");
  for (i = 0 ; i < num; i++) {

    //printf("(%d : %f) ", pairs[i].key, pairs[i].data);
    printf("%d ", pairs[i].key);
  }
  printf(">");

  return;
}

/*
 * helper to print a node in b+ tree
 */
void
print_node (bplus_tree_node_t *node)
{
  if (node->is_leaf)
    print_leaf_node(node);
  else
    print_index_node(node);
}

/*
 * driver function for printing the b+ tree
 */
void
print_tree_util (bplus_tree_node_t *root)
{
  queue_t *q = NULL;

  if (!root)
    return;

  q = create_queue();
  if (!q) {
    printf("%s: Error creating queue\n", __FUNCTION__);
    return;
  }
  
  enqueue(q, (void *)root);
  while (!q->is_empty) {
    
    int count = q->size;

    while (count > 0) {
      
      bplus_tree_node_t *node = NULL;

      node = dequeue(q);
      if (node->is_leaf) {
        print_leaf_node(node);
      } else {
       
        int i = 0;
        int num = node->u.index->num;
        
        print_index_node(node);

        /* index node has num + 1 child pointers */
        for (i = 0 ; i <= num; i++) {
          enqueue(q, (node->u.index->child[i]));
        }
      }

      count--;
    }
    
    printf("\n");
  }

  free(q);
}

/*
 * Function to print the b+ tree in level order
 */
void
print_tree (bplus_tree_t *tree)
{
  if (!tree) {
    return;
  }

  print_tree_util (tree->root);
  printf("\n");

  return;
}


/************************************
 * create/delete b plus tree node   *
 ************************************/

/*
 * free a leaf node
 */
static void
bplus_tree_delete_leaf_node (leaf_node_t *lnode)
{
    bplus_tree_node_t *next = NULL;
    bplus_tree_node_t *prev = NULL;
    
    if (!lnode)
        return;

    /* preserver the doubly link list */
    next = lnode->next;
    if (next != NULL)  
        next->u.leaf->prev = lnode->prev;  
  
    prev = lnode->prev;
    if (prev != NULL)  
        prev->u.leaf->next = lnode->next;   

    if (lnode->pairs)
        free(lnode->pairs);

    free(lnode);
    lnode = NULL;
    return;
}

/*
 * create a leaf node
 */
static leaf_node_t *
bplus_tree_create_leaf_node ()
{
    leaf_node_t *new_lnode;

    new_lnode = malloc(sizeof(leaf_node_t));
    if (!new_lnode) {
        printf("%s>Error: could not create index node", __FUNCTION__);
        return (NULL);
    }

    memset(new_lnode, 0, sizeof(leaf_node_t));
    new_lnode->prev = NULL;
    new_lnode->next = NULL;
    new_lnode->num = 0;

    /*
     * allocate an array for the <kev, value> pair
     */
    new_lnode->pairs = malloc(tree->order * sizeof(pair_t));
    if (!new_lnode->pairs) {
        printf("%s>Error: Could not create pairs\n", __FUNCTION__);
        free(new_lnode);
        return (NULL);
    }
    
    return (new_lnode);
}

/*
 * free index node
 */
static void
bplus_tree_delete_index_node (index_node_t *inode)
{
    if (!inode)
        return;

    if (inode->keys)
        free(inode->keys);

    if (inode->child)
        free(inode->child);

    free(inode);
    inode = NULL;
    return;
}

/*
 * create an index node
 */
static index_node_t *
bplus_tree_create_index_node ()
{
    index_node_t *new_inode = NULL;

    new_inode = malloc(sizeof(index_node_t));
    if (!new_inode) {
        printf("%s>Error: could not create index node", __FUNCTION__);
        return (NULL);
    }

    memset(new_inode, 0, sizeof(index_node_t));
    new_inode->num = 0;

    /*
     * allocate arrays for keys
     * TODO: this should be order -1
     */
    new_inode->keys = malloc(tree->order * sizeof(int));
    if (!new_inode->keys) {
        printf("%s>Error: Could not allocate keys\n", __FUNCTION__);
        free(new_inode);
        return (NULL);
    }
    memset(new_inode->keys, 0, tree->order * sizeof(int));

    
    /*
     * allocate child pointers
     */
    new_inode->child = malloc(tree->order * sizeof(void *));
    if (!new_inode->child) {
        printf("%s> Error: Could not allocate child pointers\n", __FUNCTION__);
        free(new_inode->keys);
        free(new_inode);
        return (NULL);
    }
    memset(new_inode->child, 0, tree->order * sizeof(void *));

    return (new_inode);
}

/*
 * free a bplus tree node
 */
static void
bplus_tree_delete_node (bplus_tree_node_t *node)
{
    if (!node)
        return;

    //TODO: check of empty

    if (node->is_leaf) {
        
        bplus_tree_delete_leaf_node(node->u.leaf);
    } else {
        bplus_tree_delete_index_node(node->u.index);
    }

    free(node);
}

/*
 * create a bplus tree node
 */
static bplus_tree_node_t *
bplus_tree_create_node (bool is_leaf)
{
    bplus_tree_node_t *new_node = NULL;

    new_node = malloc(sizeof(bplus_tree_node_t));
    if (!new_node) {
        printf("%s>Error: could not allocate memory for new node\n", __FUNCTION__);
        return;
    }

    memset(new_node, 0, sizeof(bplus_tree_node_t));
    new_node->is_leaf = is_leaf;
    new_node->parent = NULL;

    if (is_leaf) {

        /*
         * this is a leaf node,
         * allocate memory for the same
         */
        new_node->u.leaf = bplus_tree_create_leaf_node();
        if (!new_node->u.leaf) {
            
            printf("%s>Error: could not create leaf node\n", __FUNCTION__);
            free(new_node);
            return (NULL);
        }
    } else {

        /*
         * this is a index node,
         * allocate memory for the same
         */
        new_node->u.index = bplus_tree_create_index_node();
        if (!new_node->u.index) {
            
            printf("%s>Error: could not create leaf node\n", __FUNCTION__);
            free(new_node);
            return (NULL);
        }
    }
   
    return (new_node);
}

/*
 * free the memory of the allocated b tree
 */
void
bplus_tree_delete(bplus_tree_t **tree)
{
    if (!*tree)
        return;

    if (!is_tree_empty(*tree)) {
        printf("%s>Error: trying to free a non-empty tree\n", __FUNCTION__);
        return;
    }
    
    free(*tree);
    *tree = NULL;
}

/*
 * create a b plus tree
 * @param order     order of the tree (m in m-way tree)
 * @return pointer to the newly created b plus tree
 */
bplus_tree_t *
bplus_tree_create (int order)
{
    bplus_tree_t *new_tree = NULL;

    new_tree = malloc(sizeof(bplus_tree_t));
    if (!new_tree) {
        printf("%s>Error: Could not allocate memory for b-plus tree\n", __FUNCTION__);
        return (NULL);
    }

    memset(new_tree, 0, sizeof(bplus_tree_t));
    new_tree->order = order;
    new_tree->root = NULL;

    return (new_tree);
}

/********************************
 * Search a key from bplus tree *
 ********************************/

/*
 * utility function to get the correct child pointer
 */
static int
get_child_index_util (int *a,
		      int key,
                      int start, int end)
{
    int mid = 0;

    while (start != end) {
	//TODO: avoid overflow ?
	mid = (start + end) / 2;
	if (a[mid] <= key) {
	    start = mid + 1;
	} else {
	    end = mid;
	}
    }
    
    if (a[start] <= key) {
      return (++start);
    }
    return (start);
}

/*
 * helper to get the child pointer for a key
 */
static int
get_child_index (bplus_tree_node_t *node, int key)
{
    if (node->is_leaf) {
        printf("%s>Error: Invalid node type\n", __FUNCTION__);
    }
    
    return (get_child_index_util(node->u.index->keys, key,
                                 0, node->u.index->num - 1));
}

/*
 * Binary Serach for searching within a leaf node
 */
static int
binary_search (pair_t *pairs,
               int key,
               float *data,
               int start, int end)
{
    int mid = 0;

    if (start > end)
        return (-1);
    
    mid = (start + end)/2;
    if (pairs[mid].key == key) {
        *data = pairs[mid].data;
        return (mid);
    }

    if (key < pairs[mid].key) {
        return (binary_search(pairs, key, data, start, mid - 1));
    } else {
        return (binary_search(pairs, key, data, mid + 1, end));
    }

    return (-1);
}

/*
 * utility function to search a key within a leaf node
 */
static int
bplus_tree_search_in_leaf (bplus_tree_node_t *node,
                           int key,
                           float *data)
{
    int i = 0;

    if (!node->is_leaf) {
        printf("%s> Error: node is not a leaf\n", __FUNCTION__);
        return (-1);
    }

    return binary_search(node->u.leaf->pairs,
                         key, data, 0,
                         node->u.leaf->num - 1);
}

/*
 * utility function to search a key in the giveb b+ tree.
 * first get to the root in which the key might reside.
 * Then search for the key in that root
 */
static bool
bplus_tree_search_key_internal(bplus_tree_node_t *root,
                               int key,
                               float *data)
{
    int index = 0; /* index of child pointer where the key will  */

    if (!root) {
        return (false);
    }

    print_node(root);

    if (root->is_leaf) {
        index = bplus_tree_search_in_leaf(root, key, data);
        if (index == -1) {
          return (false);
        } else {
          return (true);
        }
    }

    index = get_child_index(root, key);
    return (bplus_tree_search_key_internal(root->u.index->child[index],
                                           key, data));
}

/*
 *  search a key in b plus tree
 *  @param tree bplus tree
 *  @paramm key key to search
 *  @param data data to be filled
 *  @return true - if key is present in the tree
 *  @return false - if key is not present in the tree
 */
bool
bplus_tree_search_key (bplus_tree_t *tree,
                       int key,
                       float *data)
{
    if (!tree) {
        printf("%s> Error: Invalid tree\n", __FUNCTION__);
        return -1;
    }

    if (!data) {
        printf("%s>Error: Invalid data ", __FUNCTION__);
    }

    *data = -1;
    return (bplus_tree_search_key_internal(tree->root, key, data));
}

/******************
 * Range Serach   *
 ******************/

/*
 * find the index of the key if found or
 * get the index of next largest element
 */
int
bplus_tree_range_search_find_index_in_leaf (pair_t *a,
                                            int low_key,
                                            int start, int end)
{
    int mid = 0;

    while (start != end) {
	
       //TODO: avoid overflow ?
	mid = (start + end) / 2;

        if (a[mid].key == low_key) {
          return (mid);
        }
	
        if (a[mid].key <= low_key) {
	    start = mid + 1;
	} else {
	    end = mid;
	}
    }
    
    if (a[start].key < low_key) {
      return (++start);
    }
    
    return (start);
}

/*
 * traverse the leaf and search for low_key,
 * one a match is found (low_key or the next higher key)
 * traverse the doubly linked list until high_key(including)
 */
void
bplus_tree_range_search_in_leaf (bplus_tree_node_t *root,
                                 int low_key,
                                 int high_key)
{
  
  bplus_tree_node_t *head = NULL;
  int lowest_key_in_leaf = -1;
  int highest_key_in_leaf = -1;
  int i = 0;
  int num = 0;
  int index = 0;
  pair_t *pairs;
  bool found = false;

  if (!root->is_leaf) {
    
    printf("Error: Non leaf node encountered\n");
    return;
  }

  num = root->u.leaf->num;
  lowest_key_in_leaf = root->u.leaf->pairs[0].key;
  highest_key_in_leaf = root->u.leaf->pairs[num - 1].key;

  /*
   * print <key, values> in this leaf
   */
  pairs = root->u.leaf->pairs;
  index = bplus_tree_range_search_find_index_in_leaf(pairs, low_key, 0, num - 1);
  for (i = index; i < num; i++) {

    if (pairs[i].key > high_key) {
      goto done;
    }
    
    if (!false)
      found = true;
    printf("<Key: %d, value: %f>\n", pairs[i]. key, pairs[i].data);
  }

  /*
   * traverse the list from this leaf
   * and see if they have keys in the range above
   */
  head = root->u.leaf->next;
  while (head) {
    
    pairs = head->u.leaf->pairs;
    num = head->u.leaf->num;
    for (i = 0; i < num; i++) {

      if (pairs[i].key > high_key) {
        goto done;
      }
      
      if (!found)
        found = true;
      printf("<Key: %d, value: %f>\n", pairs[i]. key, pairs[i].data);
    }
    head = head->u.leaf->next;
  }
 
done:
  if (!found)
    printf("Range not found\n");
  return;
}

/*
 * utility function to do a range search
 * First get the leaf node in which the low_key should reside
 * get all the keys starting from low_key til high_key(both including)
 * by traversing the doubly link list
 */
void
bplus_tree_range_search_internal (bplus_tree_node_t *root,
                                  int low_key,
                                  int high_key)
{
  int index = 0;

  if (!root) {
    return;
  }

  if (root->is_leaf) {
    return (bplus_tree_range_search_in_leaf (root, low_key, high_key));
  }

  index = get_child_index(root, low_key);
  return (bplus_tree_range_search_internal(root->u.index->child[index], low_key, high_key));
}

/*
 * return all keys such that
 * low_key <= key <= high_key
 */
void
bplus_tree_range_search (bplus_tree_t *tree,
                         int low_key,
                         int high_key)
{
  if (!tree) {
    return;
  }

  if (high_key < low_key) {
    printf("Please enter a valid range\n");
    return;
  }

  bplus_tree_range_search_internal (tree->root, low_key, high_key);
  
  return;
}

/*******************************
 *  B+ tree insert             *
 *******************************/

/*
 * utility function to add a key in index node
 */
static void
index_node_add_key (index_node_t *node, int key)
{
  int num = 0;

  num = node->num;
  node->keys[num++] = key;
  node->num = num;
}

/*
 * utility function add a (k, v) pair in leaf node
 */
static void
leaf_node_add_pair (leaf_node_t *node, int key, float value)
{
  int num = 0;

  num = node->num;
  node->pairs[num].key = key;
  node->pairs[num].data = value;
  node->num++;
}

/*
 * utility function to check if index node has room to add a new key
 */
static bool
index_has_room (bplus_tree_t *tree,
                bplus_tree_node_t *node)
{
  if (!tree) {
    printf("%s: Invalid tree\n", __FUNCTION__);
    return (false);
  }

  if (!node) {
    printf("%s: Invalid node\n", __FUNCTION__);
    return (false);
  }
  
  if (node->is_leaf) {
    printf("%s: Non-leaf node\n", __FUNCTION__);
    return (false);
  }

  return (node->u.index->num < (tree->order - 1));
}

/*
 * utility function to check if leaf node has roomm to add a new pair
 */
static bool
leaf_has_room (bplus_tree_t *tree,
               bplus_tree_node_t *node)
{
  if (!tree) {
    printf("%s: Invalid tree\n", __FUNCTION__);
    return (false);
  }

  if (!node) {
    printf("%s: Invalid node\n", __FUNCTION__);
    return (false);
  }
  
  if (!node->is_leaf) {
    printf("%s: Non-leaf node\n", __FUNCTION__);
    return (false);
  }

  return (node->u.leaf->num < (tree->order - 1));
}

/*
 * get the leaf in which the key should reside
 */
static bplus_tree_node_t *
find_leaf_for_key (bplus_tree_node_t *root,
                   int key)
{
  int index = 0;

  if (!root) {
    return (NULL);
  }

  if (root->is_leaf) {
    return (root);
  }

  index = get_child_index(root, key);
  return (find_leaf_for_key(root->u.index->child[index], key));
}

/*
 * utility function to insert a new (k, v) pair in
 * non-full leaf node
 *
 * Simply, do a sorted add of the new pair in the pairs array
 */
static void
insert_into_non_full_leaf (bplus_tree_node_t *node,
                           int key,
                           float value)
{
  int i = 0;
  int num = 0;
  pair_t *pairs = NULL;

  if (!node) {
    printf("%s: Error: invalid node\n", __FUNCTION__);
    return;
  }
  
  if (!node->is_leaf) {
    printf("%s: Error: non leaf node\n", __FUNCTION__);
    return;
  }
 
  pairs = node->u.leaf->pairs;
  num = node->u.leaf->num;
  for (i = num - 1; (i >= 0 && (pairs[i].key > key)); i--) {

    pairs[i + 1].key = pairs[i].key;
    pairs[i + 1].data = pairs[i].data;
  }

  pairs[i + 1].key = key;
  pairs[i + 1].data = value;
  node->u.leaf->num++;

  return;
}

/*
 * insert a new key in non-full parent node(index node)
 *
 * do a sorted insert of the new key in keys array of the index node
 * rearragnge child pointers accordingly
 */
static void
insert_key_into_non_full_parent (bplus_tree_node_t *parent,
                                 bplus_tree_node_t *leaf,
                                 bplus_tree_node_t *new_leaf,
                                 int key)
{
  int i = 0;
  int j = 0;
  int num = 0;
  int *keys = NULL;
  void **child = NULL;

  if (!parent || !leaf || !new_leaf) {
    printf("%s: Error: Invalid args\n", __FUNCTION__);
    return;
  }

  if (parent->is_leaf) {
    printf("%s: Error: non index node\n", __FUNCTION__);
    return;
  }

  keys = parent->u.index->keys;
  child = parent->u.index->child;
  num = parent->u.index->num;

  /*
   * move all the keys which are greater than this key by 1
   */
  for (i = num - 1; (i >= 0 && (keys[i] > key)); i--) {

    keys[i + 1] = keys[i];
  }

  /*
   * we are going to add the new key at i;
   * which means we want to shift all child pointers starting at i + 1 by 1
   */
  for (j = num; j > i + 1; j--) {
    child[j + 1] = child[j];
  }

  keys[i + 1] = key;
  child[j + 1] = new_leaf;
  parent->u.index->num++;

  return;
}

/*
 * insert a new key in a full parent
 * split the node in two and adjust parent
 */
static bplus_tree_node_t *
insert_key_into_full_parent (bplus_tree_node_t *root,
                             bplus_tree_node_t *parent,
                             bplus_tree_node_t *leaf,
                             bplus_tree_node_t *new_leaf,
                             int key)
{
  int i = 0, j = 0, k = 0, l = 0;
  int num_tmp_keys = 0;
  int num = 0;
  int promote_key = 0; //key which will be promoted
  int *keys = NULL; //keys in parent
  void **child = NULL; //child pointers in parent
  int *new_keys = NULL; //keys in new_node
  void **new_child = NULL; //child pointers in new_node
  int tmp_keys[tree->order]; //hold all keys in parent + new key
  void *tmp_child[tree->order + 1]; //hold all children in parent + new_leaf
  bplus_tree_node_t *new_node = NULL;

  if (!root || !parent || !leaf || !new_leaf) {
    printf("%s: Error: invalid arguments\n", __FUNCTION__);
    return (root);
  }

  if (parent->is_leaf) {
    printf("%s: Error: parent cannot be leaf here\n", __FUNCTION__);
    return (root);
  }

  new_node = bplus_tree_create_node(false);
  if (!new_node) {
    printf("%s: Error: cannot create new node\n", __FUNCTION__);
    return (root);
  }
  new_node->parent = parent->parent;

  memset(tmp_keys, 0, sizeof(tmp_keys));
  memset(tmp_child, 0, sizeof(tmp_child));

  keys = parent->u.index->keys;
  child = parent->u.index->child;
  num = parent->u.index->num;
  num_tmp_keys = num + 1;

  /*
   * copy all the keys from parent  which are less than key in tmp_keys
   */
  i = 0;
  j = 0;
  while (keys[j] < key && j < num) {
    tmp_keys[i++] = keys[j++];
  }
  
  /*
   * at this point new_leaf will be inserted at loc i in tmp_child
   */
  k = 0;
  l = 0;
  while (k <= i) {
    tmp_child[k++] = child[l++];
  }

  /*
   * we now have the right location for key
   */
  tmp_keys[i++] = key;

  /*
   * insert the new leaf into child pointers
   */
  tmp_child[k++] = new_leaf;

  /*
   * copy rest of the keys
   */
  while (j < num) {
    tmp_keys[i++] = keys[j++];
  }

  /*
   * copy rest of the pointers
   */
  while (l <= num) {
    tmp_child[k++] = child[l++];
  }

  /*
   * now tmp_keys and tmp_child should have
   * keys and child in correct order (if things worked :-))
   */

  /*
   * copy half the content in parent node
   */
  parent->u.index->num = 0;
  for (i = 0; i < (tree->order/2); i++) {
    
    keys[i] = tmp_keys[i];
    parent->u.index->num++;
  }

  for (j = 0; j <= i; j++) {
    
    child[j] = tmp_child[j];
  }

  /*
   * promote the middle key as this is an index node
   */
  promote_key = tmp_keys[i];

  /*
   * copy rest of the half in new node
   */
  new_keys = new_node->u.index->keys;
  new_child = new_node->u.index->child;
  for (i = (tree->order/2 + 1), j = 0; i < num_tmp_keys; i++, j++) {
    
    new_keys[j] = tmp_keys[i];
    new_node->u.index->num++;
  }
  
  for (i = (tree->order/2 + 1), j = 0; i < num_tmp_keys + 1; i++, j++) {
    
    bplus_tree_node_t *tmp = NULL;

    new_child[j] = tmp_child[i];

    /* we have a new parent for this child pointers */
    tmp = new_child[j];
    tmp->parent = new_node;
  }

  return adjust_parent(root, parent, new_node, promote_key);
}

/*
 * adjust the parent after we have promoted a key from a lower level
 */
bplus_tree_node_t *
adjust_parent (bplus_tree_node_t *root,
             bplus_tree_node_t *leaf,
             bplus_tree_node_t *new_leaf,
             int parent_key)
{
  bplus_tree_node_t *new_node = NULL;
  bplus_tree_node_t *parent = NULL;

  parent = leaf->parent;

  /*
   * If we don't have a parent
   */
  if (!parent) {
    
    /*
     * create a new index node;
     * put the parent_key in the newly created node;
     * adjust child pointers for this node;
     * return the new node (which is the new root of the tree)
     */
    new_node = bplus_tree_create_node(false);
    if (!new_node) {
      printf("%s: Error: could not create new index node\n", __FUNCTION__);
      return root;
    }
    
    /* new nodes parent is going to be NULL */

    /* add the parent key which was promoted to index node */
    index_node_add_key(new_node->u.index, parent_key);

    /* adjust the child pointers accordingly */
    new_node->u.index->child[0] = leaf;
    new_node->u.index->child[1] = new_leaf;
    
    /* we have a new parent for leaf and new leaf */
    leaf->parent = new_node;
    new_leaf->parent = new_node;
    return (new_node);
  }

  /*
   * if we have a parent which is not full
   */
  if (index_has_room(tree, parent)) {
    insert_key_into_non_full_parent(parent, leaf, new_leaf, parent_key);
    return (root);
  }

  /*
   * Parent doesnt have room;
   * spilt the node accordingly
   */
  return (insert_key_into_full_parent(root, parent, leaf, new_leaf, parent_key));
}

/*
 * Insert a new <key, value> in full leaf
 * split the node in two leafs and promote a key to parent node
 */
bplus_tree_node_t *
insert_into_full_leaf (bplus_tree_node_t *root,
                       bplus_tree_node_t *node,
                       int key,
                       float value)
{
  int i = 0 ;
  int j = 0;
  int num = 0;
  int promote_key = 0;
  bplus_tree_node_t *new_leaf = NULL;
  pair_t tmp_pairs[tree->order];
  pair_t *pairs = NULL;
  pair_t *new_pairs = NULL;

  if (!node) {
    printf("%s: Error: invalid node\n", __FUNCTION__);
    return(NULL);
  }

  if (!node->is_leaf) {
    printf("%s: Error: non leaf node\n", __FUNCTION__);
    return(NULL);
  }

  memset(tmp_pairs, 0, tree->order * sizeof(pair_t));

  /*
   * create a new leaf;
   * parent for this leaf is the same as the parent for leaf
   */
  new_leaf = bplus_tree_create_node(true);
  if (!new_leaf) {
    printf("%s: Error: Could not create new leaf\n", __FUNCTION__);
    return (root);
  }
  new_leaf->parent = node->parent;

  /*
   * change the doubly link list
   */
  new_leaf->u.leaf->next = node->u.leaf->next;
  node->u.leaf->next = new_leaf;
  new_leaf->u.leaf->prev = node;
  if (new_leaf->u.leaf->next) {
    bplus_tree_node_t *tmp = NULL;

    tmp = new_leaf->u.leaf->next;
    tmp->u.leaf->prev = new_leaf;
  }

  /*
   * transfer half of the pairs to new leaf
   */
  num = node->u.leaf->num;
  pairs = node->u.leaf->pairs;
  i = 0; //index in tmp_pairs
  j = 0; //index in original leaf

  /* copy everything less than key to tmp pairs */
  while (pairs[j].key < key && j < num) {
    tmp_pairs[i++] = pairs[j++];
  }

  /* copy the new <key, value> in right location */
  tmp_pairs[i].key = key;
  tmp_pairs[i].data = value;
  i++;

  /* copy the rest of pairs to tmp_pairs */
  while (j < num) {
    tmp_pairs[i++] = pairs[j++];
  }

  /* copy half the pairs in original leaf */
  node->u.leaf->num = 0;
  for (i = 0; i < (tree->order)/2; i++) {
    pairs[i] = tmp_pairs[i];
    node->u.leaf->num++;
  }

  /* copy the rest of the half in new leaf */
  new_pairs = new_leaf->u.leaf->pairs;
  for (i = (tree->order/2), j = 0; i < tree->order; i++, j++) {
    new_pairs[j] = tmp_pairs[i];
    new_leaf->u.leaf->num++;
  }

  promote_key = new_pairs[0].key;

  return adjust_parent(root, node, new_leaf, promote_key);
}

/*
 * Create the root node for tree
 * (1st insertion)
 */
bplus_tree_node_t *
bplus_tree_create_root (int key,
                        float value)
{
  bplus_tree_node_t *root = NULL;

  /*
   * create a new leaf node
   * and mark it as root
   */
  root = bplus_tree_create_node(true);
  if (!root) {
    printf("%s: Error: could not create root node\n", __FUNCTION__);
    return (NULL);
  }

  leaf_node_add_pair(root->u.leaf, key, value);
  return (root);
}

/*
 * utility function to add a key to the B+ tree
 */
void
bplus_tree_insert_internal (bplus_tree_node_t **root,
                            int key,
                            float value)
{
  int index = 0;
  float data = 0;
  bplus_tree_node_t *leaf = NULL;

  /*
   * empty tree:
   * create a root and insert in the root
   */
  if (!*root) {

    *root = bplus_tree_create_root(key, value);
    return;
  }

  /*
   * tree is non empty
   */
  leaf = find_leaf_for_key (*root, key);
  if (!leaf) {
    
    /*
     * Cannot happen ?
     */
    printf("%s: Error: could not find leaf for key %d\n", __FUNCTION__, key);
    return;
  }

  /*
   * if the key is already present modify the contents
   */
  index = bplus_tree_search_in_leaf(leaf, key, &data);
  if (index != -1) {
    
    leaf->u.leaf->pairs[index].data = value;
    return;
  }

  /*
   * leaf has room;
   * insert into right location in leaf and return
   * there is no change to the tree in this case
   */
  if (leaf_has_room(tree, leaf)) {
   
    insert_into_non_full_leaf(leaf, key, value);
    return;
  }

  /*
   * leaf is full;
   * create a new leaf and adjust accordingly
   */
  *root = insert_into_full_leaf(*root, leaf, key, value);
  return;

}

/*
 * insert a (key, value) pair in b+ tree
 */
void
bplus_tree_insert (bplus_tree_t *tree,
                   int key,
                   float value)
{
  if (!tree)
    return;

  bplus_tree_insert_internal(&tree->root, key, value);

  return;
}

/***********************************
 *   Delete                        *
 ***********************************/
static int ceil2 (int x, int y)
{
  if (x == 0)
    return -1;

  return (1 + ((x - 1) / y)); // if x != 0);
}

static bool
node_is_valid (bplus_tree_node_t *root,
               bplus_tree_node_t *node)
{
  //TODO_URGENT:
  int num = 0;

  if (node->is_leaf)
    num = node->u.leaf->num;
  else
    num = node->u.index->num;

  if (node == root) {
    if (num == 0)
      return (false);
    return (true);
  }

  /*
   * each index/leaf node must have atleast ceil(m/2) children
   * i.e: ceil(m/2) - 1 keys for valid b+ tree
   */
  if (num < (ceil2(tree->order, 2) - 1))
    return (false);

  return (true);
}

static int
search_key_index_in_keys (int *keys,
                          int key,
                          int start, int end)
{
    int mid = 0;

    if (start > end)
        return (-1);
    
    mid = (start + end)/2;
    if (keys[mid] == key) {
        return (mid);
    }

    if (key < keys[mid]) {
        return (search_key_index_in_keys(keys, key, start, mid - 1));
    } else {
        return (search_key_index_in_keys(keys, key, mid + 1, end));
    }

    return (-1);
}

static int
search_key_index_in_pairs (pair_t *pairs,
                      int key,
                      int start, int end)
{
    int mid = 0;

    if (start > end)
        return (-1);
    
    mid = (start + end)/2;
    if (pairs[mid].key == key) {
        return (mid);
    }

    if (key < pairs[mid].key) {
        return (search_key_index_in_pairs(pairs, key, start, mid - 1));
    } else {
        return (search_key_index_in_pairs(pairs, key, mid + 1, end));
    }

    return (-1);
}

static int
search_child_index_in_children (void **child,
                                void *key,
                                int start, int end)
{
    int i = 0;

    while (child[i] != key)
      i++;

    return (i);
}

static bool
node_has_keys (bplus_tree_node_t *node)
{
  if (node->is_leaf)
    return (node->u.leaf->num);
  else
    return (node->u.index->num);
}

bplus_tree_node_t *
get_first_child (bplus_tree_node_t *node)
{
  if (node->is_leaf)
    return (NULL);

  return (node->u.index->child[0]);
}

static void
adjust_leaf_node (bplus_tree_node_t *node,
                  int key)
{
  int i = 0;
  int index = 0;
  int num = 0;
  pair_t *pairs = NULL;

  if (!node->is_leaf) {
    printf("%s: Error: non leaf node", __FUNCTION__);
    return;
  }

  /*
   * delete the key, value pair
   * and adjust the rest of the pairs accordingly
   */

  /* adjust the keys */
  num = node->u.leaf->num;
  pairs = node->u.leaf->pairs;
  index = search_key_index_in_pairs(pairs, key, 0, num - 1);
  if (index == -1)
    return;

  for (i = index; i < num; i++) {
    pairs[i] = pairs[i + 1];
  }

  /* node has one less key */
  node->u.leaf->num--;

  return;
}

static void
adjust_index_node (bplus_tree_node_t *node,
                   bplus_tree_node_t *child,
                   int key)
{
  int i = 0;
  int index = 0;
  int num = 0;
  int *keys = NULL;
  void **children = NULL;

  if (node->is_leaf) {
    printf("%s: Error: leaf node\n");
    return;
  }

  /* adjust the keys */
  num = node->u.index->num;
  keys = node->u.index->keys;
  index = search_key_index_in_keys(keys, key, 0, num - 1);
  if (index == -1)
    return;

  for (i = index; i < num; i++) {
    keys[i] = keys[i + 1];
  }
  
  /* adjust the child pointers */
  num = node->u.index->num + 1; //one more child then keys
  children = node->u.index->child;
  index = search_child_index_in_children(children, child, 0, num -1);
  if (index == -1) {
    return;
  }
  
  for (i = index; i < num; i++) {
    children[i] = children[i + 1];
  }

  /* node has one less key */
  node->u.index->num--;
  return;
}

static void
adjust_node (bplus_tree_node_t *node,
             bplus_tree_node_t *child,
             int key)
{
  if (node->is_leaf)
    adjust_leaf_node(node, key);
  else
    adjust_index_node(node, child, key);
}

static bplus_tree_node_t *
modify_root (bplus_tree_node_t *root)
{
  bplus_tree_node_t *new_root = NULL;

  if (!root)
    return (NULL);

  /*
   * if there are keys;
   * return root as is
   */
  if (node_has_keys(root))
    return (root);

  /*
   * root is empty
   * promote the child if needed
   * else tree is now empty
   */
  if (!root->is_leaf) {
    new_root = get_first_child(root);
    new_root->parent = NULL;
    return (new_root);
  }

  bplus_tree_delete_node(root);
  return(NULL);
}

static bool
is_sibling_generous (bplus_tree_node_t *node)
{
  int num = 0;

  if (!node)
    return (false);

  if (node->is_leaf)
    num = node->u.leaf->num;
  else
    num = node->u.index->num;

  if (num > (tree->order/2))
    return (true);

  return (false);
}

static void
get_sibling_and_parent_key (bplus_tree_node_t *node,
                            bplus_tree_node_t **sibling,
                            int *sibling_index,
                            int *parent_key,
                            int *parent_key_index)
{
  int i = 0;
  int num = 0;
  int *keys = NULL;
  void **children = NULL;
  bplus_tree_node_t *parent = NULL;

  /*
   * try to get the left neighbor for this node
   */
  parent = node->parent;
  if (!parent) {
    printf("%s: Error: no parent\n", __FUNCTION__);
    return;
  }

  if (parent->is_leaf) {
    printf("%s: Error: We shouldn't have hit a leaf node here\n", __FUNCTION__);
    return;
  }

  num = parent->u.index->num;
  keys = parent->u.index->keys;
  children = parent->u.index->child;

  /* node has num + 1 child pointers  */
  i = 0;
  while (children[i] != node) {
    i++;
  }

  *sibling_index = i - 1;

  /*
   * node is the leftmost leaf
   */
  if (*sibling_index == -1) {
    *parent_key_index = 0;
    *sibling = children[1];
  } else {
    *parent_key_index = *sibling_index;
    *sibling = children[*sibling_index];
  }

  *parent_key = keys[*parent_key_index];

  return;
}

static void
borrow_and_adjust_leaf_nodes (bplus_tree_node_t *root,
                              bplus_tree_node_t *node,
                              bplus_tree_node_t *sibling,
                              int sibling_index,
                              int parent_key_index,
                              int parent_key)
{
  int i = 0;
  int num1 = 0;
  int num2 = 0;
  pair_t *pairs1 = NULL;
  pair_t *pairs2 = NULL;

  num1 = node->u.leaf->num;
  num2 = sibling->u.leaf->num;
  pairs1 = node->u.leaf->pairs;
  pairs2 = sibling->u.leaf->pairs;
  if (sibling_index == -1) {
  
    /*
     * leaf is the left most leaf
     * we need to borrow leftmost pair from sibling
     * and place it in the rightmost pair of node.
     * Also change the parent key at the parent_key_index
     *
     * example (for order = 5):
     *              parent(4)
     *    node: (2, 3) sibling: (7, 8, 9, 10)
     *
     * will become
     *              parent(8)
     *    node: (2, 3, 7) sibling: (8, 9, 10)
     */
    pairs1[num1] = pairs2[0];
    for (i = 0; i < num2; i++) {
      pairs2[i] = pairs2[i + 1];
    }
    node->parent->u.index->keys[parent_key_index] = pairs2[0].key;
  } else {
    
    /*
     * leaf has a neighbor on the left
     * we need to borrow rightmost pair from sibling
     * and place it in the leftmost pair of node
     * Also change the parent key at the parent_key_index
     *
     * example(for order = 5):
     *              parent(4)
     *    sibling: (2, 3, 4, 5) node: (7, 8)
     *
     * will become
     *              parent(5)
     *    sibling: (2, 3, 4) node: (5, 7, 8)
     */
    for (i = num1; i > 0; i--) {
      pairs1[i] = pairs1[i - 1];
    }
    pairs1[0] = pairs2[num2 - 1];
    node->parent->u.index->keys[parent_key_index] = pairs1[0].key;
  }

  node->u.leaf->num++;
  sibling->u.leaf->num--;
  return;
}

static void
borrow_and_adjust_index_nodes (bplus_tree_node_t *root,
                               bplus_tree_node_t *node,
                               bplus_tree_node_t *sibling,
                               int sibling_index,
                               int parent_key_index,
                               int parent_key)
{
  int i = 0;
  int *pkeys = NULL; //keys in parent node
  int *nkeys = NULL; //keys in node
  int *skeys = NULL; //keys in sibling
  void **nchild = NULL; //child pointers in node
  void **schild = NULL; //child pointers in sibling
  int nnum = 0; //number of keys in node
  int snum = 0; //number of keys in sibling
  bplus_tree_node_t *parent = NULL;

  nnum = node->u.index->num;
  snum = sibling->u.index->num;
  nkeys = node->u.index->keys;
  skeys = sibling->u.index->keys;
  nchild = node->u.index->child;
  schild = sibling->u.index->child;
  parent = node->parent;
  pkeys = parent->u.index->keys;

  if (sibling_index == -1) {

    /*
     * the node is the leftmost node in the tree.
     * borrow a key from sibling through the neighbor
     */
    bplus_tree_node_t *borrowed_child = NULL;

    /*
     * borrow the parent key in the node
     * also adjust child pointers accordingly
     */
    nkeys[nnum] = parent_key;
    nchild[nnum + 1] = schild[0];

    /*
     * the child which was just borrowed now has a new parent
     */
    borrowed_child = (bplus_tree_node_t *)nchild[nnum + 1];
    borrowed_child->parent = node;

    /*
     * parent key at the parent_key_index will change to the leftmost key
     * in sibling
     */
    pkeys[parent_key_index] = skeys[0];
    
    /*
     * shift all the keys in sibling
     */
    for (i = 0; i < snum - 1; i++ )
      skeys[i] = skeys[i + 1];

    /*
     * shift all the children in sibling
     */
    for (i = 0; i < snum; i++)
      schild[i] = schild[i + 1];
  } else {
    
    /*
     * node has a sibling to its left.
     * borrow the rightmost key fromm sibling
     * and place it in leftmost key of node
     * do the borrow through the parent
     */
    bplus_tree_node_t *borrowed_child = NULL;

    /*
     * shift all keys in node
     */
    for (i = nnum; i > 0; i--)
      nkeys[i] = nkeys[i - 1];

    /*
     * shift all the child pointers
     */
    for (i = nnum + 1; i > 0; i--)
      nchild[i] = nchild[i - 1];

    /*
     * borrow a key from sibling in node
     * through parent
     */
    nkeys[0] = parent_key;
    pkeys[parent_key_index] = skeys[snum - 1];

    /*
     * borrow the child pointer
     */
    nchild[0] = schild[snum];

    /*
     * parent of the borrowed child has now changed
     */
    borrowed_child = (bplus_tree_node_t *)nchild[0];
    borrowed_child->parent = node;
  }
  
  /*
   * After adjusting:
   * node has one mmore key;
   * sibling has one less key
   */
  node->u.index->num++;
  sibling->u.index->num--;
}


static void
borrow_from_sibling (bplus_tree_node_t *root,
                     bplus_tree_node_t *node,
                     bplus_tree_node_t *sibling,
                     int sibling_index,
                     int parent_key_index,
                     int parent_key)
{
  if (node->is_leaf)
    return borrow_and_adjust_leaf_nodes(root, node, sibling, sibling_index,
                                        parent_key_index, parent_key);

  return (borrow_and_adjust_index_nodes(root, node, sibling, sibling_index,
                                        parent_key_index, parent_key));
}

static bplus_tree_node_t *
merge_parent_and_sibling_for_leaf_nodes (bplus_tree_node_t *root,
                                         bplus_tree_node_t *node,
                                         bplus_tree_node_t *sibling,
                                         int sibling_index,
                                         int parent_key_index,
                                         int parent_key)
{
  int i = 0, j = 0;
  int nnum = 0;
  int snum = 0;
  pair_t *npairs = NULL;
  pair_t *spairs = NULL;
  bplus_tree_node_t *parent = NULL;

  nnum = node->u.leaf->num;
  snum = sibling->u.leaf->num;
  npairs = node->u.leaf->pairs;
  spairs = sibling->u.leaf->pairs;
  parent = node->parent;

  /*
   * we will add all the pairs from node to neigh
   * case1:
   *  node is the leftmost node
   *
   *  example:
   *          parent (<a> 16 <b>)
   *
   *    node (2)        sibling(17, 18)
   *
   *    node and neighbors have been swapped
   *
   *              ||
   *              \/
   *
   *              parent(<a> 16 <b>)
   *
   *    node (17, 18)   sibling (2)
   *
   *  final state:
   *
   *                  parent(<a> 16(to be deleted) <b>)
   *
   *                  sibling(2, 17, 18)
   *
   *  case2:
   *    node has a left neighbor
   *
   *  example:
   *            parent (16)
   *
   *    node (2, 3, 5)        sibling(17, 18)
   *
   *    final-state:
   *
   *                parent (16 (to be deleted))
   *
   *                sibling(2, 3, 5, 17)
   */
  i = snum;
  j = 0;
  while (j < nnum) {
    spairs[i++] = npairs[j++];
    sibling->u.leaf->num++;
  }

  root = delete_key_from_node(root, parent, node, parent_key); 

  /* node can now be deleted */
  bplus_tree_delete_node(node);
  
  return (root);
}

static bplus_tree_node_t *
merge_parent_and_sibling_for_index_nodes (bplus_tree_node_t *root,
                                          bplus_tree_node_t *node,
                                          bplus_tree_node_t *sibling,
                                          int sibling_index,
                                          int parent_key_index,
                                          int parent_key)
{
  int i = 0, j = 0;
  int nnum = 0 ;
  int snum = 0;
  int *nkeys = NULL;
  int *skeys = NULL;
  void **nchild = NULL;
  void **schild = NULL;
  bplus_tree_node_t *parent = NULL;
  bplus_tree_node_t *merged_child = NULL;

  nnum = node->u.index->num;
  snum = sibling->u.index->num;
  nkeys = node->u.index->keys;
  skeys = sibling->u.index->keys;
  nchild = node->u.index->child;
  schild = sibling->u.index->child;
  parent = node->parent;

  skeys[snum] = parent_key;
  snum = ++sibling->u.index->num;
  

  i = snum;
  j = 0;
  while (j < nnum) {
    skeys[i] = nkeys[j];
    schild[i] = nchild[j];
    sibling->u.index->num++;
    node->u.index->num--;

    /* change parent for the merged child */
    merged_child = (bplus_tree_node_t *)nchild[j];
    merged_child->parent = sibling;
    i++;
    j++;
  }

  /* onde more child */
  schild[i] = nchild[j];
  merged_child = (bplus_tree_node_t *)nchild[j];
  merged_child->parent = sibling;

  root = delete_key_from_node(root, parent, node, parent_key); 

  /* node can now be deleted */
  bplus_tree_delete_node(node);
 
  return (root);
}

static bplus_tree_node_t *
merge_parent_and_sibling (bplus_tree_node_t *root,
                          bplus_tree_node_t *node,
                          bplus_tree_node_t *sibling,
                          int sibling_index,
                          int parent_key_index,
                          int parent_key)
{
  bplus_tree_node_t *tmp = NULL;

  if (sibling_index == -1) {
    tmp = node;
    node = sibling;
    sibling = tmp;
  }
  
  if (node->is_leaf)
    return (merge_parent_and_sibling_for_leaf_nodes(root, node,
                                                    sibling, sibling_index,
                                                    parent_key_index,
                                                    parent_key));

  return (merge_parent_and_sibling_for_index_nodes(root, node,
                                                   sibling, sibling_index,
                                                   parent_key_index,
                                                   parent_key));
}

static bplus_tree_node_t *
delete_key_from_node (bplus_tree_node_t *root,
                      bplus_tree_node_t *node,
                      bplus_tree_node_t *child,
                      int key)
{
  int parent_key = 0;
  int parent_key_index = 0;
  int sibling_index = 0;
  bplus_tree_node_t *sibling = NULL;

  if (!root)
    return (root);

  if (!node)
    return (root);

  if (!node->is_leaf && !child)
    return (root);

  /*
   * delete the key and the child pointer fromm the node
   * This will simply delete the key/pointer without adjusting the tree
   */
  adjust_node(node, child, key);

  /*
   * if deletion of key didn't violate b+ tree property
   * nothing more to be done as tree remains unchanged
   */
  if (node_is_valid(root, node))
    return (root);
  
  /*
   * special case of root
   */
  if (node == root)
    return (modify_root(root));

  /*
   * b+ tree properties are violated;
   * adjust the tree accordingly
   */

  get_sibling_and_parent_key(node, &sibling, &sibling_index,
                             &parent_key, &parent_key_index);

  /*
   * see if we can borrow from sibling without
   * violating the B+ tree properties.
   * There is no change in the tree in this case;
   * return root as is
   */
  if (is_sibling_generous(sibling)) {

    borrow_from_sibling(root, node, sibling, sibling_index,
                        parent_key_index, parent_key);
    return (root);
  }

  return (merge_parent_and_sibling(root, node, sibling, sibling_index,
                                   parent_key_index, parent_key));
  
}

bplus_tree_node_t *
bplus_tree_delete_key_util (bplus_tree_node_t *root,
                            int key)
{
  bplus_tree_node_t *leaf;

  if (!root)
    return;

  leaf = find_leaf_for_key(root, key);
  if (!leaf)
    return;

  return (delete_key_from_node(root, leaf, NULL, key));
}

static void
bplus_tree_delete_key (bplus_tree_t *tree,
                   int key)
{
  if (!tree)
    return;

  tree->root = bplus_tree_delete_key_util(tree->root, key);

  return;
}

/********************
 * Driver function  *
 ********************/
int
main ()
{
  int key = 0;
  float data = 0;

  tree = bplus_tree_create(3);
  if (!tree)
    return -1;

  print_tree(tree);

  bplus_tree_insert(tree, 1, 1);
  print_tree(tree);

  
  bplus_tree_delete(&tree);
}
