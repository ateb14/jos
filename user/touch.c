#include <inc/lib.h>

void
umain(int argc, char **argv){
    if(argc <= 1){
        cprintf("usage: touch <name>\n");
        return;
    }
    int fd = open(argv[1], O_WRONLY | O_CREAT);
    if(fd < 0){
        cprintf("touch failed: %e\n", fd);
    }
}