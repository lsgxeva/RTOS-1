#include "kernel.h"


void K_terminate() {
  K_cleanup();
  exit(0);
}

void K_context_switch(jmp_buf prev, jmp_buf next) {
  if (setjmp(prev) == 0) {
    longjmp(next, 1);
  } else {
    return;
  }
}

void K_process_switch() { 
//assumptions: 
//-called by currently executing process (PCB == current),
//-calling process has already enqueued itself into a process queue to re-enter execution at some point
  PCB* next = ppq_dequeue(_rpq);
  PCB* tmp = current_process;
  assert(tmp != NULL);
  next->state = EXECUTING;
  current_process = next; //non-atomic. Will reset?
  K_context_switch(tmp->context, next->context);
}

void K_release_processor() {
  PCB* current = current_process;
  current->state = READY;
  ppq_enqueue(current, _rpq);
  K_process_switch();
}

void null_process() {
  while(1) {
    K_release_processor(); //trusted kernel process
  }
}


PCB* pid_to_PCB(int target) {
  //re-implement with pid array/hashtable. O(n) lookup is terrible
  PCB* next = pq_peek(_process_list);
  while (next != NULL && next->pid != target)
	next  = next->p_next;
  if (next->pid == target) return next;
  else return NULL;
}

int K_request_process_status(MessageEnvelope* msg) {
  PCB* next = pq_peek(_process_list);
  char tmp_buf[20];
  strcpy(msg->data, "");
  while (next != NULL) {
    sprintf(tmp_buf, "%d,%d,%d\n", next->pid, 
    				   next->state, 
				   next->priority);
    strcat(msg->data, tmp_buf);
    next = next->p_next;
  }
  
  return 0;  
}

int K_change_priority(int new_priority, int target_pid) {
  PCB* target = pid_to_PCB(target_pid);
  if (target == current_process) {
    target->priority = new_priority;
    return 0;
  } else {
    switch(target->state) {
      case READY: ppq_remove(target, _rpq); ppq_enqueue(target, _rpq); break;
      case MESSAGE_WAIT: ppq_remove(target, _mwq); ppq_enqueue(target, _mwq); break;
      case ENVELOPE_WAIT: ppq_remove(target, _ewq); ppq_enqueue(target, _ewq); break;
      default: break;
    }
  }
  return 0;
}


MessageEnvelope* K_request_message_envelope(void) {
  MessageEnvelope *env = NULL;
  while (mq_is_empty(_feq)) {
    current_process->state = ENVELOPE_WAIT;
    ppq_enqueue(current_process, _ewq);
    K_process_switch();
  }
  env = mq_dequeue(_feq);
  strcpy(env->data, "");
  mq_enqueue(env, current_process->message_send);
  return env;
}

void K_release_message_envelope(MessageEnvelope* env) {
  strcpy(env->data, "");
  mq_remove(env, current_process->message_send);
  mq_remove(env, current_process->message_receive);
  mq_enqueue(env, _feq);
  if (!ppq_is_empty(_ewq)) {
    PCB* nextPCB = ppq_dequeue(_ewq);
    nextPCB->state = READY;
    ppq_enqueue(nextPCB, _rpq);
  }
}


void K_register_trace(MessageEnvelope* env, int type) {
  assert(_tq != NULL && env != NULL);
  msg_event* new_evt       = (msg_event*) malloc(sizeof(msg_event));
  new_evt->destination_pid = env->destination_pid;
  new_evt->source_pid      = env->sender_pid;
  new_evt->mtype           = env->type;
  new_evt->timestamp       = ticks / TIMER_INTERVAL;
  new_evt->type            = type;
  new_evt->next            = NULL;
  trace_enqueue(new_evt, _tq);
}

void K_send_message(int dest_pid, MessageEnvelope* env) {
  env->sender_pid = current_process->pid;
  env->destination_pid = dest_pid;
  //assume message text and type are handled elsewhere
  PCB* target = pid_to_PCB(dest_pid);
  mq_remove(env, current_process->message_send);
  mq_enqueue(env, target->message_receive);
  K_register_trace(env, 0);
  //do trace
  if (target->state == MESSAGE_WAIT) {
    target->state = READY;
    ppq_remove(target, _mwq);
    ppq_enqueue(target, _mwq);
  }
}

MessageEnvelope* K_receive_message(void) {
  PCB* current = current_process;
  MessageEnvelope* env = NULL;
  assert(current != NULL);
  while (mq_is_empty(current->message_receive)) {
    if (current == timer_i_process || 
 	current == keyboard_i_process ||
	current == crt_i_process) return NULL;
    //if no message waiting and not an i_process, block on receive
    current->state = MESSAGE_WAIT;
    ppq_enqueue(current, _mwq);
    K_process_switch();
  }
  env = mq_dequeue(current_process->message_receive);
  //still want the envelope associated with this process, so put it on the send queue
  mq_enqueue(env, current_process->message_send);
  //do trace
  K_register_trace(env, 1);
  return env;
}

int K_get_trace_buffer(MessageEnvelope* env) {
  assert(env != NULL);
  msg_event* evts = _tq->send;
  int len = MESSAGE_SIZE/32;
  char tmp_buf[len];
  strcpy(env->data, "");
  while(evts != NULL) {
    sprintf(tmp_buf, "SENT:\tTO: %d,\tFR: %d,\tTIME: %d,\tTYPE: %d\n", 
    		evts->destination_pid, 
		evts->source_pid,
		evts->timestamp,
		evts->mtype);
  
    strcat(env->data, tmp_buf);
    evts = evts->next;
  }
  evts = _tq->receive;
  while(evts != NULL) {
    sprintf(tmp_buf, "RCVD:\tFR: %d,\tTO: %d,\tTIME: %d,\tTYPE: %d\n", 
    		evts->source_pid, 
		evts->destination_pid,
		evts->timestamp,
		evts->mtype);
    strcat(env->data, tmp_buf);
    evts = evts->next;
  }
  free(evts);
  return 0;
}


    
void K_cleanup() {
  printf("RTX: sending signal\n");
  kill(_kbd_pid, SIGINT);
  kill(_crt_pid, SIGINT);
  
  int stat = 0;
  if (_kbd_mem_ptr != NULL) {
    printf("RTX: unmapping keyboard share\n");
    stat = munmap(_kbd_mem_ptr, MEMBLOCK_SIZE);
    if (DEBUG && stat == -1) {printf("RTX: Error unmapping keyboard share\n");} else {printf("RTX: SUCCESS\n");}
    stat = close(_kbd_fid);
    if (DEBUG && stat == -1) {printf("RTX: Error unmapping keyboard share\n");} else {printf("RTX: SUCCESS\n");}
    stat = unlink(KEYBOARD_FILE);
    if (DEBUG && stat == -1) {printf("RTX: Error unmapping keyboard share\n");} else {printf("RTX: SUCCESS\n");}

  }
  if (_crt_mem_ptr != NULL) {
    printf("RTX: unmapping crt share\n");
    stat = munmap(_crt_mem_ptr, MEMBLOCK_SIZE);
    if (DEBUG && stat == -1) {printf("RTX: Error unmapping crt share\n");} else {printf("RTX: SUCCESS\n");}
    stat = close(_crt_fid);
    if (DEBUG && stat == -1) {printf("RTX: Error unmapping crt share\n");} else {printf("RTX: SUCCESS\n");}
    stat = unlink(CRT_FILE);
    if (DEBUG && stat == -1) {printf("RTX: Error unmapping crt share\n");} else {printf("RTX: SUCCESS\n");}
  }

  if (!pq_is_empty(_process_list)) { 
    pq_free(&_process_list);
    printf("RTX: deallocating global process list\n");
  }
  //since PCBs have all been freed, no need to free more
  ppq_free(_rpq);
  ppq_free(_ewq);
  ppq_free(_mwq);
  trace_free(&_tq);
  if (!mq_is_empty(_feq)) {
    printf("RTX: deallocating envelope list\n");
    mq_free(_feq);
  }

}
