#include "tokenizer.h"

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <unistd.h>


/************************ Local things ************************/

// Structures
struct FdManager {
    int used_std;
    int saved_std;
    int opened_fd;
};

// Constants
const char SYNTAX_ERROR[] = "Syntax error\n";
const char COMMAND_NF[]   = "Command not found\n";
const char IO_ERROR[]     = "I/O error\n";


// Utils
void DupWithOpen(struct FdManager* fd_manager, bool* io_error, const char* path, unsigned flags, unsigned mode) {
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

void Dup(struct FdManager* fd_manager) {
    fd_manager->saved_std = dup(fd_manager->used_std);
    dup2(fd_manager->opened_fd, fd_manager->used_std);
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

char* PreparePath(const char* start, const size_t len) {
    char* path = strndup(start, len + 1);
    path[len] = '\0';

    return path;
}

bool IsCommandEnd(const struct Token* token) {
    return !token || token->type == TT_PIPE;
}

size_t GetArgc(struct Token* command) {
    struct Token* token = command;

    size_t argc = 0;
    while (!IsCommandEnd(token)) {
        switch (token->type) {
            case TT_WORD: {
                token = token->next;
                ++argc;
                break;
            }
            default: {
                token = token->next->next;
                break;
            }
        }
    }

    return argc;
}

char** GetArgv(struct Token* command) {
    const size_t argc = GetArgc(command);

    char** argv = calloc(argc + 1, sizeof(char*));
    argv[argc] = NULL;

    int idx = 0;
    struct Token* token = command;
    while (!IsCommandEnd(token)) {
        switch (token->type) {
            case TT_WORD: {
                argv[idx] = strndup(token->start, token->len + 1);
                argv[idx++][token->len] = '\0';
                token = token->next;
                break;
            }
            default: {
                token = token->next->next;
                break;
            }
        }
    }

    return argv;
}

struct Token** SplitInCommands(const struct Tokenizer* tokenizer, size_t* commands_cnt, bool* syntax_error) {
    struct Token* token = tokenizer->head;

    *commands_cnt = 1;
    while (token) {
        switch (token->type) {
        case TT_PIPE: {
            ++(*commands_cnt);
            break;
        }
        default:
            break;
        }

        token = token->next;
    }

    struct Token** commands = calloc(*commands_cnt, sizeof(struct Token*));

    if (tokenizer->head->type == TT_PIPE) {
        *syntax_error = true;
        free(commands);
        return NULL;
    }

    commands[0] = tokenizer->head;
    int idx = 0;
    token = tokenizer->head;
    while (token) {
        switch (token->type) {
        case TT_PIPE: {
            if (!token->next || token->next->type == TT_PIPE) {
                *syntax_error = true;
                free(commands);
                return NULL;
            }
            commands[++idx] = token->next;
            break;
        }
        default:
            break;
        }

        token = token->next;
    }

    return commands;
}

// Validators
void ValidateIOSyntax(const struct Token* token, bool* syntax_error, bool* token_changed, bool has_permission_to_change_io) {
    if (!has_permission_to_change_io || *token_changed || !token->next || token->next->type != TT_WORD) {
        *syntax_error = true;
    }

    *token_changed = true;
}

void ValidateCommandSyntax(struct Token* command, bool* syntax_error, bool first_command, bool last_command) {
    struct Token* token = command;

    bool out_changed = false;
    bool in_changed = false;

    while (!IsCommandEnd(token) && !*syntax_error) {
        switch (token->type) {
            case TT_INFILE: {
                ValidateIOSyntax(token, syntax_error, &in_changed, first_command);
                break;
            } case TT_OUTFILE: {
                ValidateIOSyntax(token, syntax_error, &out_changed, last_command);
                break;
            } default: {
                break;
            }
        }

        token = token->next;
    }
}

void ValidateIOAndOpenFile(struct Token* command, struct FdManager* fd_manager, bool* io_error, bool first_command, bool last_command) {
    if (*io_error) {
        return;
    }

    struct Token* token = command;
    char* path = NULL;

    while (!IsCommandEnd(token)) {
        switch (token->type) {
            case TT_INFILE: {
                if (first_command) {
                    path = PreparePath(token->next->start, token->next->len);
                    DupWithOpen(fd_manager, io_error, path, O_RDONLY, 0);
                }

                token = token->next->next;

                break;
            }
            case TT_OUTFILE: {
                if (last_command) {
                    path = PreparePath(token->next->start, token->next->len);
                    DupWithOpen(fd_manager, io_error, path, O_WRONLY | O_CREAT, 0644);
                }

                token = token->next->next;

                break;
            }
            default: {
                token = token->next;
                break;
            }
        }
    }

    if (*io_error) {
        if (!path) {
            unlink(path);
        }
    }

    free(path);
}

/**************************************************************/

void Exec(const struct Tokenizer* tokenizer) {
    if (!tokenizer->token_count) {
        return;
    }


    bool io_error = false;
    bool syntax_error = false;

    size_t commands_cnt = 0;
    struct Token** commands = SplitInCommands(tokenizer, &commands_cnt, &syntax_error);

    if (syntax_error) {
        printf("%s", SYNTAX_ERROR);
        return;
    }

    for (size_t i = 0; i < commands_cnt && !syntax_error; ++i) {
        assert(commands[i]);
        ValidateCommandSyntax(commands[i], &syntax_error, i == 0, i == commands_cnt - 1);
    }

    if (syntax_error) {
        printf("%s", SYNTAX_ERROR);
        free(commands);
        return;
    }

    assert(commands[0]);
    assert(commands[commands_cnt - 1]);

    int in_fd = STDIN_FILENO;
    for (int i = 0; i < commands_cnt; ++i) {
        int pipefd[2];

        if (pipe(pipefd) == -1) {
            perror("cannot do pipe");
            free(commands);
            return;
        }

        int pid = fork();

        if (!pid) {
            struct FdManager in_manager = {STDIN_FILENO, -1, in_fd};
            struct FdManager out_manager = {STDOUT_FILENO, -1, pipefd[1]};

            if (i == 0) { // если первая команда, пробуем читать из файла
                ValidateIOAndOpenFile(commands[0], &in_manager, &io_error, true, false);
                if (io_error) {
                    ResetStdIO(&in_manager);
                    ResetStdIO(&out_manager);
                    free(commands);

                    printf("%s", IO_ERROR);
                    return;
                }
            } else { // если не первая, то stdin переводим на вывод прошлой команды
                Dup(&in_manager);
            }

            if (i == commands_cnt - 1) { // если последняя команда, пробуем писать в файл
                ValidateIOAndOpenFile(commands[commands_cnt - 1], &out_manager, &io_error, false, true);
                if (io_error) {
                    ResetStdIO(&in_manager);
                    ResetStdIO(&out_manager);
                    free(commands);

                    printf("%s", IO_ERROR);
                    return;
                }
            } else {
                Dup(&out_manager);
            }

            char** argv = GetArgv(commands[i]);
            if (execvp(*argv, argv)) {
                ResetStdIO(&in_manager);
                ResetStdIO(&out_manager);

                for (size_t j = 0, argc = GetArgc(commands[i]); j < argc + 1; ++j) {
                    free(argv[j]);
                }
                free(argv);
                free(commands);

                printf("%s", COMMAND_NF);
                return;
            }
        } else {
            close(pipefd[1]);
            in_fd = pipefd[0];
        }
    }

    while (wait(NULL) > 0) {}
    free(commands);
}
