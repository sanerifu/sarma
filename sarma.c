#define ESTD_ALL_IMPLEMENTATION
#include "estd/arena.h"
#include "estd/common.h"
#include "estd/log.h"
#include "estd/result.h"
#include "estd/str.h"
#include "estd/string_builder.h"

#include <stdio.h>

int main() {
    EstdArena* ESTD_CLEAN(estd_arena_destroy) arena = NULL;
    EstdString formatted = {0};
    ESTD_BUBBLE(estd_string_format(&formatted, &arena, "Hello, World!\n"), "Could not format");
    printf("%" PRIestr, ESTD_STRING_ARG(formatted));
    return ESTD_SUCCESS;
}
