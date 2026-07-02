/*
 * virtual_filesystem.c - in-memory virtual filesystem implementation.
 *
 * See virtual_filesystem.h. Implements mkdir/splitPath/find_child plus
 * ls/cd/pwd/rm/rmdir/touch/cat/cp/write/grep/tree over the in-memory tree.
 */
#define _POSIX_C_SOURCE 200809L

#include "virtual_filesystem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static VfsNode *root; /* "/" */
static VfsNode *cwd;  /* current working directory within the sandbox */

/* ---- lifecycle --------------------------------------------------------- */

void vfs_init(void) {
    root = (VfsNode *)calloc(1, sizeof(VfsNode));
    if (root == NULL) {
        fprintf(stderr, "vfs: out of memory initializing root\n");
        exit(EXIT_FAILURE);
    }
    strcpy(root->name, "/");
    root->fileType = 'D';
    root->parentPtr = NULL;
    root->siblingPtr = NULL;
    root->childPtr = NULL;
    root->content = NULL;
    cwd = root;
}

static void free_subtree(VfsNode *node) {
    if (node == NULL) {
        return;
    }
    free_subtree(node->childPtr);
    free_subtree(node->siblingPtr);
    free(node->content);
    free(node);
}

void vfs_free(void) {
    free_subtree(root);
    root = NULL;
    cwd = NULL;
}

/* ---- helpers ----------------------------------------------------------- */

VfsNode *vfs_find_child(VfsNode *parent, const char *name) {
    if (parent == NULL || name == NULL) {
        return NULL;
    }
    for (VfsNode *p = parent->childPtr; p != NULL; p = p->siblingPtr) {
        if (strcmp(p->name, name) == 0) {
            return p;
        }
    }
    return NULL;
}

/* Append a fully-formed node as the last child of parent. */
static void append_child(VfsNode *parent, VfsNode *child) {
    child->parentPtr = parent;
    if (parent->childPtr == NULL) {
        parent->childPtr = child;
        return;
    }
    VfsNode *sib = parent->childPtr;
    while (sib->siblingPtr != NULL) {
        sib = sib->siblingPtr;
    }
    sib->siblingPtr = child;
}

/* Detach a node from its parent's child/sibling chain (does not free it). */
static void detach_child(VfsNode *node) {
    VfsNode *parent = node->parentPtr;
    if (parent == NULL) {
        return;
    }
    if (parent->childPtr == node) {
        parent->childPtr = node->siblingPtr;
        return;
    }
    for (VfsNode *p = parent->childPtr; p != NULL; p = p->siblingPtr) {
        if (p->siblingPtr == node) {
            p->siblingPtr = node->siblingPtr;
            return;
        }
    }
}

/*
 * Split a path into (dirName, baseName) and return the parent node that baseName
 * should live under. Absolute paths start at root, relative paths at cwd.
 * Prints "ERROR: directory X does not exist" and returns NULL on a bad prefix.
 */
VfsNode *vfs_splitpath(const char *pathName, char *baseName, char *dirName) {
    const char *lastSlash = strrchr(pathName, '/');

    if (lastSlash == NULL) {
        strcpy(baseName, pathName);
        strcpy(dirName, "");
    } else if (lastSlash == pathName) {
        strcpy(baseName, lastSlash + 1);
        strcpy(dirName, "/");
    } else {
        strcpy(baseName, lastSlash + 1);
        size_t dirLen = (size_t)(lastSlash - pathName);
        strncpy(dirName, pathName, dirLen);
        dirName[dirLen] = '\0';
    }

    VfsNode *cur;
    if (dirName[0] == '\0') {
        cur = cwd;
    } else if (pathName[0] == '/') {
        cur = root;
    } else {
        cur = cwd;
    }

    size_t n = strlen(dirName) + 1;
    char *tmp = malloc(n);
    if (tmp == NULL) {
        return NULL;
    }
    memcpy(tmp, dirName, n);

    char *tok = strtok(tmp, "/");
    while (tok != NULL) {
        if (tok[0] == '\0') {
            tok = strtok(NULL, "/");
            continue;
        }
        VfsNode *next = vfs_find_child(cur, tok);
        if (next == NULL || next->fileType != 'D') {
            printf("ERROR: directory %s does not exist\n", tok);
            free(tmp);
            return NULL;
        }
        cur = next;
        tok = strtok(NULL, "/");
    }
    free(tmp);
    return cur;
}

/* Resolve a whole path (dir or file) to a node, or NULL if it doesn't exist. */
static VfsNode *resolve(const char *path) {
    if (path == NULL || path[0] == '\0' || strcmp(path, ".") == 0) {
        return cwd;
    }
    if (strcmp(path, "/") == 0) {
        return root;
    }
    char baseName[VFS_NAME_MAX];
    char dirName[VFS_NAME_MAX];
    VfsNode *parent = vfs_splitpath(path, baseName, dirName);
    if (parent == NULL) {
        return NULL;
    }
    return vfs_find_child(parent, baseName);
}

/* ---- command handlers -------------------------------------------------- */

void vfs_mkdir(const char *path) {
    if (path == NULL || strcmp(path, "/") == 0 || path[0] == '\0') {
        printf("MKDIR ERROR: no path provided\n");
        return;
    }
    char baseName[VFS_NAME_MAX];
    char dirName[VFS_NAME_MAX];
    VfsNode *parent = vfs_splitpath(path, baseName, dirName);
    if (parent == NULL) {
        return;
    }
    VfsNode *dup = vfs_find_child(parent, baseName);
    if (dup != NULL) {
        printf("MKDIR ERROR: directory %s already exists\n", path);
        return;
    }
    VfsNode *node = (VfsNode *)calloc(1, sizeof(VfsNode));
    if (node == NULL) {
        return;
    }
    strncpy(node->name, baseName, VFS_NAME_MAX - 1);
    node->fileType = 'D';
    append_child(parent, node);
    printf("MKDIR SUCCESS: node %s successfully created\n", path);
}

void vfs_touch(const char *path) {
    if (path == NULL || path[0] == '\0' || strcmp(path, "/") == 0) {
        printf("TOUCH ERROR: no path provided\n");
        return;
    }
    char baseName[VFS_NAME_MAX];
    char dirName[VFS_NAME_MAX];
    VfsNode *parent = vfs_splitpath(path, baseName, dirName);
    if (parent == NULL) {
        return;
    }
    if (vfs_find_child(parent, baseName) != NULL) {
        /* touch on an existing node is a no-op, like Unix (no mtime here). */
        return;
    }
    VfsNode *node = (VfsNode *)calloc(1, sizeof(VfsNode));
    if (node == NULL) {
        return;
    }
    strncpy(node->name, baseName, VFS_NAME_MAX - 1);
    node->fileType = 'F';
    append_child(parent, node);
    printf("TOUCH SUCCESS: file %s successfully created\n", path);
}

void vfs_write(const char *path, const char *text) {
    if (path == NULL || path[0] == '\0') {
        printf("WRITE ERROR: no path provided\n");
        return;
    }
    VfsNode *node = resolve(path);
    if (node == NULL) {
        /* auto-create the file if the parent exists */
        char baseName[VFS_NAME_MAX];
        char dirName[VFS_NAME_MAX];
        VfsNode *parent = vfs_splitpath(path, baseName, dirName);
        if (parent == NULL) {
            return;
        }
        node = (VfsNode *)calloc(1, sizeof(VfsNode));
        if (node == NULL) {
            return;
        }
        strncpy(node->name, baseName, VFS_NAME_MAX - 1);
        node->fileType = 'F';
        append_child(parent, node);
    }
    if (node->fileType != 'F') {
        printf("WRITE ERROR: %s is not a file\n", path);
        return;
    }
    free(node->content);
    node->content = (text != NULL) ? strdup(text) : NULL;
    printf("WRITE SUCCESS: wrote %zu bytes to %s\n",
           text != NULL ? strlen(text) : 0, path);
}

void vfs_cat(const char *path) {
    VfsNode *node = resolve(path);
    if (node == NULL) {
        printf("CAT ERROR: %s does not exist\n", path ? path : "");
        return;
    }
    if (node->fileType != 'F') {
        printf("CAT ERROR: %s is not a file\n", path);
        return;
    }
    if (node->content != NULL) {
        printf("%s\n", node->content);
    }
}

void vfs_cp(const char *src, const char *dst) {
    if (src == NULL || dst == NULL) {
        printf("CP ERROR: usage: cp <src> <dst>\n");
        return;
    }
    VfsNode *srcNode = resolve(src);
    if (srcNode == NULL || srcNode->fileType != 'F') {
        printf("CP ERROR: source file %s does not exist\n", src);
        return;
    }
    char baseName[VFS_NAME_MAX];
    char dirName[VFS_NAME_MAX];
    VfsNode *parent = vfs_splitpath(dst, baseName, dirName);
    if (parent == NULL) {
        return;
    }
    VfsNode *existing = vfs_find_child(parent, baseName);
    if (existing != NULL && existing->fileType == 'F') {
        free(existing->content);
        existing->content = srcNode->content ? strdup(srcNode->content) : NULL;
    } else if (existing == NULL) {
        VfsNode *node = (VfsNode *)calloc(1, sizeof(VfsNode));
        if (node == NULL) {
            return;
        }
        strncpy(node->name, baseName, VFS_NAME_MAX - 1);
        node->fileType = 'F';
        node->content = srcNode->content ? strdup(srcNode->content) : NULL;
        append_child(parent, node);
    } else {
        printf("CP ERROR: %s is a directory\n", dst);
        return;
    }
    printf("CP SUCCESS: copied %s to %s\n", src, dst);
}

void vfs_grep(const char *pattern, const char *path) {
    if (pattern == NULL || path == NULL) {
        printf("GREP ERROR: usage: grep <pattern> <file>\n");
        return;
    }
    VfsNode *node = resolve(path);
    if (node == NULL || node->fileType != 'F') {
        printf("GREP ERROR: %s is not a file\n", path);
        return;
    }
    if (node->content == NULL) {
        return;
    }
    char *copy = strdup(node->content);
    if (copy == NULL) {
        return;
    }
    for (char *line = strtok(copy, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        if (strstr(line, pattern) != NULL) {
            printf("%s\n", line);
        }
    }
    free(copy);
}

void vfs_ls(const char *path) {
    VfsNode *dir = resolve(path);
    if (dir == NULL) {
        printf("LS ERROR: %s does not exist\n", path ? path : "");
        return;
    }
    if (dir->fileType == 'F') {
        printf("%s\n", dir->name);
        return;
    }
    for (VfsNode *p = dir->childPtr; p != NULL; p = p->siblingPtr) {
        printf("%s%s\n", p->name, p->fileType == 'D' ? "/" : "");
    }
}

void vfs_cd(const char *path) {
    if (path == NULL || path[0] == '\0' || strcmp(path, "/") == 0) {
        cwd = root;
        return;
    }
    if (strcmp(path, "..") == 0) {
        if (cwd->parentPtr != NULL) {
            cwd = cwd->parentPtr;
        }
        return;
    }
    if (strcmp(path, ".") == 0) {
        return;
    }
    VfsNode *target = resolve(path);
    if (target == NULL) {
        printf("CD ERROR: directory %s does not exist\n", path);
        return;
    }
    if (target->fileType != 'D') {
        printf("CD ERROR: %s is not a directory\n", path);
        return;
    }
    cwd = target;
}

void vfs_pwd(void) {
    if (cwd == root) {
        printf("/\n");
        return;
    }
    /* Walk parent pointers to root, collecting names, then print reversed. */
    VfsNode *chain[256];
    int depth = 0;
    for (VfsNode *p = cwd; p != NULL && p != root && depth < 256; p = p->parentPtr) {
        chain[depth++] = p;
    }
    for (int i = depth - 1; i >= 0; i--) {
        printf("/%s", chain[i]->name);
    }
    printf("\n");
}

void vfs_rmdir(const char *path) {
    VfsNode *node = resolve(path);
    if (node == NULL) {
        printf("RMDIR ERROR: directory %s does not exist\n", path ? path : "");
        return;
    }
    if (node == root) {
        printf("RMDIR ERROR: cannot remove root\n");
        return;
    }
    if (node->fileType != 'D') {
        printf("RMDIR ERROR: %s is not a directory\n", path);
        return;
    }
    if (node->childPtr != NULL) {
        printf("RMDIR ERROR: directory %s is not empty\n", path);
        return;
    }
    if (cwd == node) {
        cwd = node->parentPtr;
    }
    detach_child(node);
    free(node);
    printf("RMDIR SUCCESS: directory %s removed\n", path);
}

void vfs_rm(const char *path) {
    VfsNode *node = resolve(path);
    if (node == NULL) {
        printf("RM ERROR: file %s does not exist\n", path ? path : "");
        return;
    }
    if (node->fileType != 'F') {
        printf("RM ERROR: %s is not a file (use rmdir)\n", path);
        return;
    }
    detach_child(node);
    free(node->content);
    free(node);
    printf("RM SUCCESS: file %s removed\n", path);
}

/* ---- tree printing ----------------------------------------------------- */

static void tree_helper(VfsNode *node, int depth) {
    while (node != NULL) {
        for (int i = 0; i < depth; i++) {
            printf("    ");
        }
        printf("%s%s\n", node->name, node->fileType == 'D' ? "/" : "");
        if (node->childPtr != NULL) {
            tree_helper(node->childPtr, depth + 1);
        }
        node = node->siblingPtr;
    }
}

void vfs_tree(void) {
    printf("/\n");
    tree_helper(root->childPtr, 1);
}
