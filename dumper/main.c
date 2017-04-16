#include <stdio.h>
#include <stdint.h>

int dump_file_named(char*);
int dump_file(FILE*);
int ctrl_byte(FILE*);
int tempo(FILE*);
int waves(FILE*);
int instruments(FILE*);

int main(int argc, char **argv) {
  if(argc != 2) {
    return 255;
  }

  return dump_file_named(argv[1]);
}

int dump_file_named(char *filename) {
  int ret;
  FILE *fp = fopen(filename, "rb");
  if(!fp) {
    fprintf(stderr, "File %s not found\n", filename);
    return 254;
  }
  ret = dump_file(fp);
  fclose(fp);
  return ret;
}

int dump_file(FILE *fp) {
  int ret;

  if((ret = ctrl_byte(fp))) {
    return ret;
  }

  if((ret = ctrl_byte(fp))) {
    return ret;
  }

  if((ret = tempo(fp))) {
    return ret;
  }

  if((ret = waves(fp))) {
    return ret;
  }

  if((ret = instruments(fp))) {
    return ret;
  }

  return 0;
}

int ctrl_byte(FILE *fp) {
  int ctrl_byte;
  if((ctrl_byte = fgetc(fp)) == EOF) {
    fprintf(stderr, "unexpected EOF\n");
    return EOF;
  } else {
    printf("Ctrl byte: %d\n", ctrl_byte);
    return 0;
  }
}

int tempo(FILE *fp) {
  int tempo;
  if((tempo = fgetc(fp)) == EOF) {
    fprintf(stderr, "unexpected EOF\n");
    return EOF;
  }
  printf("Tempo: %d\n", tempo);
  return 0;
}

int waves(FILE *fp) {
  int waveBytes;
  uint8_t wave[16];

  if((waveBytes = fgetc(fp)) == EOF) {
    fprintf(stderr, "unexpected EOF\n");
    return EOF;
  }
  waveBytes = waveBytes ? waveBytes : 256;
  printf("Wave bytes: %d\n", waveBytes);

  /* special case for no waves */
  if(waveBytes == 1) {
    fgetc(fp);
    return 0;
  }

  if(waveBytes % 16 != 0) {
    fprintf(stderr, "Invalid wave byte count: %d\n", waveBytes);
    return 1;
  }

  while(waveBytes) {
    int i;
    int cnt = fread(wave, 16, 1, fp);
    if(cnt != 1) {
      fprintf(stderr, "failed to read wave\n");
      return EOF;
    }
    for(i = 0; i < 16; i++) {
      printf("%2X ", wave[i]);
    }
    printf("\n");
    waveBytes -= 16;
  }

  return 0;
}

int instruments(FILE *fp) {
  int instrumentBytes;

  if((instrumentBytes = fgetc(fp)) == EOF) {
    fprintf(stderr, "unexpected EOF\n");
    return EOF;
  }
  instrumentBytes = instrumentBytes ? instrumentBytes : 256;
  printf("Instrument bytes: %d\n", instrumentBytes);

  if(instrumentBytes % 16 != 0) {
    fprintf(stderr, "Invalid instrument byte count: %d\n", instrumentBytes);
    return 1;
  }

  printf("Skipping instrument table\n");
  fseek(fp, instrumentBytes, SEEK_CUR);
}
