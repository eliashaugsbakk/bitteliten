/** Huffman in/decoding
 *
 * Takes in a list of "symbols" which may be bigger than 8bit.
 *
 */

#include "codec.h"

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


/* Write bits into BitWriter. Returns 0 on success, 1 on EOF/error. */
static int write_bits(BitWriter *bw, uint64_t bits, unsigned length) {
        for (unsigned i = 0; i < length; i++) {
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

/* Read bits into out_bits. Returns 0 on success, 1 on EOF/error. */
static int read_bits(BitReader *br, uint64_t *out_bits, unsigned length) {
        uint64_t result = 0;

        for (unsigned i = 0; i < length; i++) {
                if (br->buffer_pos == 8) {
                        int ch = fgetc(br->file_stream);
                        if (ch == EOF) {
                                return 1;
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

/* Write the final partially filled byte, if there is one. */
static int flush(BitWriter *bw) {
        if (bw->buffer_pos != 0) {
                if (fputc(bw->buffer, bw->file_stream) == EOF)
                        return 1;
        }
        bw->buffer = 0;
        bw->buffer_pos = 0;
        return 0;
}

int huff_write_symbols(FILE *file, const SymbolList *symbols, int alphabet_size) {
        (void)file;
        (void)symbols;
        (void)alphabet_size;
        return 1;
}

int huff_read_symbols(FILE *file, SymbolList *output, int alphabet_size) {
        (void)file;
        (void)output;
        (void)alphabet_size;
        return 1;
}
