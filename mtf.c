#include "codec.h"

static void move_to_front(uint8_t *list, size_t pos) {
	uint8_t temp = list[pos];
	for (size_t i = pos; i > 0; i--) {
		list[i] = list[i-1];
	}

	list[0] = temp; 
}

void mtf_encode(const uint8_t *input, size_t size, uint8_t *output) {

	uint8_t bytes[256];
	for (size_t i = 0; i < 256; i++) {
		bytes[i] = i;
	}

	for (size_t i = 0; i < size; i++) {
		uint8_t current = input[i];

		size_t pos = 0; 
		while (bytes[pos] != current) {
			pos++; 
		}

		move_to_front(bytes, pos);

		output[i] = pos;

	}

}

void mtf_decode(const uint8_t *input, size_t size, uint8_t *output) {

	uint8_t bytes[256];
	for (size_t i = 0; i < 256; i++) {
		bytes[i] = i;
	}

	for (size_t i = 0; i < size; i++) {
		size_t pos = input[i]; 

		output[i] = bytes[pos]; 
		
		move_to_front(bytes, pos);
	}
}
