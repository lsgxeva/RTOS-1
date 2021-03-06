/*  Copyright 2009 Syed S. Albiz
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
#include<curses.h>
#include<sys/mman.h>
#include "global.h"
#include<string.h>

int parent_pid, fid, mem_size;
caddr_t mem_ptr;
FILE *tr_out;
void die() {
  munmap(mem_ptr, mem_size);
  endwin();			/* End curses mode		  */
#ifdef DEBUG
  fclose(tr_out);
  printf("CRT: Received SIGINT, quitting..\n");
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
  int x, y, row, col;
  char *tmp;
#ifdef DEBUG
  tr_out = fopen("trace.out", "w");
#endif
  register_handlers();
  sscanf(argv[0], "%d", &parent_pid);
  sscanf(argv[1], "%d", &fid);
  sscanf(argv[2], "%d", &mem_size);
  unmask();
  initscr(); 			/* Start curses mode 		  */
  noecho();
  cbreak();
  scrollok(stdscr, TRUE);
  idlok(stdscr, TRUE);
  getmaxyx(stdscr,row,col);
#ifdef DEBUG
  printw("CRT: %d %d %d\n", parent_pid, mem_size, fid);
#endif
  refresh();			/* Print it on to the real screen */

  mem_ptr = mmap((caddr_t)0, mem_size, PROT_READ|PROT_WRITE,
  		 MAP_SHARED, fid, (off_t)0);
  mem_buffer *buffer = (mem_buffer*) mem_ptr;
  char local_buffer[MEMBLOCK_SIZE];
  local_buffer[0] = '\0';
  unmask();
  while(1) {
    while (buffer->flag != MEM_DONE) {
    }
    strncpy(local_buffer, buffer->data, mem_size); //RTX in charge or null termination
    tmp = strtok(local_buffer, "\n");
    buffer->length = 0;
    buffer->flag = MEM_READY;
    getyx(stdscr, y, x);
    while (tmp != NULL) {
      if (strlen(tmp) > 0) {
        if (strstr(tmp, "STOPCLOCK") != NULL) {
	  mvprintw(0, col-20, "                    ");
          move(y, 0);
        } else if (strstr(tmp, "CLOCK") != NULL) {
          mvprintw(0, col-strlen(tmp)-1, "%s\n\r", tmp);
          move(y, 0);
        } else {
          printw("CCI:%s\n\r", tmp);
#ifdef DEBUG
          fprintf(tr_out, "$:%s\n\r", tmp);
#endif
          //move(y, 0);
        }
        refresh();
      }
      tmp = strtok(NULL, "\n");
    }

    kill(parent_pid, SIGUSR2);
  }
  return 0; 
}
