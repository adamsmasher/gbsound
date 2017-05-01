all: ../gbsound.gb

DEPS=\
build/decompress.o\
build/gbsound.o\
build/main.o

build/%.o: %.asm data/test.bin
	rgbasm -o $@ $< > /dev/null

../gbsound.gb: $(DEPS)
	rgblink -t -m build/gbsound.map -o ../gbsound.gb $(DEPS)
	rgbfix -v -t "GBSOUND" -j -m 0 -n 0 -p 0xF0 -r 0 ../gbsound.gb

.PHONY: clean

clean:
	find build -mindepth 1 -not -name README | xargs rm -rf
	rm -f ../gbsound.gb
