#include "file_reader_writer.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

void free_buffer(Buffer *buf) {
        if (buf) {
                free(buf->data);
                free(buf);
        }
}

Buffer *read_file(const char *file_path) {
        FILE *fp = fopen(file_path, "rb");
        if (!fp) {
                perror("Failed to open file");
                return NULL;
        }

        /* Go to the end of the file. */
        if (fseek(fp, 0L, SEEK_END) != 0) {
                perror("Failed to seek to end");
                fclose(fp);
                return NULL;
        }

        /* Get the size of the file. */
        long bufsize = ftell(fp);
        if (bufsize < 0) {
                perror("Failed to get file size");
                fclose(fp);
                return NULL;
        }

        /* Go back to the start of the file. */
        if (fseek(fp, 0L, SEEK_SET) != 0) {
                perror("Failed to seek to start");
                fclose(fp);
                return NULL;
        }

        /* Allocate our buffer to that size. */
        Buffer *buf = malloc(sizeof(Buffer));
        if (!buf) {
                perror("Failed to allocate Buffer struct");
                fclose(fp);
                return NULL;
        }

        /* Allocate at least 1 byte, since malloc(0) may return NULL. */
        buf->size = (size_t)bufsize;
        buf->data = malloc(buf->size > 0 ? buf->size : 1);
        if (!buf->data) {
                perror("Failed to allocate buffer data");
                free(buf);
                fclose(fp);
                return NULL;
        }

        size_t bytes_read = fread(buf->data, 1, buf->size, fp);
        if (bytes_read != buf->size) {
                perror("Failed to read complete file");
                free_buffer(buf);
                fclose(fp);
                return NULL;
        }

        fclose(fp);
        return buf;
}

int write_file(const char *file_path, const Buffer *buf) {
        if (!file_path || !buf)
                return 1;

        FILE *fp = fopen(file_path, "wb");
        if (!fp) {
                perror("Failed to open file for writing");
                return 1;
        }

        /* fwrite must not get a NULL pointer, even with size 0. */
        if (buf->size > 0 && fwrite(buf->data, 1, buf->size, fp) != buf->size) {
                perror("Failed to write complete data");
                fclose(fp);
                return 1;
        }

        /* Buffered data is written on close, so errors like a full disk show up here. */
        if (fclose(fp) != 0) {
                perror("Failed to close file");
                return 1;
        }

        return 0;
}
