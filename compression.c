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

/* TODO: per block: huff_read_symbols -> zero_run_decode -> mtf_decode -> bwt_inverse */
int decompress_file(const char *input_filename, const char *output_filename) {
        (void)input_filename;
        (void)output_filename;
        return 1;
}
