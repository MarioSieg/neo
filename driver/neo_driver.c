/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include <neo_core.h>
#include <neo_compiler.h>

#define PROMPT ">>>"

static void show_exit(const uint8_t *cmd) {
    (void)cmd;
}

static void show_help(const uint8_t *cmd) {
    (void)cmd;
    printf("Neo Interactive Shell\n");
    printf("Type \"help\", \"version\", \"license\" for more information.\n");
    printf("Press enter twice to execute code.\n\n");
}

static void show_version(const uint8_t *cmd) {
    (void)cmd;
#if defined(NEO_DBG) && NEO_DBG == 1
#define NEO_BUILDMODE_NAME "Debug"
#else
#define NEO_BUILDMODE_NAME "Release"
#endif
    printf("(c) Copyright Mario \"Neo\" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com\n");
    printf("Neo " NEO_BUILDMODE_NAME " v." NEO_STRINGIZE(NEO_VER_MAJOR) "." NEO_STRINGIZE(NEO_VER_MINOR) " for " NEO_OS_NAME"\n");
    printf("Buildinfo: " NEO_COM_NAME " | " NEO_OS_NAME " | " NEO_CPU_NAME " | " NEO_CRT_NAME  " | " __DATE__ " " __TIME__ "\n");
}

static void show_license(const uint8_t *cmd) {
    (void)cmd;
}

typedef struct command_t {
    const char *kw;
    void (*cmd)(const uint8_t *cmd);
} command_t;

const command_t shell_commands[] = {
    {"exit", &show_exit},
    {"help", &show_help},
    {"version", &show_version},
    {"license", &show_license}
};

#define is_done(c) (((c) == EOF) || (c) == 3 || (c) == 4)

static NEO_NODISCARD const uint8_t *read_source_from_shell(size_t *plen) { /* Reads UTF-8 source code from stdin until the user presses return twice. */
    neo_dassert(plen != NULL);
    bool prompt = true;
    FILE *in = stdin;
    FILE *out = stdout;
    size_t len = 0, cap = 128;
    uint8_t *buf = neo_memalloc(NULL, sizeof(*buf)*cap);
    uint8_t utf8[5];
    uint32_t curr, prev = 0;
    for (;;) {
        if (prompt) { fprintf(out, PROMPT " "); prompt = false; }
        int tmp = fgetc(in);
        if (is_done(tmp)) { break; }
        curr = (uint32_t)tmp;
        uint32_t u8len = utf8_seqlen(curr);
        if (neo_unlikely(!u8len)) { continue; } /* Invalid UTF-8 -> we're done here. */
        *utf8 = (uint8_t)curr; /* Copy first byte. */
        bool is_err = false;
        for (uint32_t i = 1; i < u8len; ++i) { /* Copy UTF-8 sequence. */
            tmp = fgetc(in);
            if (is_done(tmp)) { is_err = true; }
            utf8[i] = (uint8_t)tmp;
        }
        if (is_err) { break; }
        size_t pos;
        neo_unicode_error_t err = neo_utf8_validate(utf8, u8len, &pos);
        if (neo_likely(err == NEO_UNIERR_OK)) { /* Valid UTF-8 -> append to buffer. */
            while (len+u8len >= cap) { /* Resize buffer if necessary. */
                buf = neo_memalloc(buf, (cap<<=1)*sizeof(*buf));
            }
            memcpy(buf+len, utf8, u8len);
            len += u8len;
        } else {
            fprintf(out, "Invalid UTF-8 sequence at position %zu: ", pos);
            for (uint32_t i = 0; i < u8len; ++i) {
                printf("%02x ", utf8[i]);
            }
            fprintf(out, "\n");
            break;
        }
        if (curr == '\n') {
            if (prev == '\n') { /* Double newline -> we're done here. */
                --len;
                break;
            }
            prompt = true;
        }
        prev = curr;
    }
    buf[len] = '\0';
    *plen = len;
    return buf;
}

static void interactive_shell_input_loop(neo_compiler_t *compiler) {
    neo_dassert(compiler != NULL);
    for (;;) {
        size_t len = 0;
        const uint8_t *input = read_source_from_shell(&len);
        if (neo_unlikely(!len || !input)) {
            neo_memalloc((void *)input, 0); /* Free input buffer. */
            continue;
        }
        /* Search for command. */
        for (size_t i = 0; i < sizeof(shell_commands)/sizeof(*shell_commands); ++i) {
            size_t klen = strlen(shell_commands[i].kw);
            if (len < klen) { continue; } /* Length mismatch -> skip. */
            if (memcmp(input, shell_commands[i].kw, klen) == 0) { /* Match -> execute command. */
                shell_commands[i].cmd(input);
                neo_memalloc((void *)input, 0); /* Free input buffer. */
                return;
            }
        }
        source_load_error_info_t info = {0};
        const source_t *src = source_from_memory_ref((const uint8_t *)"stdin", input, &info); /* Create source object. */
        if (neo_unlikely(!src)) {
            printf("Failed to load source"); /* This should never happen because we validate pointers and UTF-8 before anyway. */
            neo_memalloc((void* )input, 0); /* Free input buffer. */
            continue;
        }
        compiler_compile(compiler, src, NULL);
        neo_memalloc((void *)input, 0); /* Free input buffer. */
        source_free(src); /* Free source object. */
    }
}

static void interactive_shell(void) {
    show_help(NULL);

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
    } else if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        printf("Usages:\n");
        printf("\tneo\n");
        printf("\tneo [filename]\n");
        printf("\tneo -h | --help\n");
        printf("\tneo --version\n");
        printf("\tneo --license\n");
    } else if (neo_likely(argc == 2)) {
        const uint8_t *filename = (const uint8_t *)argv[1];
        source_load_error_info_t info = {0};
        const source_t *src = source_from_file(filename, &info);
        if (neo_unlikely(!src)) {
            printf("Failed to load source: %s\n", argv[1]);
            neo_osi_shutdown();
            return EXIT_FAILURE;
        }
        neo_compiler_t *compiler = NULL;
        compiler_init(&compiler, COM_FLAG_NONE);
        compiler_compile(compiler, src, NULL);
        compiler_free(&compiler);
        source_free(src);
    }
    neo_osi_shutdown();
    return EXIT_SUCCESS;
}
