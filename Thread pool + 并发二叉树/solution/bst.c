/*
 * bst.c Binary Search Tree
 *
 * TODO: Add fine-grained locking to make this thread-safe.
 *
 * The current implementations are correct sequential code but NOT thread-safe.
 * You must use the tree-level lock (tree->lock) and per-node locks via the
 * NODE_LOCK(node) / NODE_UNLOCK(node) macros to make these operations safe
 * for concurrent access.
 *
 * You may add static helper functions as needed.
 */

#include "bst.h"

#include <errno.h>
#include <stdlib.h>


//Node allocation / deallocation

static bst_node_t *bst_node_create(int key, int value) {
    bst_node_t *node = (bst_node_t *)malloc(sizeof(*node));
    if (node == NULL) {
        return NULL;
    }

    node->key = key;
    node->value = value;
    node->lock_count = 0;
    node->left = NULL;
    node->right = NULL;

    if (pthread_mutex_init(&node->lock, NULL) != 0) {
        free(node);
        return NULL;
    }

    return node;
}

//Free a node that is being physically removed from the tree.
static void bst_node_free(bst_node_t *node) {
    pthread_mutex_destroy(&node->lock);
    free(node);
}

//Tree init/destroy

bst_t *bst_init(void) {
    bst_t *tree = (bst_t *)malloc(sizeof(*tree));
    if (tree == NULL) {
        return NULL;
    }

    tree->root = NULL;
    if (pthread_mutex_init(&tree->lock, NULL) != 0) {
        free(tree);
        return NULL;
    }

    return tree;
}

//Called after all threads have joined - no locking needed.
static void bst_destroy_node(bst_node_t *node) {
    if (node == NULL) {
        return;
    }

    bst_destroy_node(node->left);
    bst_destroy_node(node->right);
    pthread_mutex_destroy(&node->lock);
    free(node);
}

void bst_destroy(bst_t *tree) {
    if (tree == NULL) {
        return;
    }

    bst_destroy_node(tree->root);
    pthread_mutex_destroy(&tree->lock);
    free(tree);
}

/*
 * Insert a key-value pair. Returns 0 on success, -1 on duplicate key
 * or allocation failure.
 */
int bst_insert(bst_t *tree, int key, int value) {
   if (tree == NULL) {
        errno = EINVAL;
        return -1;
    }

    // lock the tree to protect the root pointer
    pthread_mutex_lock(&tree->lock);

    if (tree->root == NULL) {
        bst_node_t *new_node = bst_node_create(key, value);
        if (new_node == NULL) {
            pthread_mutex_unlock(&tree->lock);
            return -1;
        }
        tree->root = new_node;
        pthread_mutex_unlock(&tree->lock);
        return 0;
    }

    // transition from tree lock to the root node lock
    bst_node_t *cur = tree->root;
    NODE_LOCK(cur);
    pthread_mutex_unlock(&tree->lock);

    while (1) {
        if (key == cur->key) {
            NODE_UNLOCK(cur);
            return -1;
        }

        if (key < cur->key) {
            if (cur->left == NULL) {
                bst_node_t *new_node = bst_node_create(key, value);
                if (new_node == NULL) {
                    NODE_UNLOCK(cur);
                    return -1;
                }
                cur->left = new_node;
                NODE_UNLOCK(cur);
                return 0;
            }

            // lock child, then unlock current
            bst_node_t *next = cur->left;
            NODE_LOCK(next);
            NODE_UNLOCK(cur);
            cur = next;
        } else {
            if (cur->right == NULL) {
                bst_node_t *new_node = bst_node_create(key, value);
                if (new_node == NULL) {
                    NODE_UNLOCK(cur);
                    return -1;
                }
                cur->right = new_node;
                NODE_UNLOCK(cur);
                return 0;
            }

            // lock child, then unlock current
            bst_node_t *next = cur->right;
            NODE_LOCK(next);
            NODE_UNLOCK(cur);
            cur = next;
        }
    }
}

//Look up a key. Returns 0 and writes *value on success, -1 if not found.
int bst_lookup(bst_t *tree, int key, int *value) {
    if (tree == NULL || value == NULL) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&tree->lock);
    if (tree->root == NULL) {
        pthread_mutex_unlock(&tree->lock);
        return -1;
    }

    bst_node_t *cur = tree->root;
    NODE_LOCK(cur);
    pthread_mutex_unlock(&tree->lock);

    while (1) {
        if (key == cur->key) {
            *value = cur->value;
            NODE_UNLOCK(cur);
            return 0;
        }

        bst_node_t *next = (key < cur->key) ? cur->left : cur->right;
        if (next == NULL) {
            NODE_UNLOCK(cur);
            return -1;
        }

        // Lock next before releasing current
        NODE_LOCK(next);
        NODE_UNLOCK(cur);
        cur = next;
    }
}

/*
 * Helper: handle the two-children case.
 *
 * Called with cur being the node whose key/value will be replaced.
 * Finds the in-order successor (leftmost node in right subtree), copies
 * its key/value into cur, then removes the successor.
 */
static void delete_two_children(bst_node_t *cur) {
    bst_node_t *succ_parent = cur;    
    bst_node_t *succ = cur->right;
    NODE_LOCK(succ);

    while (succ->left != NULL) {
        bst_node_t *next_succ = succ->left;
        NODE_LOCK(next_succ);
        
        if (succ_parent != cur) {
            NODE_UNLOCK(succ_parent);
        }
        succ_parent = succ;
        succ = next_succ;
    }

    cur->key = succ->key;
    cur->value = succ->value;

    if (succ_parent == cur) {
        succ_parent->right = succ->right;
    } else {
        succ_parent->left = succ->right;
        NODE_UNLOCK(succ_parent);
    }

    NODE_UNLOCK(succ);
    bst_node_free(succ);
}

//Delete a key from the BST. Returns 0 on success, -1 if key not found.
int bst_delete(bst_t *tree, int key) {
    if (tree == NULL) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&tree->lock);
    if (tree->root == NULL) {
        pthread_mutex_unlock(&tree->lock);
        return -1;
    }

    bst_node_t *cur = tree->root;
    NODE_LOCK(cur);

    // deleting the root
    if (cur->key == key) {
        if (cur->left == NULL || cur->right == NULL) {
            tree->root = (cur->left != NULL) ? cur->left : cur->right;
            pthread_mutex_unlock(&tree->lock);
            NODE_UNLOCK(cur);
            bst_node_free(cur);
            return 0;
        }
        pthread_mutex_unlock(&tree->lock);
        delete_two_children(cur);
        NODE_UNLOCK(cur);
        return 0;
    }

    pthread_mutex_unlock(&tree->lock);
    bst_node_t *parent = cur; // parent locked
    bst_node_t *next;
    int is_left;

    if (key < cur->key) {
        next = cur->left;
        is_left = 1;
    } else {
        next = cur->right;
        is_left = 0;
    }

    if (next == NULL) {
        NODE_UNLOCK(parent);
        return -1;
    }
    NODE_LOCK(next);
    cur = next; // parent and cur locked

    while (cur->key != key) {
        if (key < cur->key) {
            next = cur->left;
            is_left = 1;
        } else {
            next = cur->right;
            is_left = 0;
        }

        if (next == NULL) {
            NODE_UNLOCK(parent);
            NODE_UNLOCK(cur);
            return -1;
        }

        NODE_UNLOCK(parent);
        parent = cur;
        NODE_LOCK(next);
        cur = next;
    }

    // parent and cur locked
    if (cur->left == NULL || cur->right == NULL) {
        bst_node_t *child = (cur->left != NULL) ? cur->left : cur->right;
        if (is_left) parent->left = child;
        else         parent->right = child;
        
        NODE_UNLOCK(parent);
        NODE_UNLOCK(cur);
        bst_node_free(cur);
    } else {
        NODE_UNLOCK(parent);
        delete_two_children(cur);
        NODE_UNLOCK(cur);
    }

    return 0;
}
