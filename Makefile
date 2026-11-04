CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2

SRC = main.c compression.c bwt.c mtf.c zero_run.c huffman.c file_reader_writer.c
HEADERS = codec.h compression.h file_reader_writer.h

bitteliten: $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) $(SRC) -o bitteliten

clean:
	rm -f bitteliten

.PHONY: clean
