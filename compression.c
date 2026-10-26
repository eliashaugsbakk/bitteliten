#include "compression.h"
#include "file_reader_writer.h"
#include "codec.h"

#include <stdio.h>
#include <stdlib.h>

int compress_file(const char *input_filename, const char *output_filename) {
        Buffer *input = read_file(input_filename);
        if (!input)
                return 1;

        TokenList tokens = {0};
        if (lz_encode(input->data, input->size, &tokens) != 0) {
                free_buffer(input);
                return 1;
        }

        FILE *output = fopen(output_filename, "wb");
        if (!output) {
                tokens_free(&tokens);
                free_buffer(input);
                return 1;
        }

        int result = huff_write(output, &tokens, input->size);

        if (fclose(output) != 0)
                result = 1;

        tokens_free(&tokens);
        free_buffer(input);
        return result;
}

int decompress_file(const char *input_filename, const char *output_filename) {
        FILE *input = fopen(input_filename, "rb");
        if (!input)
                return 1;

        TokenList tokens = {0};
        uint64_t original_len = 0;
        int result = huff_read(input, &tokens, &original_len);

        if (fclose(input) != 0)
                result = 1;

        if (result != 0) {
                tokens_free(&tokens);
                return 1;
        }

        if (original_len > SIZE_MAX) {
                tokens_free(&tokens);
                return 1;
        }

        Buffer output = {.data = malloc((size_t)original_len),
                         .size = (size_t)original_len};

        if (original_len > 0 && output.data == NULL) {
                tokens_free(&tokens);
                return 1;
        }

        if (lz_decode(&tokens, output.data, output.size) != 0) {
                free(output.data);
                tokens_free(&tokens);
                return 1;
        }

        result = write_file(output_filename, &output);

        free(output.data);
        tokens_free(&tokens);
        return result;
}
