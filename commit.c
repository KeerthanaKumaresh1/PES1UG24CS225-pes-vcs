#include "commit.h"
#include "index.h"
#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

int commit_parse(const void *data, size_t len, Commit *c) {
    (void)len;
    const char *p = (const char *)data;
    char hex[HASH_HEX_SIZE + 1];
    if (sscanf(p, "tree %64s\n", hex) != 1) return -1;
    if (hex_to_hash(hex, &c->tree) != 0) return -1;
    p = strchr(p, '\n') + 1;
    if (strncmp(p, "parent ", 7) == 0) {
        if (sscanf(p, "parent %64s\n", hex) != 1) return -1;
        if (hex_to_hash(hex, &c->parent) != 0) return -1;
        c->has_parent = 1;
        p = strchr(p, '\n') + 1;
    } else {
        c->has_parent = 0;
    }
    char abuf[256];
    if (sscanf(p, "author %255[^\n]\n", abuf) != 1) return -1;
    char *ls = strrchr(abuf, ' ');
    if (!ls) return -1;
    c->timestamp = (uint64_t)strtoull(ls+1, NULL, 10);
    *ls = '\0';
    strncpy(c->author, abuf, sizeof(c->author)-1);
    p = strchr(p, '\n') + 1;
    p = strchr(p, '\n') + 1;
    p = strchr(p, '\n') + 1;
    strncpy(c->message, p, sizeof(c->message)-1);
    return 0;
}

int commit_serialize(const Commit *c, void **data_out, size_t *len_out) {
    char th[HASH_HEX_SIZE+1], ph[HASH_HEX_SIZE+1];
    hash_to_hex(&c->tree, th);
    char buf[8192]; int n = 0;
    n += snprintf(buf+n, sizeof(buf)-n, "tree %s\n", th);
    if (c->has_parent) {
        hash_to_hex(&c->parent, ph);
        n += snprintf(buf+n, sizeof(buf)-n, "parent %s\n", ph);
    }
    n += snprintf(buf+n, sizeof(buf)-n,
        "author %s %" PRIu64 "\n"
        "committer %s %" PRIu64 "\n"
        "\n%s",
        c->author, c->timestamp,
        c->author, c->timestamp,
        c->message);
    *data_out = malloc(n+1);
    if (!*data_out) return -1;
    memcpy(*data_out, buf, n+1);
    *len_out = (size_t)n;
    return 0;
}

int commit_walk(commit_walk_fn callback, void *ctx) {
    ObjectID id;
    if (head_read(&id) != 0) return -1;
    while (1) {
        ObjectType type; void *raw; size_t rlen;
        if (object_read(&id, &type, &raw, &rlen) != 0) return -1;
        Commit c; int rc = commit_parse(raw, rlen, &c); free(raw);
        if (rc != 0) return -1;
        callback(&id, &c, ctx);
        if (!c.has_parent) break;
        id = c.parent;
    }
    return 0;
}

int head_read(ObjectID *id_out) {
    FILE *f = fopen(HEAD_FILE, "r");
    if (!f) return -1;
    char line[512];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return -1; }
    fclose(f);
    line[strcspn(line, "\r\n")] = '\0';
    char ref_path[512];
    if (strncmp(line, "ref: ", 5) == 0) {
        snprintf(ref_path, sizeof(ref_path), "%s/%s", PES_DIR, line+5);
        f = fopen(ref_path, "r");
        if (!f) return -1;
        if (!fgets(line, sizeof(line), f)) { fclose(f); return -1; }
        fclose(f);
        line[strcspn(line, "\r\n")] = '\0';
    }
    return hex_to_hash(line, id_out);
}

int head_update(const ObjectID *new_commit) {
    FILE *f = fopen(HEAD_FILE, "r");
    if (!f) return -1;
    char line[512];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return -1; }
    fclose(f);
    line[strcspn(line, "\r\n")] = '\0';
    char target[512];
    if (strncmp(line, "ref: ", 5) == 0)
        snprintf(target, sizeof(target), "%s/%s", PES_DIR, line+5);
    else
        snprintf(target, sizeof(target), "%s", HEAD_FILE);
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s.tmp", target);
    f = fopen(tmp, "w");
    if (!f) return -1;
    char hex[HASH_HEX_SIZE+1];
    hash_to_hex(new_commit, hex);
    fprintf(f, "%s\n", hex);
    fflush(f); fsync(fileno(f)); fclose(f);
    return rename(tmp, target);
}

int commit_create(const char *message, ObjectID *commit_id_out) {
    ObjectID tree_id;
    if (tree_from_index(&tree_id) != 0) return -1;
    Commit c; memset(&c, 0, sizeof(c));
    memcpy(c.tree.hash, tree_id.hash, HASH_SIZE);
    c.timestamp = (uint64_t)time(NULL);
    strncpy(c.author, pes_author(), sizeof(c.author)-1);
    strncpy(c.message, message, sizeof(c.message)-1);
    ObjectID parent_id;
    if (head_read(&parent_id) == 0) {
        c.has_parent = 1;
        memcpy(c.parent.hash, parent_id.hash, HASH_SIZE);
    } else {
        c.has_parent = 0;
    }
    void *data; size_t len;
    if (commit_serialize(&c, &data, &len) != 0) return -1;
    int ret = object_write(OBJ_COMMIT, data, len, commit_id_out);
    free(data);
    if (ret != 0) return -1;
    return head_update(commit_id_out);
}

