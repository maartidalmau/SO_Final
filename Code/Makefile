#COMPILER
CC = gcc
FLAGS = -Wall -Wextra -Werror -pthread -Iinclude -Iinclude/core -Iinclude/console -Iinclude/network
DFLAGS = -Wall -Wextra -g -pthread -Iinclude -Iinclude/core -Iinclude/console -Iinclude/network

SRC = src/maester.c \
	  src/dataStructures.c \
	  src/utils.c \
	  src/console/console.c \
	  src/console/trade.c \
	  src/console/list.c \
	  src/network/core/frame.c \
	  src/network/core/network.c \
	  src/network/handlers/allianceHandler.c \
	  src/network/handlers/frameHandler.c \
	  src/network/transport/server.c \
	  src/network/transport/client.c \
	  src/network/transport/router.c \
	  src/network/core/envoy.c

OBJ = $(patsubst src/%.c,obj/%.o,$(SRC))

BINARY = maester

# Default target
all: $(BINARY)

# Link objects into final binary
$(BINARY): $(OBJ)
	$(CC) $(FLAGS) $(OBJ) -o $(BINARY)

# Compile .c from src/ into .o in obj/
obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(FLAGS) -c $< -o $@

clean:
	rm -rf obj/

fclean: clean
	rm -f $(BINARY)

rebuild: fclean all

gdb: 
	gdb ./$(BINARY)

valgrind: 
	valgrind --leak-check=full ./$(BINARY)