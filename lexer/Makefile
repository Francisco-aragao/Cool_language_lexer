# General purpose Makefile v3.0 - by Gab

# Set default target as all
.DEFAULT_GOAL := all

# Directories
MAIN_SRCDIR = ./src
BINDIR = ./bin
OBJDIR = ./obj
MAIN_INCDIR = ./include
LIBDIR = ./lib

# Compiler
CC = gcc

# Output
EXEC = lexer

# Change .c to .o
SRC = $(wildcard $(MAIN_SRCDIR)/*.c)
OBJ = $(addprefix $(OBJDIR)/,$(notdir $(SRC:.c=.o)))

# Get all inc dirs
MULTI_INCDIR = $(wildcard $(MAIN_INCDIR)/*/)
ALL_INCDIR = -I $(MAIN_INCDIR) $(addprefix -I , $(MULTI_INCDIR))

# Flags
CFLAGS = $(ALL_INCDIR) -Wall -Wextra -pedantic
DBGFLAGS = -g -fno-inline
LFLAGS = -L $(LIBDIR)

# Ignore these files
.PHONY : compile all run clean valgrind

# Compile source to outputs .o 
compile: $(OBJ)

ifeq ($(DEBUG),YES)
CFLAGS := $(CFLAGS) $(DBGFLAGS)
endif

$(OBJDIR)/%.o: $(MAIN_SRCDIR)/%.c
	$(CC) -c $(CFLAGS) $< -o $@

# Link everything together
all: compile
	$(CC) -o $(BINDIR)/$(EXEC) $(OBJDIR)/*.o $(LFLAGS)

# Run the program
run:
	(cd $(BINDIR) && ./$(EXEC) $(ARGS))

# Delete the program and build files
clean:
	rm -f $(BINDIR)/$(EXEC)
	rm -f $(OBJDIR)/*.o
	
# Run valgrind to search for memory leaks
valgrind:
	(cd $(BINDIR) && valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=valgrind-out.txt ./$(EXEC) $(ARGS))