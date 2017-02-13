all: ../famiconv

DEPS=\
build/Importer.o\
build/famiconv.o\
build/Tokenizer.o

HEADERS=\
Command.h\
Importer.h\
Song.h\
Tokenizer.h

build/%.o: %.cpp $(HEADERS)
	g++ -std=c++17 -Wall -O2 -Werror -c -o $@ $<

../famiconv: $(DEPS)
	g++ -o famiconv $(DEPS)

.PHONY: clean

clean:
	find build -mindepth 1 -not -name README | xargs rm -rf
	rm -f ../famiconv
