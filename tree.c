// tree.c — Tree object serialization and construction
//
// PROVIDED functions: get_file_mode, tree_parse, tree_serialize
// TODO functions: tree_from_index
//
// Binary tree format (per entry, concatenated with no separators):
//   "<mode-as-ascii-octal> <name>\0<32-byte-binary-hash>"

#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

// ─── Mode Constants ─────────────────────────────────────────────────────────
#define MODE_FILE 0100644
#define MODE_EXEC 0100755
#define MODE_DIR  0040000

// ─── PROVIDED ───────────────────────────────────────────────────────────────

// Determine the object mode for a filesystem path.
uint32_t get_file_mode(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return 0;
    if (S_ISDIR(st.st_mode))    return MODE_DIR;
    if (st.st_mode & S_IXUSR)   return MODE_EXEC;
    return MODE_FILE;
}

// Parse binary tree data into a Tree struct safely.
// Returns 0 on success, -1 on parse error.
int tree_parse(const void *data, size_t len, Tree *tree_out) {
    tree_out->count = 0;
    const uint8_t *ptr = (const uint8_t *)data;
    const uint8_t *end = ptr + len;

    while (ptr < end && tree_out->count < MAX_TREE_ENTRIES) {
        TreeEntry *entry = &tree_out->entries[tree_out->count];

        const uint8_t *space = memchr(ptr, ' ', end - ptr);
        if (!space) return -1;

        char mode_str[16] = {0};
        size_t mode_len = space - ptr;
        if (mode_len >= sizeof(mode_str)) return -1;
        memcpy(mode_str, ptr, mode_len);
        entry->mode = strtol(mode_str, NULL, 8);
        ptr = space + 1;

        const uint8_t *null_byte = memchr(ptr, '\0', end - ptr);
        if (!null_byte) return -1;

        size_t name_len = null_byte - ptr;
        if (name_len >= sizeof(entry->name)) return -1;
        memcpy(entry->name, ptr, name_len);
        entry->name[name_len] = '\0';
        ptr = null_byte + 1;

        if (ptr + HASH_SIZE > end) return -1;
        memcpy(entry->hash.hash, ptr, HASH_SIZE);
        ptr += HASH_SIZE;

        tree_out->count++;
    }
    return 0;
}

static int compare_tree_entries(const void *a, const void *b) {
    return strcmp(((const TreeEntry *)a)->name, ((const TreeEntry *)b)->name);
}

// Serialize a Tree struct into binary format for storage.
// Caller must free(*data_out).
int tree_serialize(const Tree *tree, void **data_out, size_t *len_out) {
    size_t max_size = tree->count * 296;
    uint8_t *buffer = malloc(max_size);
    if (!buffer) return -1;

    Tree sorted_tree = *tree;
    qsort(sorted_tree.entries, sorted_tree.count, sizeof(TreeEntry), compare_tree_entries);

    size_t offset = 0;
    for (int i = 0; i < sorted_tree.count; i++) {
        const TreeEntry *entry = &sorted_tree.entries[i];
        int written = sprintf((char *)buffer + offset, "%o %s", entry->mode, entry->name);
        offset += written + 1; // +1 for null terminator from sprintf
        memcpy(buffer + offset, entry->hash.hash, HASH_SIZE);
        offset += HASH_SIZE;
    }

    *data_out = buffer;
    *len_out  = offset;
    return 0;
}

// ─── IMPLEMENTED ─────────────────────────────────────────────────────────────

// Forward declarations from other translation units
int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);
int index_load(Index *index);

// Recursive helper: given a slice of index entries (all sharing a common
// directory prefix at `depth` slashes), build and write a tree object,
// returning its hash in *id_out.
//
// `entries`  – pointer to first IndexEntry in this group
// `count`    – how many entries are in this group
// `prefix`   – the directory prefix these entries are under (e.g. "src/")
//              empty string "" means root
static int write_tree_level(IndexEntry *entries, int count,
                            const char *prefix, ObjectID *id_out) {
    Tree tree;
    tree.count = 0;

    int i = 0;
    while (i < count) {
        const char *path = entries[i].path;
        size_t prefix_len = strlen(prefix);

        // Path relative to current level (strip the common prefix)
        const char *rel = path + prefix_len;

        // Does this entry sit directly in the current directory,
        // or is it inside a subdirectory?
        char *slash = strchr(rel, '/');

        if (slash == NULL) {
            // ── Direct file entry ──────────────────────────────────────────
            TreeEntry *te = &tree.entries[tree.count];
            strncpy(te->name, rel, sizeof(te->name) - 1);
            te->name[sizeof(te->name) - 1] = '\0';
            te->mode = entries[i].mode;
            memcpy(te->hash.hash, entries[i].id.hash, HASH_SIZE);
            tree.count++;
            i++;
        } else {
            // ── Subdirectory group ─────────────────────────────────────────
            // Extract subdirectory name (e.g. "src" from "src/main.c")
            size_t dir_name_len = (size_t)(slash - rel);
            char dir_name[256];
            if (dir_name_len >= sizeof(dir_name)) return -1;
            memcpy(dir_name, rel, dir_name_len);
            dir_name[dir_name_len] = '\0';

            // Build the full prefix for the subdirectory
            // e.g. prefix="" + "src/" → "src/"
            char sub_prefix[512];
            snprintf(sub_prefix, sizeof(sub_prefix), "%s%s/", prefix, dir_name);

            // Collect all entries that belong to this subdirectory
            int j = i;
            while (j < count) {
                const char *p = entries[j].path + prefix_len;
                // Does this path start with "dir_name/"?
                if (strncmp(p, dir_name, dir_name_len) == 0 &&
                    p[dir_name_len] == '/') {
                    j++;
                } else {
                    break;
                }
            }
            // entries[i..j-1] all belong to sub_prefix
            int sub_count = j - i;

            // Recurse to build the subtree
            ObjectID sub_id;
            if (write_tree_level(&entries[i], sub_count,
                                 sub_prefix, &sub_id) != 0) {
                return -1;
            }

            // Add an entry for this directory in the current tree
            TreeEntry *te = &tree.entries[tree.count];
            strncpy(te->name, dir_name, sizeof(te->name) - 1);
            te->name[sizeof(te->name) - 1] = '\0';
            te->mode = MODE_DIR;
            te->hash = sub_id;
            tree.count++;

            i = j; // advance past all sub-entries
        }
    }

    // Serialize the tree and write it to the object store
    void *buf;
    size_t buf_len;
    if (tree_serialize(&tree, &buf, &buf_len) != 0) return -1;

    int rc = object_write(OBJ_TREE, buf, buf_len, id_out);
    free(buf);
    return rc;
}

// Build a tree hierarchy from the current index and write all tree
// objects to the object store.
//
// Returns 0 on success, -1 on error.
int tree_from_index(ObjectID *id_out) {
    Index index;
    index.count = 0;

    if (index_load(&index) != 0) return -1;
    if (index.count == 0) {
        // Empty tree: write a zero-entry tree object
        Tree empty;
        empty.count = 0;
        void *buf;
        size_t buf_len;
        if (tree_serialize(&empty, &buf, &buf_len) != 0) return -1;
        int rc = object_write(OBJ_TREE, buf, buf_len, id_out);
        free(buf);
        return rc;
    }

    return write_tree_level(index.entries, index.count, "", id_out);
}
