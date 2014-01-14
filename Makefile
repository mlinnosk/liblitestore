# Makefile template for shared library

CC = gcc # C compiler
CCPP = g++
# CFLAGS = -Wall -O2 -fPIC
CFLAGS = -fPIC -Wall -Wextra -O2 -g # C flags
LDFLAGS = -shared  # linking flags
RM = rm -f  # rm command
LIB_NAME = litestore
TARGET_LIB = liblitestore.so # target lib
TEST_MAIN = litestore_test

SRCS = src/litestore.c # source files
OBJS = $(SRCS:.c=.o)
TEST_SRCS = tests/main.cpp tests/litestore_raw_test.cpp tests/litestore_iterator_test.cpp tests/litestore_kv_test.cpp tests/litestore_array_test.cpp
TEST_INC = ./src

.PHONY: all
all: ${TARGET_LIB}

.PHONY: memtest
memtest: $(TEST_MAIN)
	LD_LIBRARY_PATH=. valgrind --tool=memcheck ./$(TEST_MAIN)

.PHONY: test
test: $(TEST_MAIN)
	LD_LIBRARY_PATH=. ./$(TEST_MAIN)

$(TEST_MAIN): $(TARGET_LIB) ${TEST_SRCS}
	$(CCPP) -Wall -o $(TEST_MAIN) ${TEST_SRCS} -I$(TEST_INC) -L./ -l$(LIB_NAME) -lsqlite3 -lgtest -lpthread

$(TARGET_LIB): $(OBJS)
	$(CC) ${LDFLAGS} -o $@ $^

$(SRCS:.c=.d):%.d:%.c
	$(CC) $(CFLAGS) -MM $< >$@

# include $(SRCS:.c=.d)

.PHONY: clean
clean:
	-${RM} ${TARGET_LIB} ${OBJS} $(SRCS:.c=.d) $(TEST_MAIN)