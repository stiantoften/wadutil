#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdint.h>

static void cpy(const uint8_t *inbytes, size_t *in_i, uint8_t *outbytes, size_t *out_i, uint32_t count) {
    for (; count > 0; count--) {
        outbytes[(*out_i)++] = inbytes[(*in_i)++];
    }
}

static unsigned char *decompress(FILE *wad, const size_t insize, size_t *outsize) {
    size_t in_i = 0, out_i = 0, malloc_i = 0x2000;

    uint8_t *in = malloc(insize);
    uint8_t *out = malloc(malloc_i);

    fread(in, 1, insize, wad);

    while (in_i < insize) {
        uint32_t B1, B2, B3 = 1, block = in[in_i++];

        // maximum copy count = 0x100
        if (malloc_i < (out_i + 0x100)) {
            out = realloc(out, malloc_i += 0x10000);
        }

        if (block == 0x12 && in[in_i + 0] == 0 && in[in_i + 1] == 0 && in[in_i + 2] == 0xEE) {
            in_i = (in_i & ~0x1FFFu) + 0x2000;
            continue;
        } else if (block >= 0x40) {
            B1 = block >> 2u & 7u;
            B2 = in[in_i++] << 3u;
            block = (block >> 5u) - 1;
        } else if (block >= 0x10) {
            if (block >= 0x20) {
                block = (block &= 0x1Fu) == 0 ? in[in_i++] + 0x1F : block;
            } else {
                B3 = ((block & 8u) << 11u) + 0x4000;
                block = (block &= 7u) == 0 ? in[in_i++] + 0x07 : block;
            }
            B1 = in[in_i++] >> 2u;
            B2 = in[in_i++] << 6u;
        } else {
            cpy(in, &in_i, out, &out_i, (block == 0 ? in[in_i++] + 0x0F : block) + 3);
            continue;
        }
        if (B3 == 1 || block != 0x01) {
            size_t backref = out_i - B1 - B2 - B3;
            cpy(out, &backref, out, &out_i, block + 2);
        }
        cpy(in, &in_i, out, &out_i, in[in_i - 2] & 3u);
    }

    free(in);
    *outsize = out_i;
    return out;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Not enough arguments; need an input file!\n");
        return 0;
    }

    FILE *wad = fopen(argv[1], "rb+");
    if (wad == NULL) {
        printf("Unable to open %s!\n", argv[1]);
        return 0;
    }

    // strip off the file extension
    size_t arglen = strlen(argv[1]);
    while (argv[1][--arglen] != '.');

    char folder[0x40];
    strncpy(folder, argv[1], arglen);
    folder[arglen] = '\0';
    mkdir(folder, 0700);

    printf("%-30s%-10s%-10s%-10s%-10s\n", "file", "pos", "insize", "outsize", "status");

    char magic[4] = {0};
    int found = 0;

    while (fread(&magic, 1, 3, wad), !feof(wad)) {
        if (!strcmp(magic, "WAD")) {
            size_t insize, outsize;

            fread(&insize, 4, 1, wad);
            fseek(wad, 9, SEEK_CUR);        // 3 + 4 + 9 = 0x10

            char outfilename[0x40];
            sprintf(outfilename, "%s/subwad_%04X.WAD", folder, found);
            size_t offset = ftell(wad);

            uint8_t *data = decompress(wad, insize, &outsize);

            FILE *outfile = fopen(outfilename, "wb+");
            fwrite(data, 1, outsize, outfile);
            fclose(outfile);

            free(data);

            printf("%-30s%-10lX%-10zX%-10zXdone!\n", outfilename, offset, insize, outsize);
            found++;
        }
        // align to next 0x40
        fseek(wad, ((uint32_t)ftell(wad) & ~0x3Fu) + 0x40, SEEK_SET);
    }

    fclose(wad);
    printf("\nDone decompressing!\n");
    return 0;
}