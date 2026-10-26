#ifndef CODEC_H
#define CODEC_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum {
        MIN_MATCH = 3,
        MAX_MATCH = 258
};

#define MAX_DISTANCE (1u << 15)

/* Tokens */

typedef enum {
        TOK_LITERAL = 0,
        TOK_MATCH = 1
} TokenKind;

typedef struct {
        uint8_t kind;
        uint8_t literal;   /* only for literals */
        uint16_t length;   /* only for matches */
        uint32_t distance; /* only for matches */
} Token;

typedef struct {
        Token *data;
        size_t count;
        size_t capacity;
} TokenList;

void tokens_free(TokenList *tokens);

/* Symbol mapping */

#define SYM_END 256

int sym_litlen_alphabet_size(void);
int sym_dist_alphabet_size(void);

int sym_length(unsigned length);
unsigned sym_length_base(int symbol);
unsigned sym_length_extra_bits(int symbol);

int sym_distance(unsigned distance);
unsigned sym_distance_base(int symbol);
unsigned sym_distance_extra_bits(int symbol);

/* LZ */

int lz_encode(const uint8_t *input, size_t input_size, TokenList *output);
int lz_decode(const TokenList *tokens, uint8_t *output, size_t output_size);

/* Huffman */

int huff_write(FILE *file, const TokenList *tokens, uint64_t original_len);
int huff_read(FILE *file, TokenList *output, uint64_t *original_len);

#endif
