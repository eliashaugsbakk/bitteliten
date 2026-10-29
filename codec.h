#ifndef CODEC_H
#define CODEC_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Symbols */

/* Output of zero-run coding. MTF value v (1..255) becomes symbol v + 1. */
enum {
        BWT_RUNA = 0,
        BWT_RUNB = 1,
        BWT_EOB = 257,
        BWT_ALPHABET_SIZE = 258
};

typedef struct {
        uint16_t *data;
        size_t count;
        size_t capacity;
} SymbolList;

int symbols_push(SymbolList *symbols, uint16_t symbol);
void symbols_free(SymbolList *symbols);

/* BWT */

enum {
        BWT_BLOCK_SIZE = 900000
};

/* All functions handle one block of at most BWT_BLOCK_SIZE bytes. */
int bwt_forward(const uint8_t *input, size_t size, uint8_t *output, uint32_t *primary_index);
int bwt_inverse(const uint8_t *input, size_t size, uint32_t primary_index, uint8_t *output);

void mtf_encode(const uint8_t *input, size_t size, uint8_t *output);
void mtf_decode(const uint8_t *input, size_t size, uint8_t *output);

/* Appends the symbols for one block to output, ending with BWT_EOB. */
int zero_run_encode(const uint8_t *input, size_t size, SymbolList *output);
int zero_run_decode(const SymbolList *symbols, uint8_t *output, size_t output_size);

/* Huffman */

/* Writes one block of symbols, including its own table. */
int huff_write_symbols(FILE *file, const SymbolList *symbols, int alphabet_size);
/* Reads one block written by huff_write_symbols. */
int huff_read_symbols(FILE *file, SymbolList *output, int alphabet_size);

#endif
