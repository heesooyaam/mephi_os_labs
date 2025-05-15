#define _GNU_SOURCE
#define __GLIBC_USE

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <asm-generic/errno-base.h>
#define CUR_DIR "."

const int BUFF_SIZE = 4096;
int NEED_ROTATE = 0;

void handler(int) {
    NEED_ROTATE = 1;
}

void rotate(const char* filename) {
    static int max_idx = -1;
    ++max_idx;

    char *nxt = NULL, *cur = NULL;
    asprintf(&nxt, "%s/%s.%d", CUR_DIR, filename, max_idx + 1);

    for (int i = max_idx; i; --i) {
        asprintf(&cur, "%s/%s.%d", CUR_DIR, filename, i);
        rename(cur, nxt);

        free(nxt);
        nxt = cur;
        cur = NULL;
    }

    asprintf(&cur, "%s/%s", CUR_DIR, filename);
    rename(cur, nxt);
    free(cur);
    free(nxt);
}

int main(int argc, char** argv) {
    const char* fname = argv[1];

    signal(SIGHUP, handler);
    FILE *log = fopen(fname, "a");

    char buff[BUFF_SIZE];
    for (;;) {
        if (!fgets(buff, sizeof(buff) - 1, stdin)) {
            if (errno == EINTR) {
                continue;
            } else if (feof(stdin)){
                break;
            } else {
                return 1;
            }
        }

        if (NEED_ROTATE) {
            rotate(fname);

            fclose(log);
            log = fopen(fname, "a");

            NEED_ROTATE = 0;
        }

        fputs(buff, log);
    }

    fclose(log);

    return 0;
}