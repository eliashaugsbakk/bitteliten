#ifndef FILE_READER_WRITER_H
#define FILE_READER_WRITER_H

#include <stddef.h>

typedef struct {
        unsigned char *data;
        size_t size;
} Buffer;

Buffer *read_file(const char *file_path);

int write_file(const char *file_path, const Buffer *buf);

void free_buffer(Buffer *buf);

#endif
