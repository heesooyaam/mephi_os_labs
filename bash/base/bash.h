#include "tokenizer.h"

#include <unistd.h>
#include <sys/wait.h>

char** GetArgv(struct Tokenizer* tokenizer) {
    const size_t argc = tokenizer->token_count;

    char** argv = calloc(argc + 1, sizeof(char*));
    argv[argc] = NULL;

    struct Token* token = tokenizer->head;

    for (size_t i = 0; i < argc; ++i) {
        argv[i] = calloc(token->len + 1, sizeof(char));
        memcpy(argv[i], token->start, token->len);
        argv[i][token->len] = '\0';

        token = token->next;
    }

    return argv;
}

void Exec(struct Tokenizer* tokenizer) {
    if (!(tokenizer->token_count)) {
        return;
    }

    pid_t pid = fork();

    if (!pid) {
        char** argv = GetArgv(tokenizer);

        if (execvp(argv[0], argv)) {
            printf("Command not found\n");
        }

        for (size_t i = 0; i < tokenizer->token_count; ++i) {
            free(argv[i]);
        }
        free(argv);
    } else {
        int status;
        waitpid(pid, &status, 0);
    }
}
