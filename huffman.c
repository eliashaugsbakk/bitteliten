#include "codec.h"
#include <stdint.h>

typedef struct {
        uint8_t buffer;
        uint8_t buffer_pos;
        FILE *file_stream;
} BitWriter;

typedef struct {
        uint8_t buffer;
        uint8_t buffer_pos;
        FILE *file_stream;
} BitReader;

// Reads bits into out_bits. Returns 0 on success, -1 on EOF/Error.
int read_bits(BitReader *br, uint64_t *out_bits, uint8_t length) {
        uint64_t result = 0;

        for (int i = 0; i < length; i++) {
                if (br->buffer_pos == 8) {
                        int ch = fgetc(br->file_stream);
                        if (ch == EOF) {
                                return -1;
                        }
                        br->buffer = (uint8_t)ch;
                        br->buffer_pos = 0;
                }

                uint64_t current_bit = (br->buffer >> br->buffer_pos) & 1;

                result |= (current_bit << i);

                br->buffer_pos++;
        }

        *out_bits = result;
        return 0;
}

int write_bits(BitWriter *bw, uint64_t bits, uint8_t length) {
        for (int i = 0; i < length; i++) {
                if (bw->buffer_pos == 8) {
                        if (fputc(bw->buffer, bw->file_stream) == EOF)
                                return 1;

                        bw->buffer = 0;
                        bw->buffer_pos = 0;
                }

                uint8_t current_bit = (bits >> i) & 1;
                bw->buffer = bw->buffer | (current_bit << bw->buffer_pos);

                bw->buffer_pos++;
        }
        return 0;
}

int flush(BitWriter *bw) {
        if (bw->buffer_pos != 0) {
                if (fputc(bw->buffer, bw->file_stream) == EOF)
                        return 1;
        }
        bw->buffer = 0;
        bw->buffer_pos = 0;
        return 0;
}

/* Huffman compression from TokenList to compressed bits */
int huff_write(FILE *output_file, const TokenList *tokens, uint64_t original_len) {
        int result = 1;
        return result;
}

/* Huffman decompression from compressed bits to TokenList */
int huff_read(FILE *output_file, TokenList *output, uint64_t *original_len) {
        int result = 1;
        return result;
}

