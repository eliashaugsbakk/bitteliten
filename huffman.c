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

/* Read bits into out_bits. Returns 0 on success, -1 on EOF/error. */
static int read_bits(BitReader *br, uint64_t *out_bits, unsigned length) {
        uint64_t result = 0;

        for (unsigned i = 0; i < length; i++) {
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

static void count_frequencies(const TokenList *tokens, uint32_t *literal_match_length_freq, uint32_t *distance_freq) {
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

static Node *create_node(uint32_t symbol, uint64_t frequency, Node *left,
                         Node *right) {
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

static void free_tree(Node *node) {
        if (!node)
                return;
        free_tree(node->left);
        free_tree(node->right);
        free(node);
}

static void free_nodes(Node **nodes, size_t alphabet_size) {
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

static Node *build_huffman_tree(uint32_t *frequencies, size_t alphabet_size) {
        // Maximum possible active nodes: leaf nodes plus internal nodes.
        size_t max_nodes = alphabet_size * 2;
        Node **nodes = calloc(max_nodes, sizeof(Node *));
        if (!nodes)
                goto cleanup;

        size_t active_count = 0;

        // Initialize one leaf for each symbol with a non-zero frequency.
        for (size_t symbol = 0; symbol < alphabet_size; symbol++) {
                if (frequencies[symbol] > 0) {
                        nodes[active_count++] = create_node((uint32_t)symbol,
                                                            frequencies[symbol],
                                                            NULL, NULL);
                        if (!nodes[active_count - 1]) {
                                goto cleanup;
                        }
                }
        }

        if (active_count == 0) {
                goto cleanup;
        }

        // Give a one-symbol tree a one-bit code later during code generation.
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

static void build_codes(Node *node, HuffmanCode *codes, uint32_t code, unsigned depth) {
        if (node == NULL || codes == NULL)
                return;

        // A leaf receives the path accumulated so far.
        if (node->left == NULL && node->right == NULL) {
                codes[node->symbol].bits = code;
                codes[node->symbol].length = (depth == 0) ? 1 : depth;

                return;
        }

        if (node->left != NULL)
                build_codes(node->left, codes, code, depth + 1);

        if (node->right != NULL)
                // The right branch contributes a one at the current depth.
                build_codes(node->right, codes, code | ((uint32_t)1 << depth), depth + 1);
}

static unsigned symbol_bit_width(size_t alphabet_size) {
        unsigned width = 0;
        size_t values = 1;

        while (values < alphabet_size) {
                values <<= 1;
                width++;
        }
        return width;
}

/* Serialize one Huffman tree in preorder, using packed bits. */
static int write_tree(BitWriter *bw, const Node *node, size_t alphabet_size) {
        // Tree presence field: zero means that this tree is empty.
        if (node == NULL)
                return write_bits(bw, 0, 1);

        // Tree presence field: one means that node data follows.
        if (write_bits(bw, 1, 1) != 0)
                return 1;

        if (node->left == NULL && node->right == NULL) {
                unsigned width = symbol_bit_width(alphabet_size);

                // Node type field: one means that this node is a leaf.
                if (write_bits(bw, 1, 1) != 0)
                        return 1;

                // Leaf payload: the symbol represented by this node.
                return write_bits(bw, node->symbol, width);
        }

        // Node type field: zero means that this node is internal.
        if (write_bits(bw, 0, 1) != 0)
                return 1;
        if (write_tree(bw, node->left, alphabet_size) != 0)
                return 1;
        return write_tree(bw, node->right, alphabet_size);
}

/* Encode tokens using the generated literal/length and distance codes. */
static int write_tokens(BitWriter *bw, const TokenList *tokens, const HuffmanCode *litlen_codes,
                        const HuffmanCode *dist_codes) {
        for (size_t i = 0; i < tokens->count; i++) {
                const Token *token = &tokens->data[i];

                if (token->kind == TOK_LITERAL) {
                        const HuffmanCode code = litlen_codes[token->literal];

                        // Token payload: literal/length code for the literal.
                        if (write_bits(bw, code.bits, code.length) != 0)
                                return 1;
                        continue;
                }

                if (token->kind == TOK_MATCH) {
                        int length_symbol = sym_length(token->length);
                        int distance_symbol = sym_distance(token->distance);
                        const HuffmanCode length_code = litlen_codes[length_symbol];
                        const HuffmanCode distance_code = dist_codes[distance_symbol];

                        // Token payload: Huffman-coded length symbol.
                        if (write_bits(bw, length_code.bits, length_code.length) != 0)
                                return 1;

                        // Match payload: extra bits belonging to the length symbol.
                        if (write_bits(bw, token->length - sym_length_base(length_symbol),
                                       sym_length_extra_bits(length_symbol)) != 0)
                                return 1;

                        // Match payload: Huffman-coded distance symbol.
                        if (write_bits(bw, distance_code.bits, distance_code.length) != 0)
                                return 1;

                        // Match payload: extra bits belonging to the distance symbol.
                        if (write_bits(bw, token->distance - sym_distance_base(distance_symbol),
                                       sym_distance_extra_bits(distance_symbol)) != 0)
                                return 1;
                        continue;
                }

                return 1;
        }

        return 0;
}

static int write_uint64_be(FILE *file, uint64_t value) {
        for (unsigned shift = 56; ; shift -= 8) {
                if (fputc((int)(value >> shift) & 0xff, file) == EOF)
                        return 1;
                if (shift == 0)
                        break;
        }
        return 0;
}

/* Huffman compression from TokenList to compressed bits */
int huff_write(FILE *output_file, const TokenList *tokens, uint64_t original_len) {
        int result = 1;

        // Number of symbols in the literal/length and distance alphabets.
        int litlen_size = sym_litlen_alphabet_size();
        int dist_size = sym_dist_alphabet_size();

        // Frequency tables filled from the input token list.
        uint32_t *litlen_freq = NULL;
        uint32_t *dist_freq = NULL;

        // Tree-derived code and bit length for every symbol.
        HuffmanCode *litlen_codes = NULL;
        HuffmanCode *dist_codes = NULL;

        // Trees used for code generation and serialization.
        Node *litlen_tree = NULL;
        Node *dist_tree = NULL;

        // Bit writer for the tree and token sections.
        BitWriter bw = {.file_stream = output_file};

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
        if (litlen_tree == NULL)
                goto cleanup;

        litlen_codes = calloc((size_t)litlen_size, sizeof(*litlen_codes));
        dist_codes = calloc((size_t)dist_size, sizeof(*dist_codes));
        if (litlen_codes == NULL || dist_codes == NULL)
                goto cleanup;

        build_codes(litlen_tree, litlen_codes, 0, 0);
        build_codes(dist_tree, dist_codes, 0, 0);

        /* Header: original uncompressed length, stored big-endian. */
        if (write_uint64_be(output_file, original_len) != 0)
                goto cleanup;

        /* Trees and encoded token stream. */
        if (write_tree(&bw, litlen_tree, (size_t)litlen_size) != 0)
                goto cleanup;
        if (write_tree(&bw, dist_tree, (size_t)dist_size) != 0)
                goto cleanup;
        if (write_tokens(&bw, tokens, litlen_codes, dist_codes) != 0)
                goto cleanup;

        /* End-of-stream marker: no more tokens follow this code. */
        const HuffmanCode end_code = litlen_codes[SYM_END];
        if (write_bits(&bw, end_code.bits, end_code.length) != 0)
                goto cleanup;

        if (flush(&bw) != 0)
                goto cleanup;

        result = 0;

cleanup:
        free(litlen_freq);
        free(dist_freq);
        free(litlen_codes);
        free(dist_codes);
        free_tree(litlen_tree);
        free_tree(dist_tree);

        return result;
}

/* Huffman decompression from compressed bits to TokenList */
int huff_read(FILE *output_file, TokenList *output, uint64_t *original_len) {
        int result = 1;
        return result;
}
