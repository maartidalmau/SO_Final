#COMPILER
CC = gcc
FLAGS = -Wall -Wextra -Werror -pthread -I./console -I.
DFLAGS = -Wall -Wextra -g -pthread -I./console -I.

SRC = maester.c \
	  console/console.c \
	  console/trade.c \
	  console/list.c \
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