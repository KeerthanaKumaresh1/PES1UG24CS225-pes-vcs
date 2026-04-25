#ifndef OBJECT_H
#define OBJECT_H
#include <stddef.h>
#include <stdint.h>

#define HASH_SIZE     32
#define HASH_HEX_SIZE 64

#define PES_DIR     ".pes"
#define OBJECTS_DIR ".pes/objects"
#define REFS_DIR    ".pes/refs/heads"
#define INDEX_FILE  ".pes/index"
#define HEAD_FILE   ".pes/HEAD"

typedef struct {
    unsigned char hash[HASH_SIZE];
} ObjectID;

typedef enum {
    OBJ_BLOB   = 1,
    OBJ_TREE   = 2,
    OBJ_COMMIT = 3
} ObjectType;

void hash_to_hex(const ObjectID *id, char *hex_out);
int  hex_to_hash(const char *hex, ObjectID *id_out);
void compute_hash(const void *data, size_t len, ObjectID *id_out);
void object_path(const ObjectID *id, char *path_out, size_t path_size);
int  object_exists(const ObjectID *id);
int  object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);
int  object_read(const ObjectID *id, ObjectType *type_out, void **data_out, size_t *len_out);

#endif
