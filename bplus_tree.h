#ifndef BPLUS_TREE_H_
#define BPLUS_TREE_H_

/*****************************
 * Auxillary data structures *
 *****************************/

typedef struct qnode_t_ {

  void        *data;
  struct qnode_t_    *next;
} qnode_t;

typedef struct queue_t_ {

  int is_empty;
  int size;
  qnode_t *head;
  qnode_t *tail;
} queue_t;

/*******************************
 * B+ tree related definitions *
 *******************************/

typedef struct pair_t_ {
    int key;        /* key */
    double data;    /* data */
} pair_t;

typedef struct index_node_t_ {

    int num;        /* number of keys in this node */
    int *keys;      /* array of keys in this node */
    void **child;   /* array of child pointers */
} index_node_t;

typedef struct leaf_node_t_ {

    struct bplus_tree_node_t_ *prev;    /* prev node */
    struct bplus_tree_node_t_ *next;    /* next node */
    int num;                      /* num of records in this leaf */
    pair_t       *pairs;          /* iarray of <key, value> pairs */
} leaf_node_t;

typedef struct bplus_tree_node_t_ {

    bool is_leaf;   /* set to true if this is a leaf node */
    struct bplus_tree_node_t_ *parent;

    /*
     * A node in b-plus tree can be:
     * 1. leaf node
     * 2. index node
     * It is represented as an union below
     */
    union {
        index_node_t *index;
        leaf_node_t *leaf;
    } u;
} bplus_tree_node_t;

typedef struct bplus_tree_t_ {
    
    int order;                  /* set to m in an m-way tree */
    int num_leafs;              /* total number of leaf nodes currently present in the tree*/
    int num_index;              /* total number of index nodes currently in the tree */
    bplus_tree_node_t *root;    /* root of the tree */
} bplus_tree_t;

/*
 * helper function to check if the tree is empty
 */
static inline bool
is_tree_empty (bplus_tree_t *tree)
{
    if (!tree)
        return(true);

    if (tree->num_leafs || tree->num_index)
        return (false);

    return(true);
}

#endif /* BPLUS_TREE_H_ */
