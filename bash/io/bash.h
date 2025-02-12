#include "tokenizer.h"

#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>

struct FdManager {
    int used_std;
    int saved_std;
    int opened_fd;
};

struct ErrorsManager {
    bool io_error;
    bool syntax_error;
};

const char SYNTAX_ERROR[] = "Syntax error\n";
const char IO_ERROR[] = "I/O error\n";
const char COMMAND_NF[] = "Command not found\n";

void Dup(struct FdManager* fd_manager, bool* io_error, const char* path, unsigned flags, unsigned mode) {
    if (flags & O_CREAT) {
        fd_manager->opened_fd = open(path, flags, mode);
    } else {
        fd_manager->opened_fd = open(path, flags);
    }

    if (fd_manager->opened_fd == -1) {
        *io_error = true;
    } else {
        fd_manager->saved_std = dup(fd_manager->used_std);
        dup2(fd_manager->opened_fd, fd_manager->used_std);
    }
}

void ResetStdIO(const struct FdManager* fd_manager) {
    if (fd_manager->saved_std != -1) {
        dup2(fd_manager->saved_std, fd_manager->used_std);
        close(fd_manager->saved_std);
    }

    if (fd_manager->opened_fd != -1) {
        close(fd_manager->opened_fd);
    }
}

void CheckIOSyntax(const struct Token* token, bool* syntax_error, bool* token_changed) {
    if (*token_changed || !token->next || token->next->type != TT_WORD) {
        *syntax_error = true;
    }
    
    *token_changed = true;
}

void CheckSyntax(const struct Tokenizer* tokenizer, bool* syntax_error) {
    struct Token* token = tokenizer->head;

    bool out_changed = false;
    bool in_changed = false;

    while (token) {
        switch (token->type) {
            case TT_INFILE: {
                CheckIOSyntax(token, syntax_error, &in_changed);
                break;
            } case TT_OUTFILE: {
                CheckIOSyntax(token, syntax_error, &out_changed);
                break;
            } default: {
                break;
            }
        }

        token = token->next;
    }
}

char* PreparePath(const char* start, const size_t len) {
    char* path = strndup(start, len + 1);
    path[len] = '\0';

    return path;
}

size_t CheckIOAndOpenFiles(const struct Tokenizer* tokenizer, struct FdManager* in_manager, struct FdManager* out_manager, bool* io_error) {
    size_t argc = 0;
    struct Token* token = tokenizer->head;

    char* in_path = NULL;
    char* out_path = NULL;

    while (token) {
        switch (token->type) {
            case TT_INFILE: {
                in_path = PreparePath(token->next->start, token->next->len);
                Dup(in_manager, io_error, in_path, O_RDONLY, 0);

                token = token->next;

                break;
            }
            case TT_OUTFILE: {
                out_path = PreparePath(token->next->start, token->next->len);
                Dup(out_manager, io_error, out_path, O_WRONLY | O_CREAT, 0644);

                token = token->next->next;

                break;
            }
            default: {
                token = token->next;
                ++argc;
                break;
            }
        }
    }

    if (*io_error) {
        if (!in_path) {
            unlink(in_path);
        }

        if (!out_path) {
            unlink(out_path);
        }
    }

    free(in_path);
    free(out_path);

    return argc;
}

char** GetArgv(const struct Tokenizer* tokenizer, const size_t argc) {
    struct Token* token = tokenizer->head;

    char** argv = calloc(argc + 1, sizeof(char*));
    argv[argc] = NULL;

    int idx = 0;
    token = tokenizer->head;
    while (token) {
        if (token->type != TT_WORD) {
            token = token->next->next;
            continue;
        }

        argv[idx] = strndup(token->start, token->len + 1);
        argv[idx++][token->len] = '\0';

        token = token->next;
    }

    return argv;
}

void Exec(const struct Tokenizer* tokenizer) {
    if (!(tokenizer->token_count)) {
        return;
    }

    pid_t pid = fork();

    if (!pid) {
        struct FdManager in_manager = {STDIN_FILENO, -1, -1};
        struct FdManager out_manager = {STDOUT_FILENO, -1, -1};
        bool syntax_error = false, io_error = false;

        CheckSyntax(tokenizer, &syntax_error);
        if (!syntax_error) {
            const size_t argc = CheckIOAndOpenFiles(tokenizer, &in_manager, &out_manager, &io_error);

            char** argv = NULL;
            if (!io_error) {
                argv = GetArgv(tokenizer, argc);
            }

            if (io_error || (argv && execvp(*argv, argv))) {
                ResetStdIO(&in_manager);
                ResetStdIO(&out_manager);

                printf("%s", (io_error) ? IO_ERROR : COMMAND_NF);
            }

            if (argv) {
                for (size_t i = 0; i < tokenizer->token_count; ++i) {
                    free(argv[i]);
                }
                free(argv);
            }
        } else {
            printf("%s", SYNTAX_ERROR);
        }
    } else {
        int status;
        waitpid(pid, &status, 0);
    }
}
