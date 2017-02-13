all: gbsound.gb famiconv

.PHONY: clean famiconv gbsound.gb

famiconv:
	cd famitracker && $(MAKE)

gbsound.gb:
	cd src && $(MAKE)

clean:
	cd src && $(MAKE) clean
	cd famitracker && $(MAKE) clean
