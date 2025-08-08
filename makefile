CC=gcc
CFLAG=-Wall -Wextra -O2 -g
LIB=-lm -lraylib -lfftw3 -lpulse
SRC=main.c visualizerfiles.c  visualizerpulse.c common.c
BIN=visualizer
INSTALL_DIR=/usr/local/bin/

all: main.c
	$(CC) $(SRC) $(CFLAG) $(LIB) -o $(BIN)

install: $(BIN)
	cp $(BIN) $(INSTALL_DIR)

uninstall:
	rm $(INSTALL_DIR)$(BIN)
