OBJ_DIR=build
BIN=build/gdb-bridge

.DEFAULT_GOAL=$(BIN)

.PHONY: run

$(BIN): $(wildcard *.c)
	mkdir -p $(@D)
	gcc $^ -o $@ # -lpthread

run: $(BIN)
	./$(BIN)
