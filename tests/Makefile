export SRC_PATH ?= $(realpath ../src)
export UTILS_PATH ?= $(realpath ../utils)

CC = gcc
CPPFLAGS = -I$(UTILS_PATH)
CFLAGS = -fPIC -Wall -Wextra -g
LDFLAGS = -L$(SRC_PATH)
LDLIBS = -losmem

SNIPPETS_SRC = $(sort $(wildcard snippets/*.c))
SNIPPETS = $(patsubst %.c,%,$(SNIPPETS_SRC))

.PHONY: all src snippets clean_src clean_snippets check check-fast lint

all: src snippets

src:
	$(MAKE) -C $(SRC_PATH)

snippets: $(SNIPPETS)

clean_snippets:
	rm -rf $(SNIPPETS)

clean_src:
	$(MAKE) -C $(SRC_PATH) clean

check:
	$(MAKE) clean_src clean_snippets src snippets
	python3 run_tests.py

check-fast:
	$(MAKE) clean_src clean_snippets src snippets
	python3 run_tests.py -d

lint:
	-cpplint --recursive ../src ../utils/block_meta.h ../utils/osmem.h

snippets/%: snippets/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)
