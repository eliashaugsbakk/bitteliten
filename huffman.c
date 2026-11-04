/** Handles the Huffman part of the compression. (like Huffman) */

#include "codec.h"

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
