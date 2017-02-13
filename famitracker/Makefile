all: ../famiconv

DEPS=\
../famibuild/famiconv.o\
../famibuild/Importer.o\
../famibuild/Tokenizer.o

../famibuild/%.o: %.cpp %.h
	g++ -std=c++17 -Wall -O2 -Werror -c -o $@ $<

../famiconv: $(DEPS)
	g++ -o famiconv $(DEPS)

.PHONY: clean

clean:
	find ../famibuild -mindepth 1 -not -name README | xargs rm -rf
	rm -f ../famiconv
