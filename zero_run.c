
#include "codec.h"
#include <stdlib.h>

int symbols_push(SymbolList *symbols, uint16_t symbol) {

	if (symbols->count == symbols->capacity) {
		size_t cap; 
		if (symbols->capacity == 0) {
			cap = 1000;
		} else {
			cap = symbols->capacity * 2;
		}

		uint16_t *temp = realloc(symbols->data, cap*sizeof(uint16_t));

		if (temp == NULL) {
			return 1; 
		}

		symbols->capacity = cap; 
		symbols->data = temp;

	}

	symbols->data[symbols->count] = symbol;
	symbols->count++; 

	return 0;
}

static int write_run(SymbolList *output, size_t run) {
	while (run > 0) {
		if (run%2) {
			if (symbols_push(output, BWT_RUNA)) {
				return 1;
			}
			run = (run-1)/2;
		} else {
			if (symbols_push(output, BWT_RUNB)) {
				return 1;
			}
			run = (run-2)/2;
		}
	}
	return 0; 
	
}

void symbols_free(SymbolList *symbols) {
	if (symbols == NULL) {
		return; 
	}

	free(symbols->data); 
	symbols->data = NULL; 
	symbols->count = 0;
	symbols->capacity = 0; 
}

int zero_run_encode(const uint8_t *input, size_t size, SymbolList *output) {
	size_t run = 0; 
	for (size_t i = 0; i < size; i++) {
		uint8_t byte = input[i]; 
		if (byte != 0) {
	
			if (write_run(output, run) || symbols_push(output, byte+1)) {
				return 1;
			}
			run = 0; 
		} else {
			run++;
		}
	}
	if (write_run(output, run) || symbols_push(output, BWT_EOB)) {
		return 1;
	}

	return 0;
}

static int flush_run(size_t run, size_t *out_pos, size_t output_size, uint8_t *output) {
			if (run > output_size - *out_pos) {
				return 1; 
			}
			for (size_t j = 0; j < run; j++) {
				output[*out_pos] = 0; 
				(*out_pos)++;
			}
			return 0;
}

int zero_run_decode(const SymbolList *symbols, uint8_t *output, size_t output_size) {
	
	size_t out_pos = 0; 
	size_t run = 0; 

	size_t weight = 1;
	int seen_eob = 0;

	for (size_t i = 0; i < symbols->count; i++)  {
		if (symbols->data[i] == BWT_RUNA) {
			run += weight;
			if ( run > output_size) {
				return 1; 
			}
			weight *= 2;
		} else if (symbols->data[i] == BWT_RUNB) {
			run += 2*weight;
			if (run > output_size) {
				return 1; 
			}
			weight *= 2;
		} else if (symbols->data[i] >= 2 && symbols->data[i] <= 256) {
			if (flush_run(run, &out_pos, output_size, output)) {
				return 1;
			}
			run = 0; 
			weight = 1; 
			if (out_pos >= output_size) {
				return 1;
			}
			output[out_pos] = symbols->data[i]-1;
			out_pos++;
		} else if (symbols->data[i] == BWT_EOB) {
			if (flush_run(run, &out_pos, output_size, output)) {
				return 1;
			}
			seen_eob = 1;
			break;
		} else {
			return 1;
		}
	}
	if (seen_eob && out_pos == output_size) {
		return 0;
	} else {
		return 1;
	}

}
