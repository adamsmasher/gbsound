all: ../famiconv

DEPS=\
build/Command.o\
build/compress.o\
build/famiconv.o\
build/hash.o\
build/Importer.o\
build/Song.o\
build/Tokenizer.o

HEADERS=\
Command.h\
compress.h\
hash.h\
Importer.h\
Song.h\
Tokenizer.h

build/%.o: %.cpp $(HEADERS)
	g++ -std=c++17 -Wall -O2 -Werror -c -o $@ $<

../famiconv: $(DEPS)
	g++ -o ../famiconv $(DEPS)

.PHONY: clean

clean:
	find build -mindepth 1 -not -name README | xargs rm -rf
	rm -f ../famiconv
