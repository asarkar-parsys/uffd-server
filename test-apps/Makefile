APP_DIR=$(INSTALL_DIR)/test/
LD_FLAGS = -L$(INSTALL_DIR)/lib/ -Wl,-rpath=$(INSTALL_DIR)/lib/ 
LIBS = -luffd
all: setup socket-server socket-client mpi 
setup:
	mkdir -p $(APP_DIR)

socket-server:
	gcc -g -std=gnu11 -pthread $(INCLUDE_DIR) server.c -o $(APP_DIR)/socket-server $(LD_FLAGS) $(LIBS)

socket-client:
	gcc -g -std=gnu11 -pthread $(INCLUDE_DIR) client.c -o $(APP_DIR)/socket-client $(LD_FLAGS) $(LIBS)

mpi:
	mpicc -std=gnu11 -pthread $(INCLUDE_DIR) mpi.c -o $(APP_DIR)/mpi $(LD_FLAGS) $(LIBS)

run-server:
	cd $(APP_DIR);./socket-server

run-client:
	cd $(APP_DIR);./socket-client $(SOCK)
