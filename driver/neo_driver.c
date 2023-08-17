/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include <neo_core.h>
#include <neo_compiler.h>

#define PROMPTCHAR ">"

static bool show_exit(void) {
    return false;
}

static bool show_help(void) {
    printf("Neo Interactive Shell\n");
    printf("Type \"help\", \"version\", \"license\" for more information.\n");
    printf("Press enter twice to execute code.\n\n");
    return true;
}

static bool show_version(void) {
    printf("(c) Copyright Mario \"Neo\" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com\n");
    printf("Neo v." NEO_STRINGIZE(NEO_VER_MAJOR) "." NEO_STRINGIZE(NEO_VER_MINOR) " for " NEO_OS_NAME"\n");
    printf("Buildinfo: " NEO_COM_NAME " | " NEO_OS_NAME " | " NEO_CPU_NAME " | " NEO_CRT_NAME  " | " __DATE__ " " __TIME__ "\n");
    return true;
}

static bool show_license(void) {
    return true;
}

typedef struct command_t {
    const char *kw;
    bool (*cmd)(void);
} command_t;

const command_t CMD_LIST[] = {
    {"exit", &show_exit},
    {"help", &show_help},
    {"version", &show_version},
    {"license", &show_license}
};

static uint8_t *input_cmd(FILE *f, size_t *plen) {
    neo_dassert(f && plen);
    size_t len = 0;
    size_t size = 1<<7;
    uint8_t *buf = neo_memalloc(NULL, sizeof(*buf)*size);
    int cch; /* Current character. */
    int pch = 0; /* Previous character. */
    for (;;) {
        cch = fgetc(f);
        if (cch == EOF) { break; }
        else if (cch == '\n') {
            printf(PROMPTCHAR " ");
            if (pch == '\n') {
                --len;
                break;
            }
        } else if (cch >= 0x80) { /* Currently, we only support ASCII characters. */
            neo_error("Invalid ASCII character: %c", cch);
            break;
        }
        buf[len++] = (uint8_t)cch;
        if (len+1 >= size) { /* Grow buffer. */
            buf = neo_memalloc(buf, (size<<=1)*sizeof(*buf));
        }
        pch = cch;
    }
    buf[len] = '\n';
    buf[len+1] = '\0';
    *plen = len;
    return buf;
}

static void interactive_shell_input_loop(neo_compiler_t *compiler) {
    bool promt = true;
    for (;;) {
        if (promt) {
            printf(PROMPTCHAR " ");
            promt ^= true;
        }
        size_t len = 0;
        const uint8_t *input = input_cmd(stdin, &len);
        if (neo_unlikely(!len || !input)) { promt = false; continue; }
        compiler_compile(compiler, input, (const uint8_t *)"stdin", NULL);
        promt = true;
        neo_memalloc((void*)input, 0); /* Free input buffer. */
    }
}

static void interactive_shell(void) {
    show_help();

    neo_compiler_t *compiler = NULL;
    compiler_init(&compiler, COM_FLAG_NONE);
    interactive_shell_input_loop(compiler);
    compiler_free(&compiler);
}

int main(int argc, const char **argv) {
    (void)argv;
    neo_osi_init(); /* Must be called before any other neo_* function. */
    if (argc <= 1) {
        interactive_shell();
    }
    neo_osi_shutdown();
    return EXIT_SUCCESS;
}
