#include "compression.h"

#include <stdio.h>
#include <string.h>

static void print_help(void) {
        printf("Bruk: bitteliten [valg] [fil]\n\n");
        printf("Et filkomprimeringsprogram.\n\n");
        printf("Valg:\n");
        printf("  -k, --komprimer                               Komprimerer "
               "filen\n");
        printf("  -d, --dekomprimer                             Dekomprimerer "
               "filen\n");
        printf("  -h, --hjelp, --hjelp-vi-skal-komprimere       Viser "
               "hjelpeteksten\n\n");
        printf("Eksempel:\n");
        printf("  bitteliten -k dokument.txt\n");
        printf("  bitteliten -d dokument.txt.btln\n");
}

int main(int argc, char **argv) {
        if (argc == 1) {
                fprintf(stderr,
                        "Feil: Ingen argumenter eller fil oppgitt.\n\n");
                print_help();
                return 1;
        }

        for (int i = 1; i < argc; i++) {
                if (strcmp(argv[i], "-h") == 0 ||
                    strcmp(argv[i], "--hjelp") == 0 ||
                    strcmp(argv[i], "--hjelp-vi-skal-komprimere") == 0) {
                        print_help();
                        return 0;
                }

                if (strcmp(argv[i], "-k") == 0 ||
                    strcmp(argv[i], "--komprimer") == 0) {
                        if (i + 1 >= argc) {
                                fprintf(stderr,
                                        "Feil: Mangler filnavn etter '%s'.\n",
                                        argv[i]);
                                return 1;
                        }

                        const char *input = argv[++i];
                        char output[strlen(input) + sizeof(".btln")];

                        strcpy(output, input);
                        strcat(output, ".btln");

                        if (compress_file(input, output) != 0) {
                                fprintf(stderr,
                                        "Feil: Kunne ikke komprimere '%s'.\n",
                                        input);
                                return 1;
                        }

                        continue;
                }

                if (strcmp(argv[i], "-d") == 0 ||
                    strcmp(argv[i], "--dekomprimer") == 0) {
                        if (i + 1 >= argc) {
                                fprintf(stderr,
                                        "Feil: Mangler filnavn etter '%s'.\n",
                                        argv[i]);
                                return 1;
                        }

                        const char *input = argv[++i];
                        size_t len = strlen(input);
                        const char *extension = ".btln";
                        size_t extension_len = strlen(extension);
                        char output[len + sizeof(".out")];

                        // remove .btln extension if present
                        if (len >= extension_len &&
                            strcmp(input + len - extension_len, extension) ==
                                0) {
                                size_t output_len = len - extension_len;

                                for (size_t j = 0; j < output_len; j++) {
                                        output[j] = input[j];
                                }

                                output[output_len] = '\0';
                        } else {
                                strcpy(output, input);
                                strcat(output, ".out");
                        }

                        if (decompress_file(input, output) != 0) {
                                fprintf(stderr,
                                        "Feil: Kunne ikke dekomprimere '%s'.\n",
                                        input);
                                return 1;
                        }

                        continue;
                }

                fprintf(stderr, "Ukjent argument: %s\n\n", argv[i]);
                print_help();
                return 1;
        }

        return 0;
}
