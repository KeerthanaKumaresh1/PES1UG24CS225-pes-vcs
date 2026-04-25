#include "tree.h"
#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

uint32_t get_file_mode(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return 0;
    if (S_ISDIR(st.st_mode))  return 0040000;
    if (st.st_mode & S_IXUSR) return 0100755;
    return 0100644;
}

int tree_parse(const void *data, size_t len, Tree *tree_out) {
    tree_out->count = 0;
    const unsigned char *ptr = (const unsigned char *)data;
    const unsigned char *end = ptr + len;
    while (ptr < end && tree_out->count < MAX_TREE_ENTRIES) {
        TreeEntry *e = &tree_out->entries[tree_out->count];
        const unsigned char *sp = memchr(ptr, ' ', end - ptr);
        if (!sp) return -1;
        char ms[16] = {0};
        size_t ml = sp - ptr;
        if (ml >= sizeof(ms)) return -1;
        memcpy(ms, ptr, ml);
        e->mode = (uint32_t)strtol(ms, NULL, 8);
        ptr = sp + 1;
        const unsigned char *nb = memchr(ptr, '\0', end - ptr);
        if (!nb) return -1;
        size_t nl = nb - ptr;
        if (nl >= sizeof(e->name)) return -1;
        memcpy(e->name, ptr, nl);
        e->name[nl] = '\0';
        ptr = nb + 1;
        if (ptr + HASH_SIZE > end) return -1;
        memcpy(e->hash.hash, ptr, HASH_SIZE);
        ptr += HASH_SIZE;
        tree_out->count++;
    }
    return 0;
}

static int cmp_tree(const void *a, const void *b) {
    return strcmp(((const TreeEntry*)a)->name, ((const TreeEntry*)b)->name);
}

int tree_serialize(const Tree *tree, void **data_out, size_t *len_out) {
    unsigned char *buf = malloc((size_t)tree->count * 300);
    if (!buf) return -1;
    Tree sorted = *tree;
    qsort(sorted.entries, sorted.count, sizeof(TreeEntry), cmp_tree);
    size_t off = 0;
    for (int i = 0; i < sorted.count; i++) {
        TreeEntry *e = &sorted.entries[i];
        int w = sprintf((char*)buf + off, "%o %s", e->mode, e->name);
        off += w + 1;
        memcpy(buf + off, e->hash.hash, HASH_SIZE);
        off += HASH_SIZE;
    }
    *data_out = buf;
    *len_out  = off;
    return 0;
}

static int write_tree_level(IndexEntry *entries, int count,
                             const char *prefix, ObjectID *id_out) {
    Tree tree; tree.count = 0;
    int i = 0;
    while (i < count) {
        const char *rel   = entries[i].path + strlen(prefix);
        char       *slash = strchr(rel, '/');
        if (!slash) {
            TreeEntry *e = &tree.entries[tree.count++];
            strncpy(e->name, rel, sizeof(e->name)-1);
            e->name[sizeof(e->name)-1] = '\0';
            e->mode = entries[i].mode;
            memcpy(e->hash.hash, entries[i].hash.hash, HASH_SIZE);
            i++;
        } else {
            int dlen = (int)(slash - rel);
            char sub[256]; strncpy(sub, rel, dlen); sub[dlen] = '\0';
            char np[512];
            snprintf(np, sizeof(np), "%s%s/", prefix, sub);
            size_t npl = strlen(np);
            int j = i;
            while (j < count && strncmp(entries[j].path, np, npl) == 0) j++;
            ObjectID sid;
            if (write_tree_level(entries+i, j-i, np, &sid) != 0) return -1;
            TreeEntry *e = &tree.entries[tree.count++];
            strncpy(e->name, sub, sizeof(e->name)-1);
            e->name[sizeof(e->name)-1] = '\0';
            e->mode = 0040000;
            memcpy(e->hash.hash, sid.hash, HASH_SIZE);
            i = j;
        }
    }
    void *data; size_t dlen;
    if (tree_serialize(&tree, &data, &dlen) != 0) return -1;
    int ret = object_write(OBJ_TREE, data, dlen, id_out);
    free(data);
    return ret;
}

int tree_from_index(ObjectID *id_out) {
    static Index index;
    memset(&index, 0, sizeof(index));
    if (index_load(&index) != 0) return -1;
    return write_tree_level(index.entries, index.count, "", id_out);
}
