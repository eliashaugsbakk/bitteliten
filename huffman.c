#include "codec.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct Node {
        uint32_t symbol;
        uint64_t frequency;
        struct Node *left;
        struct Node *right;
} Node;

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

typedef struct {
        uint32_t bits;
        unsigned length;
} HuffmanCode;

// Write bits into BitWriter. Returns 0 on success, 1 on EOF/Error.
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

void count_frequencies(const TokenList *tokens, uint32_t *literal_match_length_freq, uint32_t *distance_freq) {
        for (size_t i = 0; i < tokens->count; i++) {
                Token token = tokens->data[i];
                if (token.kind == TOK_LITERAL) {
                        literal_match_length_freq[token.literal]++;
                } else if (token.kind == TOK_MATCH) {
                        int length_symbol = sym_length(token.length);
                        int distance_symbol = sym_distance(token.distance);

                        literal_match_length_freq[length_symbol]++;
                        distance_freq[distance_symbol]++;
                }
        }
        literal_match_length_freq[SYM_END]++;
}

Node *create_node(int symbol, uint64_t frequency, Node *left, Node *right) {
        Node *node = malloc(sizeof(*node));
        if (!node) {
                return NULL;
        }

        node->symbol = symbol;
        node->frequency = frequency;
        node->left = left;
        node->right = right;
        return node;
}

void free_tree(Node *node) {
        if (!node)
                return;
        free_tree(node->left);
        free_tree(node->right);
        free(node);
}

void free_nodes(Node **nodes, size_t alphabet_size) {
        if (!nodes)
                return;
        size_t max_nodes = alphabet_size * 2;
        for (size_t i = 0; i < max_nodes; i++) {
                if (nodes[i] != NULL) {
                        free_tree(nodes[i]);
                        nodes[i] = NULL;
                }
        }
}

Node *build_huffman_tree(uint32_t *frequencies, size_t alphabet_size) {
        // max possible active nodes = leaf nodes + internal nodes
        size_t max_nodes = alphabet_size * 2;
        Node **nodes = calloc(max_nodes, sizeof(Node *));
        if (!nodes)
                goto cleanup;

        size_t active_count = 0;

        // initialize symbols with non-zero frequencies
        for (size_t symbol = 0; symbol < alphabet_size; symbol++) {
                if (frequencies[symbol] > 0) {
                        nodes[active_count++] = create_node((uint32_t)symbol, frequencies[symbol], NULL, NULL);
                        if (!nodes[active_count - 1]) {
                                goto cleanup;
                        }
                }
        }

        if (active_count == 0) {
                goto cleanup;
        }

        // if only one unique symbol exists
        if (active_count == 1) {
                Node *root = create_node(0, nodes[0]->frequency, nodes[0], NULL);
                if (!root) {
                        goto cleanup;
                }
                free(nodes);
                return root;
        }

        while (active_count > 1) {
                size_t min1_idx = (size_t)-1;
                size_t min2_idx = (size_t)-1;
                uint32_t smallest = UINT32_MAX;
                uint32_t second_smallest = UINT32_MAX;

                for (size_t j = 0; j < active_count; j++) {
                        uint32_t freq = nodes[j]->frequency;
                        if (freq < smallest) {
                                second_smallest = smallest;
                                min2_idx = min1_idx;
                                smallest = freq;
                                min1_idx = j;
                        } else if (freq < second_smallest) {
                                second_smallest = freq;
                                min2_idx = j;
                        }
                }

                Node *parent = create_node(0, smallest + second_smallest, nodes[min1_idx], nodes[min2_idx]);
                if (!parent) {
                        goto cleanup;
                }

                size_t high = (min1_idx > min2_idx) ? min1_idx : min2_idx;
                size_t low = (min1_idx > min2_idx) ? min2_idx : min1_idx;

                nodes[high] = nodes[active_count - 1];
                nodes[active_count - 1] = NULL;
                active_count--;

                nodes[low] = nodes[active_count - 1];
                nodes[active_count - 1] = NULL;
                active_count--;

                nodes[active_count++] = parent;
        }

        Node *root = nodes[0];
        free(nodes);
        return root;

cleanup:
        free_nodes(nodes, alphabet_size);
        free(nodes);
        return NULL;
}

/* Huffman compression from TokenList to compressed bits */
int huff_write(FILE *output_file, const TokenList *tokens, uint64_t original_len) {
        int result = 1;

        /* Number of symbols in the literal/length and distance alphabets. */
        int litlen_size = sym_litlen_alphabet_size();
        int dist_size = sym_dist_alphabet_size();

        /* Frequency tables filled from the input token list. */
        uint32_t *litlen_freq = NULL;
        uint32_t *dist_freq = NULL;

        /* Tree-derived code length for every symbol in each alphabet. */
        unsigned *litlen_lengths = NULL;
        unsigned *dist_lengths = NULL;

        /* Tree-derived code and bit length for every symbol. */
        HuffmanCode *litlen_codes = NULL;
        HuffmanCode *dist_codes = NULL;

        /* Huffman trees used directly for code generation and serialization. */
        Node *litlen_tree = NULL;
        Node *dist_tree = NULL;

        /* Bit writer for the encoded token stream. */
        BitWriter bw = {0};

        if (!output_file || !tokens || litlen_size <= 0 || dist_size <= 0) {
                return 1;
        }

        litlen_freq = calloc((size_t)litlen_size, sizeof(*litlen_freq));
        dist_freq = calloc((size_t)dist_size, sizeof(*dist_freq));
        if (!litlen_freq || !dist_freq) {
                free(litlen_freq);
                free(dist_freq);
                return 1;
        }

        count_frequencies(tokens, litlen_freq, dist_freq);

        litlen_tree = build_huffman_tree(litlen_freq, (size_t)litlen_size);
        dist_tree = build_huffman_tree(dist_freq, (size_t)dist_size);

        return result;
}

/* Huffman decompression from compressed bits to TokenList */
int huff_read(FILE *output_file, TokenList *output, uint64_t *original_len) {
        int result = 1;
        return result;
}

