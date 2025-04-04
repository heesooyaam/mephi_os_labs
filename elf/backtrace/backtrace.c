#include "backtrace.h"

#include <elf.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

static char func_name[256];
char* AddrToName(void* addr) {
    int fd = open("/proc/self/exe", O_RDONLY);
    
    struct stat st;
    fstat(fd, &st);

    void *self = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    Elf64_Ehdr *elf_header = self;

    Elf64_Shdr *section_headers = (self + elf_header->e_shoff);

    bool found = false;
    for (int i = 0; i < elf_header->e_shnum && !found; ++i) {
        Elf64_Shdr *cur_header = section_headers + i;
        if (cur_header->sh_type != SHT_SYMTAB && cur_header->sh_type != SHT_DYNSYM) {
            continue;
        }

        const size_t cnt_symbols = cur_header->sh_size / cur_header->sh_entsize;
        Elf64_Sym *sym_table = self + cur_header->sh_offset;
        char *name_table = self + (section_headers + cur_header->sh_link)->sh_offset;

        for (int j = 0; j < cnt_symbols && !found; ++j) {
            if (!(
                    sym_table[j].st_value <= (uintptr_t) addr
                    && (uintptr_t) addr < sym_table[j].st_value + sym_table[j].st_size
                )) {
                continue;
            }
            found = true;

            strcpy(func_name, name_table + sym_table[j].st_name);
        }
    }

    munmap(self, st.st_size);
    return (found ? func_name : NULL);
}

int Backtrace(void* backtrace[], int limit) {
    void** rbp = __builtin_frame_address(0);
    int last = -1;

    while (last < limit) {
        backtrace[++last] = *(rbp + 1);
        
        char* name = AddrToName(backtrace[last]);
        if (name && !strcmp(name, "main")) {
            break;
        }

        rbp = (void**) *rbp;
    }

    return last + 1;
}

void PrintBt() {
    #define MAX_BT 10
    void* backtrace[MAX_BT];
    const size_t n = Backtrace(backtrace, MAX_BT);
    for (int i = 0; i < n; i++) {
        printf("0x%lx %s\n", (uintptr_t) backtrace[i], AddrToName(backtrace[i]));
    }
}
