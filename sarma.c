#define ESTD_ALL_IMPLEMENTATION
#include <stdio.h>
#include <sys/stat.h>

#include "estd/arena.h"
#include "estd/common.h"
#include "estd/log.h"
#include "estd/result.h"
#include "estd/str.h"
#include "estd/string_builder.h"

typedef struct Node Node;
struct Node {
    uint8_t bytes[32];
    int16_t children[2];
    size_t frequency;
    size_t index;
};

static void calculateFrequencies(EstdString data, Node result[static 256]) {
    for (size_t i = 0; i < 256; i++) {
        result[i] = (Node){.bytes = {0}, .children = {-1, -1}, .frequency = 0, .index = i};
        result[i].bytes[i / 8] |= 1 << (i % 8);
    }

    for (size_t i = 0; i < data.length; i++) {
        result[data.data[i]].frequency += 1;
    }
}

static int compareNodes(void const* l, void const* r) {
    Node const* lhs = (Node const*)l;
    Node const* rhs = (Node const*)r;
    if (lhs->frequency == 0) {
        return 1;
    } else if (rhs->frequency == 0) {
        return -1;
    } else if (lhs->frequency < rhs->frequency) {
        return -1;
    } else if (lhs->frequency > rhs->frequency) {
        return 1;
    } else if (lhs->index < rhs->index) {
        return -1;
    } else if (lhs->index > rhs->index) {
        return 1;
    } else {
        return 0;
    }
}

static void printNode(Node const* node) {
    printf("%3zu\t", node->index);
    for (int i = 0; i < 256; i++) {
        if (node->bytes[i / 8] & (1 << (i % 8))) {
            printf("%c", i);
        }
    }
    printf(": %zu (%d %d)\n", node->frequency, node->children[0], node->children[1]);
}

typedef struct Code Code;
struct Code {
    char data[256];
    uint8_t length;
};

static void calculateCodes(Node nodes[static 512], size_t index, Code codes[static 256], Code* code) {
    Node const* node = &nodes[index];
    if (node->children[0] == -1 || node->children[1] == -1) {
        codes[node->index] = *code;
    } else {
        code->length += 1;
        code->data[code->length - 1] = '0';
        calculateCodes(nodes, node->children[0], codes, code);
        code->data[code->length - 1] = '1';
        calculateCodes(nodes, node->children[1], codes, code);
        code->length -= 1;
    }
}

EstdResult compress(char const* path) {
    EstdArena* ESTD_CLEAN(estd_arena_destroy) arena = NULL;
    EstdString file_data = {0};
    FILE* fp = fopen(path, "rb");
    if (fp == NULL) {
        ESTD_THROW(ESTD_IO_ERROR, "Could not open file %s", path);
    }
    ESTD_BUBBLE(estd_read_file(&file_data, &arena, fp), "Could not read parameter file");
    fclose(fp);
    Node nodes[512];
    size_t next_node = 256;
    calculateFrequencies(file_data, nodes);
    Node queue[256];
    size_t queue_size = 256;

    memcpy(queue, nodes, sizeof(queue));
    qsort(queue, queue_size, sizeof(Node), &compareNodes);
    while (queue_size > 0 && queue[queue_size - 1].frequency == 0) {
        queue_size -= 1;
    }

    while (queue_size > 1) {
        Node merged = (Node
        ){.children = {queue[0].index, queue[1].index},
          .index = next_node,
          .frequency = queue[0].frequency + queue[1].frequency};

        for (int i = 0; i < 32; i++) {
            merged.bytes[i] = queue[0].bytes[i] | queue[1].bytes[i];
        }
        nodes[next_node] = merged;
        queue[0] = merged;
        queue[1] = queue[queue_size - 1];
        queue_size -= 1;
        next_node += 1;
        qsort(queue, queue_size, sizeof(Node), &compareNodes);
    }

    Code codes[256] = {0};
    Code code = {0};
    calculateCodes(nodes, next_node - 1, codes, &code);
    for (size_t i = 0; i < 256; i++) {
        if (codes[i].length == 0) {
            continue;
        }
        printf("%2hhx (%c)\t\"%.*s\"\n", (int)i, (int)i, (int)codes[i].length, codes[i].data);
    }
    return ESTD_SUCCESS;
}

static void fcloseWrapper(void* var) {
    FILE* fp = *(FILE**)var;
    if (fp != NULL) {
        fclose(fp);
    }
}

#define SARMA_VERSION_MAJOR 0
#define SARMA_VERSION_MINOR 1

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: sarma <output> <inputs...>\n");
        return ESTD_SUCCESS;
    }
    EstdArena* ESTD_CLEAN(estd_arena_destroy) temp = NULL;
    EstdString sarma_file_name;
    EstdString dolma_file_name;
    ESTD_BUBBLE(estd_string_format(&sarma_file_name, &temp, "%s", argv[1]), "Could not create sarma file name");
    ESTD_BUBBLE(estd_string_format(&dolma_file_name, &temp, "%s.dolma", argv[1]), "Could not create dolma file name");

    FILE* ESTD_CLEAN(fclose) sarma_file = fopen(sarma_file_name.data, "wb");
    fprintf(sarma_file, "SARMA %d.%d\r\n", SARMA_VERSION_MAJOR, SARMA_VERSION_MINOR);
    fprintf(sarma_file, "File-Count: %d\r\n", argc - 2);
    fprintf(sarma_file, "\r\n");

    FILE* ESTD_CLEAN(fclose) dolma_file = fopen(dolma_file_name.data, "wb");
    
    for (int i = 2; i < argc; i++) {
        EstdArena* ESTD_CLEAN(estd_arena_destroy) arena = NULL;
        EstdString file_data = {0};
        FILE* ESTD_CLEAN(fclose) file = fopen(argv[i], "rb");
        if (file == NULL) {
            ESTD_THROW(ESTD_IO_ERROR, "Could not open file %s", argv[i]);
        }

        struct stat file_stat;
        fstat(fileno(file), &file_stat);

        char last_access[32];
        char modification[32];
        {
            struct tm* tm = gmtime(&file_stat.st_atime);
            strftime(last_access, sizeof(last_access), "%Y-%m-%d %H:%M:%S", tm);
        }
        {
            struct tm* tm = gmtime(&file_stat.st_mtime);
            strftime(modification, sizeof(modification), "%Y-%m-%d %H:%M:%S", tm);
        }
        uint32_t permissions = file_stat.st_mode & 0777;

        ESTD_BUBBLE(estd_read_file(&file_data, &arena, file), "Could not read input file");
        uint32_t crc32 = estd_crc32(file_data);
        fprintf(sarma_file, "Content-Name: %" PRIestr "\r\n", ESTD_STRING_ARG(estd_path_get_filename(ESTD_CTRING(argv[i]))));
        fprintf(sarma_file, "Content-Size: %zu\r\n", file_data.length);
        fprintf(sarma_file, "Last-Accessed-Time: %s\r\n", last_access);
        fprintf(sarma_file, "Modification-Time: %s\r\n", modification);
        fprintf(sarma_file, "Compression: None\r\n");
        fprintf(sarma_file, "CRC32: %08x\r\n", crc32);
        fprintf(sarma_file, "Permissions: %04o\r\n", permissions);
        fprintf(sarma_file, "\r\n");
        fwrite(file_data.data, sizeof(char), file_data.length, dolma_file);
    }

    return ESTD_SUCCESS;
}
