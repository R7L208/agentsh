/*
 * virtual_filesystem.h - in-memory virtual filesystem for the agentsh sandbox.
 *
 * Agent-issued file commands operate on this tree, never on the host disk.
 * The tree model uses name/fileType/child/sibling/parent pointers and carries
 * file content so that touch/write/cat/cp/grep are meaningful inside the
 * sandbox.
 */
#ifndef AGENTSH_VIRTUAL_FILESYSTEM_H
#define AGENTSH_VIRTUAL_FILESYSTEM_H

#define VFS_NAME_MAX 64

typedef struct VfsNode {
    char name[VFS_NAME_MAX];
    char fileType;            /* 'D' for directory, 'F' for regular file */
    char *content;            /* file body (NULL for directories/empty files) */
    struct VfsNode *childPtr;
    struct VfsNode *siblingPtr;
    struct VfsNode *parentPtr;
} VfsNode;

/* Lifecycle: build the root ("/") tree and tear it down. */
void vfs_init(void);
void vfs_free(void);

/* Low-level helpers (shared with the command handlers). */
VfsNode *vfs_find_child(VfsNode *parent, const char *name);
VfsNode *vfs_splitpath(const char *pathName, char *baseName, char *dirName);

/* Command handlers. Each prints its own result/error to stdout. */
void vfs_mkdir(const char *path);
void vfs_rmdir(const char *path);
void vfs_ls(const char *path);
void vfs_cd(const char *path);
void vfs_pwd(void);
void vfs_touch(const char *path);
void vfs_rm(const char *path);
void vfs_cat(const char *path);
void vfs_cp(const char *src, const char *dst);
void vfs_write(const char *path, const char *text);
void vfs_grep(const char *pattern, const char *path);
void vfs_tree(void);

#endif /* AGENTSH_VIRTUAL_FILESYSTEM_H */
