#ifndef UFFD_H
#define UFFD_H

#ifndef _GNU_SOURCE 
#define _GNU_SOURCE
#endif

#include <linux/userfaultfd.h>
#include <sys/types.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <stdint.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

#ifndef POLLRDHUP
#define POLLRDHUP 0x2000
#endif
uint64_t page_size = 16*1024;

struct handler_struct {
  int uffd;                                                 // userfault fd
  int csfd;                                                 // client socket fd
  int filefd;
  char *base_addr; 
  pthread_mutex_t running;
  pthread_t thread;
};

int memfd_create(const char *name, unsigned int flags);
void init_handler_struct(struct handler_struct* hs);
void * fault_handler_thread(void *arg); 
long init_uffd();
void register_uffd(long uffd, char* addr, unsigned long len); 
pthread_t uf_server(int csfd, int memfd, uint64_t map_len, int filefd); 
void uf_client(int sock, void** addr, uint64_t* sz); 
#endif
