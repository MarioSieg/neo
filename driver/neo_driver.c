/* (c) Copyright Mario "Neo" Sieg <mario.sieg.64@gmail.com> 2023. All rights reserved. */

#include <neo_core.h>
#include <neo_compiler.h>

static void show_help(const char *cmd);
static void show_version(const char *cmd);
static void show_license(const char *cmd);

typedef struct command_t {
    const char *cmd_long;
    const char *cmd_short;
    void (*cmd)(const char *cmd);
    const char *desc;
} command_t;

const command_t shell_commands[] = {
    {"--help", "-h", &show_help, "Shows this help."},
    {"--version", "-v", &show_version, "Shows the version of Neo."},
    {"--license", "-l", &show_license, "Shows the license of Neo."},
};

static void show_help(const char *cmd) {
    (void)cmd;
    printf("(c) Copyright Mario \"Neo\" Sieg <mario.sieg.64@gmail.com> 2023\n");
    printf("Available commands:\n");
    for (size_t i = 0; i < sizeof(shell_commands) / sizeof(*shell_commands); ++i) {
        printf("  %s, %s: %s\n", shell_commands[i].cmd_long, shell_commands[i].cmd_short, shell_commands[i].desc);
    }
}

static void show_version(const char *cmd) {
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

static void show_license(const char *cmd) {
    (void)cmd;
    puts(neo_blobs_license);
}

static void load_and_execute_neo_source(const char *file) {
    neo_assert(file != NULL && "File must not be NULL.");
    neo_osi_init(); /* Must be called before any other neo_* function. */
    const uint8_t *filename = (const uint8_t *)file;
    source_load_error_info_t info = {0};
    const source_t *src = source_from_file(filename, &info);
    if (neo_unlikely(!src)) {
        printf("Failed to load source: %s\n", file);
        neo_osi_shutdown();
        return;
    }
    neo_compiler_t *compiler = NULL;
    compiler_init(&compiler, COM_FLAG_NONE);
    compiler_compile(compiler, src, NULL);
    compiler_free(&compiler);
    source_free(src);
    neo_osi_shutdown();
}

int main(int argc, const char **argv) {
    if (argc >= 2) { /* If arguments are given, check if it is a shell command. */
        for (size_t i = 0; i < sizeof(shell_commands) / sizeof(*shell_commands); ++i) { /* Check if command is a shell command. */
            if (strcmp(argv[1], shell_commands[i].cmd_long) == 0 || strcmp(argv[1], shell_commands[i].cmd_short) == 0) {
                (*shell_commands[i].cmd)(argv[1]);
                return EXIT_SUCCESS;
            }
        }
        load_and_execute_neo_source(argv[1]); /* If not, try to load and execute it as a source file. */
    } else { /* If no arguments are given, start the interactive shell. */
        show_help(argv[0]);
    }

    return EXIT_SUCCESS;
}
