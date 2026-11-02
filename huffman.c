/** Huffman in/decoding
 *
 * Takes in or returns a list of "symbols" wich is stored as 16 bit.
 *
 * Uses cononical huffman to minimize the header size.
 *
 */

#include "codec.h"
#include <stdlib.h>

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

typedef struct Node {
        uint32_t symbol;
        uint64_t frequency;
        struct Node *left;
        struct Node *right;
} Node;

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

static void count_frequencies(const SymbolList *symbols, uint64_t *symbol_freq) {
        for (size_t i = 0; i < symbols->count; i++) {
                symbol_freq[symbols->data[i]]++;
        }
}

static Node *create_node(uint16_t symbol, uint64_t frequency, Node *left, Node *right) {
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

static void free_nodes(Node **nodes) {
        if (!nodes)
                return;
        size_t max_nodes = BWT_ALPHABET_SIZE * 2;
        for (size_t i = 0; i < max_nodes; i++) {
                if (nodes[i] != NULL) {
                        free_tree(nodes[i]);
                        nodes[i] = NULL;
                }
        }
}

static Node *build_huffman_tree(const uint64_t *symbol_freq) {
        // Maximum possible active nodes: leaf nodes plus internal nodes.
        size_t max_nodes = BWT_ALPHABET_SIZE * 2;
        Node **nodes = calloc(max_nodes, sizeof(Node *));
        if (!nodes) {
                goto cleanup;
        }

        size_t active_count = 0;

        // Initialize one leaf for each symbol with a non-zero frequency.
        for (int symbol = 0; symbol < BWT_ALPHABET_SIZE; symbol++) {
                if (symbol_freq[symbol] > 0) {
                        nodes[active_count++] = create_node((uint32_t)symbol, symbol_freq[symbol], NULL, NULL);
                        if (!nodes[active_count - 1]) {
                                goto cleanup;
                        }
                }
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
        free_nodes(nodes);
        free(nodes);
        return NULL;
}

static void build_code_lengths(const Node *node, uint8_t *lengths, unsigned depth) {
        if (node == NULL)
                return;

        if (node->left == NULL && node->right == NULL) {
                lengths[node->symbol] = depth == 0 ? 1 : depth;
                return;
        }

        if (node->left != NULL)
                build_code_lengths(node->left, lengths, depth + 1);

        if (node->right != NULL)
                build_code_lengths(node->right, lengths, depth + 1);
}

static void build_canonical_codes(const uint8_t *lengths, HuffmanCode *codes) {
        uint32_t count[BWT_ALPHABET_SIZE + 1] = {0};
        uint32_t next_code[BWT_ALPHABET_SIZE + 1] = {0};
        unsigned max_length = 0;

        for (int symbol = 0; symbol < BWT_ALPHABET_SIZE; symbol++) {
                codes[symbol] = (HuffmanCode){0};

                if (lengths[symbol] == 0)
                        continue;

                if (lengths[symbol] > max_length)
                        max_length = lengths[symbol];

                count[lengths[symbol]]++;
        }

        uint32_t code = 0;

        for (unsigned length = 1; length <= max_length; length++) {
                code = (code + count[length - 1]) << 1;
                next_code[length] = code;
        }

        for (int symbol = 0; symbol < BWT_ALPHABET_SIZE; symbol++) {
                unsigned length = lengths[symbol];

                if (length == 0)
                        continue;

                codes[symbol].bits = next_code[length];
                codes[symbol].length = length;
                next_code[length]++;
        }
}

int huff_write_symbols(FILE *file, const SymbolList *symbols) {
        (void)file;
        (void)symbols;
        return 1;
}

int huff_read_symbols(FILE *file, SymbolList *output) {
        uint32_t *symbol_freq = calloc(BWT_ALPHABET_SIZE, sizeof(*symbol_freq));
        if (symbol_freq == NULL)
                return 1;

        free(symbol_freq);

        (void)file;
        (void)output;
        return 1;
}
