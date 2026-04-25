#ifndef PES_H
#define PES_H
#include "object.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define DEFAULT_AUTHOR "PES User <pes@localhost>"

static inline const char* pes_author(void) {
    const char *env = getenv("PES_AUTHOR");
    return (env && env[0]) ? env : DEFAULT_AUTHOR;
}

void cmd_init(void);
void cmd_add(int argc, char *argv[]);
void cmd_status(void);
void cmd_commit(int argc, char *argv[]);
void cmd_log(void);

static inline void branch_list(void)            { printf("not implemented\n"); }
static inline int  branch_create(const char *n) { (void)n; return -1; }
static inline int  branch_delete(const char *n) { (void)n; return -1; }
static inline int  checkout(const char *t)      { (void)t; return -1; }

#endif
