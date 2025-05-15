#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>

static int sem_pipe[2];

void sem_init(int k) {
    pipe(sem_pipe);
    for (int i = 0; i < k; i++) {
        write(sem_pipe[1], "x", 1);
    }
}

void acquire() {
    char c;
    read(sem_pipe[0], &c, 1);
}

void release() {
    write(sem_pipe[1], "x", 1);
}

void dfs() {
    DIR *dir = opendir(".");

    struct dirent *entry;
    char **subdirs = NULL;
    int nd = 0;
    char **progs = NULL;
    int np = 0;

    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        if (!strcmp(name, ".") || !strcmp(name, "..")) {
            continue;
        }
        
        struct stat st;
        lstat(name, &st);
        if (S_ISDIR(st.st_mode)) {
            subdirs = realloc(subdirs, sizeof(char*) * (nd + 1));
            subdirs[nd++] = strdup(name);
        } else if (S_ISREG(st.st_mode) && (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
            progs = realloc(progs, sizeof(char*) * (np + 1));
            progs[np++] = strdup(name);
        }
    }
    closedir(dir);

    pid_t *sub_pids = calloc(nd, sizeof(pid_t));
    for (int i = 0; i < nd; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            chdir(subdirs[i]);
            dfs();
            exit(0);
        }

        sub_pids[i] = pid;
        free(subdirs[i]);
    }
    free(subdirs);

    pid_t *watch_pids = calloc(np, sizeof(pid_t));
    for (int i = 0; i < np; i++) {
        acquire();
        pid_t pid = fork();
        if (pid == 0) {
            pid_t child = fork();
            if (child == 0) {
                char path[PATH_MAX];
                snprintf(path, sizeof(path), "./%s", progs[i]);
                execl(path, progs[i], (char*) NULL);
            }

            int status;
            waitpid(child, &status, 0);
            release();
            exit(0);
        }

        watch_pids[i] = pid;
        free(progs[i]);
    }
    free(progs);

    for (int i = 0; i < nd; i++) {
        waitpid(sub_pids[i], NULL, 0);
    }
    free(sub_pids);

    for (int i = 0; i < np; i++) {
        waitpid(watch_pids[i], NULL, 0);
    }
    free(watch_pids);
}

int main(int argc, char **argv) {
    int k = atoi(argv[1]);
    sem_init(k);
    dfs();
    return 0;
}
