all: ../famiconv

DEPS=\
build/famiconv.o\
build/Importer.o\
build/Tokenizer.o

build/%.o: %.cpp %.h
	g++ -std=c++17 -Wall -O2 -Werror -c -o $@ $<

../famiconv: $(DEPS)
	g++ -o famiconv $(DEPS)

.PHONY: clean

clean:
	find build -mindepth 1 -not -name README | xargs rm -rf
	rm -f ../famiconv
