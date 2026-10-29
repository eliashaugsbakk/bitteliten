bitteliten: main.c compression.c bwt.c huffman.c file_reader_writer.c
	gcc main.c compression.c bwt.c huffman.c file_reader_writer.c -o bitteliten

clean:
	rm -f bitteliten
