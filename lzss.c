/** Handles the LZSS part of the compression. (like Lempel–Ziv) */

#include "codec.h"
#include <stdlib.h>

enum {
	HASH_BITS = 15,
	HASH_SIZE = (1u << HASH_BITS),
	CHAIN_LIMIT = 128
};

#define CHAIN_EMPTY SIZE_MAX

static size_t hash_function(const uint8_t *input, size_t pos) {
	uint32_t hash = 0;
	for (int i = 0; i < 3; i++) {

		hash += input[pos+i] << i*8; 
	}
	return (hash * 0x9E3779B1) >> (32 - HASH_BITS);

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

static size_t match_length(const uint8_t *input, size_t current_index, size_t candidate_index, size_t max_length) {
	size_t length = 0;

	while (length < max_length && input[current_index++] == input[candidate_index++]){
		length++;
	}
	return length;
}

/*
int lz_encode(const uint8_t *input, size_t input_size, TokenList *output) {
	size_t *head = malloc(MAX_DISTANCE * sizeof(uint8_t));
	size_t *prev = malloc(MAX_DISTANCE * sizeof(uint8_t));

}
*/

int lz_encode(const uint8_t *input, size_t input_size, TokenList *output) {

	size_t pos = 0;
	while (pos < input_size) {
		size_t max_length = (MAX_MATCH < input_size - pos) ? MAX_MATCH : input_size - pos; 
		size_t window_start = (pos > MAX_DISTANCE) ? pos - MAX_DISTANCE : 0; 

		size_t best_length_pos = 0; 
		size_t best_length = 0; 
		for (size_t candidate_pos = pos; candidate_pos > window_start; candidate_pos--) {
			size_t new_length = match_length(input, pos, candidate_pos-1, max_length);
			if (new_length > best_length) {
				best_length_pos = candidate_pos-1; 
				best_length = new_length;
			}
			if (best_length == max_length) {
				break;
			}
		}
		Token token;
		if (best_length >= MIN_MATCH) {
			token = (Token){
				.kind = TOK_MATCH, 
				.length = best_length,
				.distance = pos - best_length_pos
			};
			pos += best_length;
		} else {
			token = (Token){
				.kind = TOK_LITERAL, 
				.literal = input[pos]
			};
			pos++;
		}

		if (token_pusher(output, token) != 0) {
			return 1;
		}
	}
	return 0; 
	
}

int lz_decode(const TokenList *tokens, uint8_t *output, size_t output_size) {

	size_t out_pos = 0; 
	// while (out_pos <= output_size) {
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

