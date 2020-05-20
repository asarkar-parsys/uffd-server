ROOT_DIR=${PWD}
export INSTALL_DIR = $(ROOT_DIR)/bin/
export INCLUDE_DIR = -I$(ROOT_DIR)/include

all: libs test

libs:
	$(MAKE) -C src all 
test:
	$(MAKE) -C test-apps all

run-server:
	$(MAKE) -C test-apps run-server

run-client:
ifdef SOCK
	$(MAKE) -C test-apps run-client 
else
	@echo 'no socket arguments provided'
endif 

kill-server:
	killall socket-server

clean:
	rm -rf $(INSTALL_DIR)
