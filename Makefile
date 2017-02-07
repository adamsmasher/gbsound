all: gbsound.gb famiconv

DEPS=\
build/gbsound.o

FAMIDEPS=\
famibuild/famiconv.o\
famibuild/TextExporter.o\
famibuild/Tokenizer.o

build/%.o: src/%.asm
	rgbasm -o $@ $< > /dev/null

famibuild/%.o: famitracker/%.cpp famitracker/%.h
	g++ -std=c++17 -Wall -O2 -Werror -c -o $@ $<

gbsound.gb: $(DEPS)
	rgblink -t -m build/gbsound.map -o gbsound.gb $(DEPS)
	rgbfix -v -t "GBSOUND" -j -m 0 -n 0 -p 0xF0 -r 0 gbsound.gb

famiconv: $(FAMIDEPS)
	g++ -o famiconv $(FAMIDEPS)

.PHONY: clean

clean:
	find build -mindepth 1 -not -name README | xargs rm -rf
	rm -f gbsound.gb
