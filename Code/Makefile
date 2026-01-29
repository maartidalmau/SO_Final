#COMPILER
CC = gcc
FLAGS = -Wall -Wextra -Werror -pthread -I. -I./console -I./network 
DFLAGS = -Wall -Wextra -g -pthread -I. -I./console -I./network

SRC = maester.c \
	  console/console.c \
	  console/trade.c \
	  console/list.c \
	  network/server.c \
	  network/network.c \
	  network/frame.c \
	  network/router.c \
	  utils.c \
	  dataStructures.c

OBJ = $(SRC:.c=.o)

BINARY = maester

# Default target
all: $(BINARY)

# Link objects into final binary
$(BINARY): $(OBJ)
	$(CC) $(FLAGS) $(OBJ) -o $(BINARY)

# Compile .c into .o
%.o: %.c
	$(CC) $(FLAGS) -c $< -o $@

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(BINARY)

rebuild: fclean all

gdb: 
	gdb ./$(BINARY)

valgrind: 
	valgrind --leak-check=full ./$(BINARY)