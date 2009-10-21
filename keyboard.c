#include<signal.h>
#include<stdio.h>
#include<stdlib.h>

void die() {
  printf("KBD: Received SIGINT, quitting..\n");
  exit(1);
}
void register_handlers() {
  sigset(SIGINT, die);
}
int parent_pid, fid, mem_size;

int main(int argc, char** argv) {
  register_handlers();
  sscanf(argv[0], "%d", &parent_pid);
  sscanf(argv[1], "%d", &fid);
  sscanf(argv[2], "%d", &mem_size);
  printf("KBD: %d %d %d\n", parent_pid, mem_size, fid);
  while(1) { }
return 0;
}

