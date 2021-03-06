/* Copyright 2009 Syed S. Albiz
 * This file is part of myRTX.
 *
 * myRTX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * myRTX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with myRTX.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include<signal.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/mman.h>
#include "global.h"

int parent_pid, fid, mem_size;
caddr_t mem_ptr;


void die() {
  kill(parent_pid, SIGINT);
  munmap(mem_ptr, mem_size);
#ifdef DEBUG
  printf("KBD: Received SIGINT, quitting..\n");
#endif
  exit(1);
}

int register_handler(int signal) {
    struct sigaction sa;
    sa.sa_handler = die;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGUSR1);
    sigaddset(&sa.sa_mask, SIGUSR2);
    sigaddset(&sa.sa_mask, SIGALRM);
    sa.sa_flags = SA_RESTART; 
    if (sigaction(signal, &sa, NULL) == -1) 
      return -1;
    else 
      return 0;
}

void unmask() {
  sigset_t newmask;
  sigemptyset(&newmask);
  sigaddset(&newmask, SIGINT);
  sigaddset(&newmask, SIGUSR1);
  sigaddset(&newmask, SIGUSR2);
  sigaddset(&newmask, SIGALRM);
  sigprocmask(SIG_UNBLOCK, &newmask, NULL);
}

void register_handlers() {
  register_handler(SIGINT);
}


int main(int argc, char** argv) {
  register_handlers();
  sscanf(argv[0], "%d", &parent_pid);
  sscanf(argv[1], "%d", &fid);
  sscanf(argv[2], "%d", &mem_size);
#ifdef DEBUG
  printf("KBD: %d %d %d\n", parent_pid, mem_size, fid);
#endif
  mem_ptr = mmap((caddr_t)0, mem_size, PROT_READ|PROT_WRITE,
  		 MAP_SHARED, fid, (off_t)0);
  mem_buffer *buffer = (mem_buffer*) mem_ptr;
  char local_buffer[MEMBLOCK_SIZE];
  int index = 0;
  char kbd_in = '\0';
  unmask();
  while(1) { 
    kbd_in = getchar();
    putc(kbd_in, stdout);
    //echo back
    if (kbd_in != '\r' && index < MEMBLOCK_SIZE-1) {
      local_buffer[index++] = kbd_in;
    } else {
      local_buffer[index++] = '\0';
      while (buffer->flag != MEM_DONE)
        sleep(1); //1-sec polling
      strcpy(buffer->data, local_buffer);
      buffer->flag = MEM_READY;
      buffer->length = index;
      index = 0;
      kill(parent_pid, SIGUSR1);
    }
      
  }
return 0;
}

