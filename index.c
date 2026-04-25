#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>

IndexEntry* index_find(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++)
        if (strcmp(index->entries[i].path, path) == 0)
            return &index->entries[i];
    return NULL;
}

int index_remove(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) {
            int rem = index->count - i - 1;
            if (rem > 0)
                memmove(&index->entries[i], &index->entries[i+1],
                        rem * sizeof(IndexEntry));
            index->count--;
            return index_save(index);
        }
    }
    fprintf(stderr, "error: '%s' not in index\n", path);
    return -1;
}

int index_status(const Index *index) {
    printf("Staged changes:\n");
    int n = 0;
    for (int i = 0; i < index->count; i++) {
        printf("    staged: %s\n", index->entries[i].path);
        n++;
    }
    if (!n) printf("    (nothing to show)\n");
    printf("\n");

    printf("Unstaged changes:\n");
    n = 0;
    for (int i = 0; i < index->count; i++) {
        struct stat st;
        if (stat(index->entries[i].path, &st) != 0) {
            printf("    deleted: %s\n", index->entries[i].path); n++;
        } else if (st.st_mtime != (time_t)index->entries[i].mtime_sec ||
                   st.st_size  != (off_t)index->entries[i].size) {
            printf("    modified: %s\n", index->entries[i].path); n++;
        }
    }
    if (!n) printf("    (nothing to show)\n");
    printf("\n");

    printf("Untracked files:\n");
    n = 0;
    DIR *d = opendir(".");
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            if (strcmp(ent->d_name, "pes") == 0) continue;
            if (strstr(ent->d_name, ".o")) continue;
            int tracked = 0;
            for (int i = 0; i < index->count; i++)
                if (strcmp(index->entries[i].path, ent->d_name) == 0) { tracked=1; break; }
            if (!tracked) {
                struct stat st;
                stat(ent->d_name, &st);
                if (S_ISREG(st.st_mode)) { printf("    untracked: %s\n", ent->d_name); n++; }
            }
        }
        closedir(d);
    }
    if (!n) printf("    (nothing to show)\n");
    printf("\n");
    return 0;
}

int index_load(Index *index) {
    index->count = 0;
    FILE *f = fopen(INDEX_FILE, "r");
    if (!f) return 0;
    char hex[HASH_HEX_SIZE + 1];
    while (index->count < MAX_INDEX_ENTRIES) {
        IndexEntry *e = &index->entries[index->count];
        int n = fscanf(f, "%o %64s %lu %u %511s\n",
                       &e->mode, hex,
                       (unsigned long *)&e->mtime_sec,
                       &e->size, e->path);
        if (n != 5) break;
        if (hex_to_hash(hex, &e->hash) != 0) { fclose(f); return -1; }
        index->count++;
    }
    fclose(f);
    return 0;
}

static int cmp_idx(const void *a, const void *b) {
    return strcmp(((const IndexEntry*)a)->path, ((const IndexEntry*)b)->path);
}

int index_save(const Index *index) {
    Index sorted = *index;
    qsort(sorted.entries, sorted.count, sizeof(IndexEntry), cmp_idx);
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s.tmp", INDEX_FILE);
    FILE *f = fopen(tmp, "w");
    if (!f) return -1;
    char hex[HASH_HEX_SIZE + 1];
    for (int i = 0; i < sorted.count; i++) {
        IndexEntry *e = &sorted.entries[i];
        hash_to_hex(&e->hash, hex);
        fprintf(f, "%o %s %lu %u %s\n",
                e->mode, hex, (unsigned long)e->mtime_sec, e->size, e->path);
    }
    fflush(f); fsync(fileno(f)); fclose(f);
    return rename(tmp, INDEX_FILE);
}

int index_add(Index *index, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "error: cannot open '%s'\n", path); return -1; }
    fseek(f, 0, SEEK_END);
    size_t len = (size_t)ftell(f);
    rewind(f);
    void *buf = malloc(len + 1);
    if (!buf) { fclose(f); return -1; }
    if (fread(buf, 1, len, f) != len) { free(buf); fclose(f); return -1; }
    fclose(f);

    ObjectID oid;
    if (object_write(OBJ_BLOB, buf, len, &oid) != 0) { free(buf); return -1; }
    free(buf);

    struct stat st;
    if (lstat(path, &st) != 0) return -1;

    IndexEntry *e = index_find(index, path);
    if (!e) {
        if (index->count >= MAX_INDEX_ENTRIES) return -1;
        e = &index->entries[index->count++];
    }
    strncpy(e->path, path, sizeof(e->path) - 1);
    e->path[sizeof(e->path)-1] = '\0';
    memcpy(e->hash.hash, oid.hash, HASH_SIZE);
    e->mode      = (st.st_mode & S_IXUSR) ? 0100755 : 0100644;
    e->mtime_sec = (uint64_t)st.st_mtime;
    e->size      = (uint32_t)st.st_size;
    return index_save(index);
}

