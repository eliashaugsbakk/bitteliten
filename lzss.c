/** Handles the LZSS part of the compression. (like Lempel–Ziv) */

#include "codec.h"

int lz_encode(const uint8_t *input, size_t input_size, TokenList *output)
{
        (void)input;
        (void)input_size;
        (void)output;
        return 1;
}

int lz_decode(const TokenList *tokens, uint8_t *output, size_t output_size)
{
        (void)tokens;
        (void)output;
        (void)output_size;
        return 1;
}

void tokens_free(TokenList *tokens)
{
        (void)tokens;
}
