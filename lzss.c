/** Handles the LZSS part of the compression. (like Lempel–Ziv) */

#include "codec.h"
#include <stdlib.h>

enum {
	HASH_BITS = 15,
	HASH_SIZE = (1u << HASH_BITS),
	CHAIN_LIMIT = 1024
};

#define CHAIN_EMPTY UINT32_MAX

static size_t hash_function(const uint8_t *input, size_t pos) {
	uint32_t hash = 0;
	for (int i = 0; i < 3; i++) {

		hash += input[pos+i] << i*8; 
	}
	return (hash * 0x9E3779B1) >> (32 - HASH_BITS);

}

static void insert_position(const uint8_t *input, const size_t input_size, uint32_t *head, uint32_t *prev, size_t pos) {
	if (input_size - pos < 3) {
		return;
	}

	uint32_t hash = hash_function(input, pos);

	prev[pos] = head[hash];
	head[hash] = pos;
	
}

static size_t match_length(const uint8_t *input, size_t current_index, size_t candidate_index, size_t max_length) {
	size_t length = 0;

	while (length < max_length && input[current_index++] == input[candidate_index++]){
		length++;
	}
	return length;
}

static size_t search(const uint8_t *input, const size_t input_size, uint32_t *head, uint32_t *prev, size_t pos, size_t *best_length_pos) {
	size_t max_length = (MAX_MATCH < input_size - pos) ? MAX_MATCH : input_size - pos; 

	size_t best_length = 0; 

	size_t candidate;
	if (input_size - pos >= 3) {
		candidate = head[hash_function(input, pos)];
	} else {
		candidate = CHAIN_EMPTY;
	}

	size_t chain_counter = 0;
	while (candidate != CHAIN_EMPTY && chain_counter < CHAIN_LIMIT) {
		if (pos - candidate > MAX_DISTANCE) {
			break;
		}

		size_t new_length = match_length(input, pos, candidate, max_length);
		if (new_length > best_length) {
			*best_length_pos = candidate; 
			best_length = new_length;
		}
		if (best_length == max_length) {
			break;
		}

		candidate = prev[candidate];
		chain_counter++;
	}

	return best_length;
}

static int token_pusher(TokenList *tokens, Token token) {
	if (tokens->count == tokens->capacity) {
		size_t capacity = (tokens->capacity == 0) ? 1000 : tokens->capacity*2;

		Token *temp = realloc(tokens->data, sizeof(Token)*capacity);
		if (temp == NULL) {
			return 1;
		}

		tokens->data = temp;
		tokens->capacity = capacity;
	}

	tokens->data[tokens->count++] = token;
	return 0;
}


int lz_encode(const uint8_t *input, size_t input_size, TokenList *output) {
	if (input_size == 0) {
		return 0;
	} else if (input_size >= UINT32_MAX) {
		return 1;
	}
	uint32_t *head = malloc(HASH_SIZE*sizeof(uint32_t));
	uint32_t *prev = malloc(input_size*sizeof(uint32_t));

	if (head == NULL || prev == NULL) {
		free(head);
		free(prev);
		return 1;
	}

	for (size_t i = 0; i < HASH_SIZE; i++) {
		head[i] = CHAIN_EMPTY;
	}

	size_t pos = 0;
	while (pos < input_size) {
		size_t best_length_pos = 0;
		size_t best_length = search(input, input_size, head, prev, pos, &best_length_pos);


		insert_position(input, input_size, head, prev, pos);

		Token token;
		if (best_length >= MIN_MATCH) {
			size_t next_pos = 0; 
			if (best_length < search(input, input_size, head, prev, pos+1, &next_pos)) {
				token = (Token){
					.kind = TOK_LITERAL, 
					.literal = input[pos]
				};
				pos++;
			} else {
				token = (Token){
					.kind = TOK_MATCH, 
					.length = best_length,
					.distance = pos - best_length_pos
				};

				for (size_t i = 1; i < best_length; i++) {
					insert_position(input, input_size, head, prev, pos+i);
				}
				pos += best_length;

			}
		} else {
			token = (Token){
				.kind = TOK_LITERAL, 
				.literal = input[pos]
			};
			pos++;
		}

		if (token_pusher(output, token) != 0) {
			free(head);
			free(prev);
			return 1;
		}
	}

	free(head);
	free(prev);
	return 0; 

}


int lz_decode(const TokenList *tokens, uint8_t *output, size_t output_size) {

	size_t out_pos = 0; 
	for (size_t token_index = 0; token_index < tokens->count; token_index++) {
		Token token = tokens->data[token_index];
		
		if (token.kind == TOK_LITERAL && out_pos < output_size) {
			output[out_pos] = token.literal;
			out_pos++; 
		} else if (token.kind == TOK_MATCH && (token.distance > 0 && token.distance <= out_pos) && token.length <= output_size - out_pos) {
			for (size_t i = 0; i < token.length; i++) {
				output[out_pos] = output[out_pos - token.distance];
				out_pos++;
			}
		} else {
			return 1;
		}
	}

	if (out_pos != output_size) { 
		return 1;
	}
	return 0;
}

void tokens_free(TokenList *tokens) {
	if (tokens == NULL) {
		return;
	}
	free(tokens->data);
	tokens->data = NULL;
	tokens->count = 0;
	tokens->capacity = 0;
}

