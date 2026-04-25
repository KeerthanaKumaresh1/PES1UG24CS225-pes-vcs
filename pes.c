#include "pes.h"
#include "index.h"
#include "commit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ── init ── */
void cmd_init(void) {
    mkdir(PES_DIR,     0755);
    mkdir(OBJECTS_DIR, 0755);
    mkdir(".pes/refs", 0755);
    mkdir(REFS_DIR,    0755);

    FILE *f = fopen(HEAD_FILE, "w");
    if (f) { fprintf(f, "ref: refs/heads/main\n"); fclose(f); }

    printf("Initialized empty PES repository in .pes/\n");
}

/* ── add ── */
void cmd_add(int argc, char *argv[]) {
    if (argc < 3) { fprintf(stderr, "Usage: pes add <file>...\n"); return; }
    static Index index;
    memset(&index, 0, sizeof(index));
    index_load(&index);
    for (int i = 2; i < argc; i++) {
        if (index_add(&index, argv[i]) == 0)
            printf("Added: %s\n", argv[i]);
        else
            fprintf(stderr, "error: failed to add '%s'\n", argv[i]);
    }
}

/* ── status ── */
void cmd_status(void) {
    static Index index;
    memset(&index, 0, sizeof(index));
    index_load(&index);
    index_status(&index);
}

/* ── log callback ── */
static void log_callback(const ObjectID *id, const Commit *c, void *ctx) {
    (void)ctx;
    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(id, hex);
    printf("commit %s\n", hex);
    printf("Author: %s\n", c->author);
    printf("Date:   %llu\n", (unsigned long long)c->timestamp);
    printf("\n    %s\n\n", c->message);
}

/* ── log ── */
void cmd_log(void) {
    if (commit_walk(log_callback, NULL) != 0)
        fprintf(stderr, "No commits yet.\n");
}

/* ── commit ── */
void cmd_commit(int argc, char *argv[]) {
    const char *message = NULL;
    for (int i = 2; i < argc - 1; i++) {
        if (strcmp(argv[i], "-m") == 0) { message = argv[i+1]; break; }
    }
    if (!message) {
        fprintf(stderr, "error: commit requires a message (-m \"message\")\n");
        return;
    }
    ObjectID id;
    if (commit_create(message, &id) != 0) {
        fprintf(stderr, "error: commit failed\n");
        return;
    }
    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(&id, hex);
    printf("Committed: %.12s... %s\n", hex, message);
}

/* ── branch / checkout stubs ── */
static void cmd_branch(int argc, char *argv[]) {
    if (argc == 2)                                   branch_list();
    else if (argc == 3)                              branch_create(argv[2]);
    else if (argc == 4 && strcmp(argv[2],"-d") == 0) branch_delete(argv[3]);
}
static void cmd_checkout(int argc, char *argv[]) {
    if (argc < 3) { fprintf(stderr, "Usage: pes checkout <branch>\n"); return; }
    checkout(argv[2]);
}

/* ── main ── */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: pes <command> [args]\n");
        fprintf(stderr, "Commands: init, add, status, commit, log, branch, checkout\n");
        return 1;
    }
    const char *cmd = argv[1];
    if      (strcmp(cmd, "init")     == 0) cmd_init();
    else if (strcmp(cmd, "add")      == 0) cmd_add(argc, argv);
    else if (strcmp(cmd, "status")   == 0) cmd_status();
    else if (strcmp(cmd, "commit")   == 0) cmd_commit(argc, argv);
    else if (strcmp(cmd, "log")      == 0) cmd_log();
    else if (strcmp(cmd, "branch")   == 0) cmd_branch(argc, argv);
    else if (strcmp(cmd, "checkout") == 0) cmd_checkout(argc, argv);
    else { fprintf(stderr, "Unknown command: %s\n", cmd); return 1; }
    return 0;
}
