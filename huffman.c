/** Huffman en/decoding
 *
 * Uses canonical Huffman coding. Huffman tree determines code lengths, and
 * codes are assigned by length and symbol order. Only code lengths are stored to the file.
 *
 * File format of compressed data:
 *      - code lengths for the symbol alphabet are stored as 5-bit values
 *              (smallest functioning for diverse.lyx and enwik8.txt)
 *      - runs of three or more lengths of zero are RLE-encoded to minimize header
 *      - canonical Huffman-coded symbols, with the BWT_EOB symbol marking the ending
 *      - Padding to fill the final bits in the last byte when needed
 *
 * Program flow:
 *      Compression
 *      - count symbol frequencies
 *      - Build a Huffman tree and get a code length for each symbol which is used
 *      - Assign canonical codes based on those lengths
 *      - write the lengths, then the encoded symbols
 *
 *      Decompression
 *      - Read the code lengths and rebuild the canonical codes
 *      - Decode symbols until BWT_EOB
 */

#include "codec.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

enum {
        HUFFMAN_CODE_LENGTH_BITS = 5,
        HUFFMAN_RLE_MARKER = (1U << HUFFMAN_CODE_LENGTH_BITS) - 1,
        HUFFMAN_MAX_CODE_LENGTH = 32
};

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
        uint16_t symbol;
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

                uint8_t current_bit = (bits >> (length - 1 - i)) & 1;
                bw->buffer = bw->buffer | (current_bit << (7 - bw->buffer_pos));

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

                uint64_t current_bit = (br->buffer >> (7 - br->buffer_pos)) & 1;

                result = (result << 1) | current_bit;

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

static void count_frequencies(const SymbolList *symbols, uint64_t *frequencies) {
        for (size_t i = 0; i < symbols->count; i++) {
                frequencies[symbols->data[i]]++;
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

static Node *build_huffman_tree(const uint64_t *frequencies) {
        // Maximum possible active nodes: leaf nodes plus internal nodes.
        size_t max_nodes = BWT_ALPHABET_SIZE * 2;
        Node **nodes = calloc(max_nodes, sizeof(Node *));
        if (!nodes) {
                goto cleanup;
        }

        size_t active_count = 0;

        // Initialize one leaf for each symbol with a non-zero frequency.
        for (size_t symbol = 0; symbol < BWT_ALPHABET_SIZE; symbol++) {
                if (frequencies[symbol] > 0) {
                        nodes[active_count++] = create_node((uint16_t)symbol, frequencies[symbol], NULL, NULL);
                        if (!nodes[active_count - 1]) {
                                goto cleanup;
                        }
                }
        }

        while (active_count > 1) {
                size_t min1_idx = (size_t)-1;
                size_t min2_idx = (size_t)-1;
                uint64_t smallest = UINT64_MAX;
                uint64_t second_smallest = UINT64_MAX;

                for (size_t j = 0; j < active_count; j++) {
                        uint64_t frequency = nodes[j]->frequency;
                        if (frequency < smallest) {
                                second_smallest = smallest;
                                min2_idx = min1_idx;
                                smallest = frequency;
                                min1_idx = j;
                        } else if (frequency < second_smallest) {
                                second_smallest = frequency;
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

        for (size_t symbol = 0; symbol < BWT_ALPHABET_SIZE; symbol++) {
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

        for (size_t symbol = 0; symbol < BWT_ALPHABET_SIZE; symbol++) {
                unsigned length = lengths[symbol];

                if (length == 0)
                        continue;

                codes[symbol].bits = next_code[length];
                codes[symbol].length = length;
                next_code[length]++;
        }
}

static int write_code_lengths(BitWriter *writer, const uint8_t *lengths) {
        size_t i = 0;

        while (i < BWT_ALPHABET_SIZE) {
                // Run length encoding(RLE) on symbols that do not appear in the huffman table
                if (lengths[i] != 0) {
                        // no RLE
                        if (lengths[i] > HUFFMAN_MAX_CODE_LENGTH || lengths[i] >= HUFFMAN_RLE_MARKER)
                                return 1;
                        if (write_bits(writer, lengths[i], HUFFMAN_CODE_LENGTH_BITS) != 0)
                                return 1;
                        i++;
                        continue;
                }
                // potential RLE
                size_t run_length = 0;
                // count the number of zeros in a row
                while (i + run_length < BWT_ALPHABET_SIZE && lengths[i + run_length] == 0) {
                        run_length++;
                }

                size_t remaining = run_length;

                while (remaining >= 3) {
                        // using RLE :=)
                        size_t chunk = remaining > HUFFMAN_RLE_MARKER ? HUFFMAN_RLE_MARKER : remaining;

                        if (write_bits(writer, HUFFMAN_RLE_MARKER, HUFFMAN_CODE_LENGTH_BITS) != 0 ||
                            write_bits(writer, chunk, HUFFMAN_CODE_LENGTH_BITS) != 0)
                                return 1;

                        remaining -= chunk;
                }

                while (remaining > 0) {
                        // RLE does not prove benefitial
                        if (write_bits(writer, 0, HUFFMAN_CODE_LENGTH_BITS) != 0)
                                return 1;
                        remaining--;
                }

                i += run_length;
        }

        return 0;
}

static int read_code_lengths(BitReader *reader, uint8_t *lengths) {
        size_t symbol = 0;
        while (symbol < BWT_ALPHABET_SIZE) {
                uint64_t length;

                if (read_bits(reader, &length, HUFFMAN_CODE_LENGTH_BITS) != 0)
                        return 1;

                if (length != HUFFMAN_RLE_MARKER) {
                        // No RLE
                        if (length > HUFFMAN_MAX_CODE_LENGTH)
                                return 1;
                        lengths[symbol++] = (uint8_t)length;
                        continue;
                }

                // RLE!! :))
                uint64_t run_length;

                if (read_bits(reader, &run_length, HUFFMAN_CODE_LENGTH_BITS) != 0)
                        return 1;

                // error if it tries to write past the lengths[258] array
                if (run_length == 0 || run_length > BWT_ALPHABET_SIZE - symbol)
                        return 1;

                for (size_t i = 0; i < run_length; i++) {
                        lengths[symbol++] = 0;
                }
        }
        return 0;
}

static int write_encoded_symbols(BitWriter *writer, const SymbolList *symbols, const HuffmanCode *codes) {
        for (size_t i = 0; i < symbols->count; i++) {
                uint16_t symbol = symbols->data[i];
                HuffmanCode code = codes[symbol];

                if (write_bits(writer, code.bits, code.length) != 0)
                        return 1;
        }
        return 0;
}

int huff_write_symbols(FILE *file, const SymbolList *symbols) {
        uint64_t frequencies[BWT_ALPHABET_SIZE] = {0};
        uint8_t lengths[BWT_ALPHABET_SIZE] = {0};
        HuffmanCode codes[BWT_ALPHABET_SIZE] = {0};

        count_frequencies(symbols, frequencies);

        Node *tree = build_huffman_tree(frequencies);
        if (tree == NULL)
                return 1;

        build_code_lengths(tree, lengths, 0);
        build_canonical_codes(lengths, codes);

        BitWriter writer = {
            .buffer_pos = 0,
            .file_stream = file,
        };
        int result = 0;

        if (write_code_lengths(&writer, lengths) != 0)
                result = 1;
        else if (write_encoded_symbols(&writer, symbols, codes) != 0)
                result = 1;
        else if (flush(&writer) != 0)
                result = 1;

        free_tree(tree);
        return result;
}

int huff_read_symbols(FILE *file, SymbolList *output) {
        uint8_t lengths[BWT_ALPHABET_SIZE] = {0};
        HuffmanCode codes[BWT_ALPHABET_SIZE] = {0};
        BitReader reader = {
            .buffer_pos = 8,
            .file_stream = file,
        };

        if (read_code_lengths(&reader, lengths) != 0)
                return 1;

        build_canonical_codes(lengths, codes);

        unsigned max_length = 0;
        for (size_t symbol = 0; symbol < BWT_ALPHABET_SIZE; symbol++) {
                if (codes[symbol].length > max_length)
                        max_length = codes[symbol].length;
        }

        if (max_length > HUFFMAN_MAX_CODE_LENGTH)
                return 1;

        uint32_t bits = 0;
        unsigned bit_count = 0;

        while (1) {
                uint64_t bit;
                if (read_bits(&reader, &bit, 1) != 0)
                        return 1;

                bits = (bits << 1) | (uint32_t)bit;
                bit_count++;

                int found = 0;
                for (size_t symbol = 0; symbol < BWT_ALPHABET_SIZE; symbol++) {
                        if (codes[symbol].length != bit_count || codes[symbol].bits != bits)
                                continue;

                        found = 1;
                        if (symbol == BWT_EOB) {
                                if (symbols_push(output, (uint16_t)symbol) != 0)
                                        return 1;
                                return 0;
                        }

                        if (symbols_push(output, (uint16_t)symbol) != 0)
                                return 1;

                        bits = 0;
                        bit_count = 0;
                        break;
                }

                if (!found && bit_count == max_length)
                        return 1;
        }
}
