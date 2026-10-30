#include "codec.h"
#include <stdlib.h>

static size_t size_glob;
static const uint8_t *input_glob;


static int compare(const void *a, const void *b) {
	const size_t *valA = a;
	const size_t *valB = b;

	size_t start_a = *valA;
	size_t start_b = *valB;

	for (size_t i = 0; i < size_glob; i++) {
		uint8_t x = input_glob[(start_a + i) % size_glob];
		uint8_t y = input_glob[(start_b + i) % size_glob];
		if (x > y) {
			return 1;
		} else if (x < y) {
			return -1;
		}
	}

	return 0; 
}

int bwt_forward(const uint8_t *input, size_t size, uint8_t *output, uint32_t *primary_index) {
	*primary_index = 0;
	if (size == 0) {
		return 0;
	}


	size_t *start_positions = malloc(sizeof(size_t)*(size));

	if (start_positions == NULL) {
		return 1;
	}

	for (size_t i = 0; i < size; i++) {
		start_positions[i] = i; 
	}

	size_glob = size;
	input_glob = input; 

	qsort(start_positions, size, sizeof(size_t), compare);

	
	for (size_t row = 0; row < size; row++) {
		size_t start = start_positions[row];

		output[row] = input[(start + size - 1) % size];

		if (start == 0) {
			*primary_index = row; 
		}

	}

	
	free(start_positions);
	return 0; 
}
