#include "uffd.h"
#include "socket.h"

char rbuf[4096];

#ifndef SYS_memfd_create
#define SYS_memfd_create 319
#endif
int memfd_create(const char *name, unsigned int flags) {
  return syscall(SYS_memfd_create, name, flags);
}

void* uf_servermon(void *arg) {
  struct handler_struct mt;
  memcpy(&mt, arg, sizeof(struct handler_struct));
  pthread_mutex_unlock(&(((struct handler_struct*)arg)->running));
  pthread_join(mt.thread, 0);
  close(mt.csfd);
//   close(mt.uffd);
  printf("s: handler exit\n");
}

void init_handler_struct(struct handler_struct* hs) {
  pthread_mutex_init(&hs->running, 0);
  pthread_mutex_lock(&hs->running);
}

void * fault_handler_thread(void *arg) {
  struct handler_struct* hs = arg;
  static struct uffd_msg msg;   /* Data read from userfaultfd */
  static int fault_cnt = 0;     /* Number of faults so far handled */
  long uffd;                    /* userfaultfd file descriptor */
  long csfd;
  static char *page = NULL;
  struct uffdio_copy uffdio_copy;
  ssize_t nread;
  int filefd;
  unsigned long base_page_addr;

  uffd = hs->uffd;
  csfd = hs->csfd;
  filefd = hs->filefd;
  base_page_addr = (unsigned long)hs->base_addr  & ~(page_size - 1);

  /* Create a page that will be copied into the faulting region */
  if (page == NULL) {
    page = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    if (page == MAP_FAILED)
      errExit("ft-mmap");
  }
  
  printf("\nfault_handler_thread(): init\n");

  pthread_mutex_unlock(&hs->running);
  
  struct pollfd pfd[2] = {
    { .fd = uffd, .events = POLLIN, .revents = 0 },
    { .fd = csfd, .events = POLLIN | POLLRDHUP | POLLPRI, .revents = 0 }
  };
  /* Loop, handling incoming events on the userfaultfd
    file descriptor */

  int flock_fd = open("lock_file",O_RDWR);
  for (;;) {
    /* See what poll() tells us about the userfaultfd */
    int nready;
    nready = poll(pfd, 2, -1);
    if (nready == -1) {
      errExit("poll");
    }
    if (pfd[1].revents != 0) {
      return 0;
    }
    pfd[0].revents = 0;
    pfd[1].revents = 0;

    //printf("\nfault_handler_thread():\n");

    /* Read an event from the userfaultfd */
    nread = read(uffd, &msg, sizeof(msg));
    if (nread == 0) {
      printf("EOF on userfaultfd!\n");
      exit(EXIT_FAILURE);
    }

    if (nread == -1)
      errExit("read");

    /* We expect only one kind of event; verify that assumption */
    if (msg.event != UFFD_EVENT_PAGEFAULT) {
      fprintf(stderr, "Unexpected event on userfaultfd\n");
      exit(EXIT_FAILURE);
    }

    /* Display info about the page-fault event */
    //printf("    UFFD_EVENT_PAGEFAULT event: ");
    //printf("flags = %llx; ", msg.arg.pagefault.flags);
    //printf("address = %llx\n", msg.arg.pagefault.address);

    /* Copy the page pointed to by 'page' into the faulting
      region. Vary the contents that are copied in, so that it
      is more obvious that each fault is handled separately. */
    unsigned long fault_page_addr = (unsigned long) msg.arg.pagefault.address & ~(page_size - 1);
    fault_cnt++;

    if(fault_cnt == 256){
      madvise((void *)(0x600500000000), page_size, MADV_REMOVE ); //Setting up the madvise
    }
    
    if(fault_page_addr == (void *)0x600500000000){
      struct timeval start, end;
      unsigned long long overhead=0;
      printf("Got another fault from  = 0x%llx and removing pages from 0x%llx with len =%lld with fault_cnt=%ld\n",fault_page_addr, base_page_addr,fault_page_addr - base_page_addr, fault_cnt); 
      gettimeofday(&start,NULL);
      unsigned long div = atol(getenv("DIV"));
      unsigned long sz = 1048576/div;
      //printf("div = %ld, sz = %ld\n", div,sz);
      unsigned long i=0;
      for(i=0;i<div;i++){
        if(madvise((void *)base_page_addr + i*sz*page_size, (unsigned long)sz*page_size, MADV_REMOVE)){
          errExit("MADV_REMOVE"); 
        }
      }
      gettimeofday(&end,NULL);
      overhead += ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
      printf("Total overhead = %lld\n", overhead);
      fault_cnt = 0;
    }
    
    lseek(filefd,fault_page_addr - base_page_addr,SEEK_SET);
    read(filefd, page, page_size);
    //memset(page, 'A' + fault_cnt % 20, page_size);

    uffdio_copy.src = (unsigned long) page;

    /* We need to handle page faults in units of pages(!).
       So, round faulting address down to page boundary */

    uffdio_copy.dst = fault_page_addr;
    uffdio_copy.len = page_size;
    uffdio_copy.mode = 0;
    uffdio_copy.copy = 0;

    flock(flock_fd,LOCK_EX);    
    if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy) == -1){
      printf("src=0x%llx, dst=0x%llx, len=%lld, copy=%lld\n",uffdio_copy.src,uffdio_copy.dst,uffdio_copy.len, uffdio_copy.copy);
      perror("ioctl-UFFDIO_COPY");
    }else{
      printf("uffdio_copy successful for 0x%llx\n", uffdio_copy.dst);
    }
    flock(flock_fd,LOCK_UN);
  }
  return 0;
}

long init_uffd() {
  struct uffdio_api uffdio_api;
  long uffd;

  /* Create and enable userfaultfd object */
  uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
  if (uffd == -1)
    errExit("userfaultfd");

  uffdio_api.api = UFFD_API;
  uffdio_api.features = 0;
  if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1)
    errExit("ioctl-UFFDIO_API");

  return uffd;
}
void register_uffd(long uffd, char* addr, unsigned long len) {
  struct uffdio_register uffdio_register;
    /* Register UFFD region */
  uffdio_register.range.start = (unsigned long) addr;
  uffdio_register.range.len = len;
  uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
  if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1)
      perror("ioctl-UFFDIO_REGISTER");
  printf("userfault region = %p\n", addr);
}

pthread_t uf_server(int csfd, int memfd, uint64_t map_len, int filefd) {
  int uffd;
  ssize_t size;
  pthread_t thr, mon;
  char* addr;

  map_len=(map_len & ~(page_size - 1)); 
  printf("page aligned size %lld\n", map_len);
  map_len=map_len + page_size; 

  // handshake: server sends memfd and region size, client mmaps and returns its uffd
  printf("s: send memfd\n");
  size = sock_fd_write(csfd, (char*)&map_len, sizeof(uint64_t), memfd);
  printf("s: recv uffd\n");
  sock_fd_read(csfd, &addr, sizeof(char*), &uffd);
  printf("s: addr: %p uffd: %d map_len=%ld\n", addr,uffd, map_len);
  register_uffd((uint64_t) uffd, addr, map_len);

  struct handler_struct hs;
  init_handler_struct(&hs);
  hs.uffd = uffd;
  hs.csfd = csfd;
  hs.filefd = filefd;
  hs.base_addr = addr;

  pthread_create(&thr, NULL, fault_handler_thread, (void *) &hs);
//   pthread_detach(thr);
  pthread_mutex_lock(&hs.running);
  
  struct handler_struct mt;
  init_handler_struct(&mt);
  mt.uffd = uffd;
  mt.csfd = csfd;
  mt.thread = thr;
  pthread_create(&mon, NULL, uf_servermon, (void *) &mt);
  pthread_detach(mon);
  pthread_mutex_lock(&mt.running);

  // signal client that handler is ready
  write(csfd, "\x00", 1);
  printf("s: init done\n");
  return mon;
}

void uf_client(int sock, void** addr, uint64_t* sz) {
  ssize_t size;
  int i;
  int uffd, memfd;

  uffd = init_uffd();

  // recieve memfd and region size
  sock_fd_read(sock, sz, sizeof(uint64_t), &memfd);
  printf("c: recv memfd = %d sz = %ld\n", memfd, *sz);

  *addr = 0;
  *addr = mmap((void *)0x600000000000, *sz, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_FIXED, memfd, 0);
  if ((int64_t)*addr == -1) {
    perror("c: map failed");
    exit(1);
  }

  printf("c: mmap: %p\n", *addr);
//   register_uffd((uint64_t) uffd, *addr, *sz);

  // send userfault fd
  printf("c: send uffd %d\n", uffd);
  size = sock_fd_write(sock, addr, sizeof(*addr), uffd);

  int status = 0;
  sock_recv(sock, (char*)&status, 1);
  close(uffd);

  if (status) {
    fprintf(stderr, "c: registration failed\n");
    exit(1);
  }
}
