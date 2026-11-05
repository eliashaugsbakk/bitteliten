#include "compression.h"
#include "file_reader_writer.h"
#include "codec.h"

#include <stdio.h>
#include <stdlib.h>

int compress_file(const char *input_filename, const char *output_filename) {
	Buffer *input = NULL;
	FILE *output_file = NULL;
	uint8_t *bwt_output = NULL;
	uint8_t *mtf_output = NULL; 
	SymbolList zero_run_output = {0};

	int result = 1; 
	input = read_file(input_filename);

	if (input == NULL) {
		goto cleanup;
	}

	output_file = fopen(output_filename, "wb");

	if (output_file == NULL) {
		goto cleanup;
	}

	size_t position = 0;
	bwt_output = malloc(sizeof(uint8_t)*BWT_BLOCK_SIZE);
	mtf_output = malloc(sizeof(uint8_t)*BWT_BLOCK_SIZE); 

	if (bwt_output == NULL || mtf_output == NULL) {
		goto cleanup; 
	}

	while (position < input->size) {

		uint32_t primary_index; 

		uint32_t block_size = (BWT_BLOCK_SIZE < input->size - position) ? BWT_BLOCK_SIZE : input->size - position;

		if (bwt_forward(input->data+position, block_size, bwt_output, &primary_index)) {
			goto cleanup; 
		}

		mtf_encode(bwt_output, block_size, mtf_output);
		
		if (zero_run_encode(mtf_output, block_size, &zero_run_output)) {
			goto cleanup; 
		}

		if (fwrite(&block_size, sizeof(uint32_t), 1, output_file) != 1) {
			goto cleanup; 
		}

		if (fwrite(&primary_index, sizeof(uint32_t), 1, output_file) != 1) {
			goto cleanup; 
		}

		if (huff_write_symbols(output_file, &zero_run_output)) {
			goto cleanup; 
		}

		position += BWT_BLOCK_SIZE;
		symbols_free(&zero_run_output);

	}
	result = 0; 

	cleanup:

	if (output_file != NULL) {
		if (fclose(output_file) != 0) {
			result = 1;
		}
		if (result) {
			remove(output_filename);
		}
	} 	
	symbols_free(&zero_run_output);
	free(bwt_output);
	free(mtf_output);
	free_buffer(input);
	return result;
}

int decompress_file(const char *input_filename, const char *output_filename) {
	FILE *input = NULL;
	FILE *output = NULL;

	uint8_t *bwt_output = NULL;
	uint8_t *mtf_output = NULL;
	uint8_t *zero_run_output = NULL; 
	SymbolList huffman_output = {0};

	int result = 1;
	input = fopen(input_filename, "rb");

	if (input == NULL) {
		goto cleanup;
	}

	output = fopen(output_filename, "wb");

	if (output == NULL) {
		goto cleanup;
	}

	bwt_output = malloc(sizeof(uint8_t)*BWT_BLOCK_SIZE);
	mtf_output = malloc(sizeof(uint8_t)*BWT_BLOCK_SIZE); 
	zero_run_output = malloc(sizeof(uint8_t)*BWT_BLOCK_SIZE);

	if (bwt_output == NULL || mtf_output == NULL || zero_run_output == NULL) {
		goto cleanup; 
	}

	for (;;) {

		uint32_t block_size = 0; 
		uint32_t primary_index = 0; 
		uint32_t header[2];

		size_t header_bytes = fread(header, 1, 8, input);

		if (header_bytes == 0 && feof(input)) {
			break;
		} else if (header_bytes != 8) {
			goto cleanup; 
		}

		block_size = header[0];
		primary_index = header[1];

		if (block_size == 0 || block_size > BWT_BLOCK_SIZE || primary_index >= block_size) {
			goto cleanup;
		}

		if (huff_read_symbols(input, &huffman_output)) {
			goto cleanup;
		}
		
		if (zero_run_decode(&huffman_output, zero_run_output, block_size)) {
			goto cleanup;
		}

		mtf_decode(zero_run_output, block_size, mtf_output);

		if (bwt_inverse(mtf_output, block_size, primary_index, bwt_output)) {
			goto cleanup;
		}

		if (fwrite(bwt_output, sizeof(uint8_t), block_size, output) != block_size) {
			goto cleanup;
		}
		
		symbols_free(&huffman_output);

	}
	result = 0; 

	cleanup:
	if (input != NULL) {
		if (fclose(input) != 0) {
			result = 1; 
		}
	}
	if (output != NULL) {
		if (fclose(output) != 0) {
			result = 1;
		}
		if (result) {
			remove(output_filename);
		}
	} 	
	symbols_free(&huffman_output);
	free(zero_run_output);
	free(bwt_output);
	free(mtf_output);
	return result;

}
