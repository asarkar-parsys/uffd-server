LIB_DIR=$(INSTALL_DIR)/lib

setup:
	mkdir -p $(LIB_DIR) 

%.o: %.c
	gcc -g $(INCLUDE_DIR) -fPIC -c $<
	
libuffd.so: uffd.o socket.o
	gcc -g -shared -o $(LIB_DIR)/libuffd.so uffd.o socket.o

all: setup libuffd.so
	rm uffd.o socket.o
