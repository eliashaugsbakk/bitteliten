#include "compression.h"
#include "file_reader_writer.h"
#include "codec.h"

#include <stdio.h>
#include <stdlib.h>

/* TODO: per block: bwt_forward -> mtf_encode -> zero_run_encode -> huff_write_symbols */
int compress_file(const char *input_filename, const char *output_filename) {
        (void)input_filename;
        (void)output_filename;
        return 1;
}

/* TODO: per block: huff_read_symbols -> zero_run_decode -> mtf_decode -> bwt_inverse */
int decompress_file(const char *input_filename, const char *output_filename) {
        (void)input_filename;
        (void)output_filename;
        return 1;
}
