#include <stdio.h>
#include <stdint.h>

int dump_file_named(char*);
int dump_file(FILE*);
int ctrl_byte(FILE*);
int tempo(FILE*);
int waves(FILE*);
int pattern_table(FILE*);
int instrument_table(FILE*);
int patterns(FILE*);
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

  if((ret = pattern_table(fp))) {
    return ret;
  }

  if((ret = instrument_table(fp))) {
    return ret;
  }

  if((ret = patterns(fp))) {
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
      uint8_t high = wave[i] >> 4;
      uint8_t low = wave[i] & 0x0F;
      printf("%2X %2X ", high, low);
    }
    printf("\n");
    waveBytes -= 16;
  }

  return 0;
}

int pattern_table(FILE *fp) {
  int pattern_bytes;

  if((pattern_bytes = fgetc(fp)) == EOF) {
    fprintf(stderr, "unexpected EOF\n");
    return EOF;
  }
  pattern_bytes = pattern_bytes ? pattern_bytes : 256;
  printf("Pattern bytes: %d\n", pattern_bytes);

  if(pattern_bytes % 2 != 0) {
    fprintf(stderr, "Invalid pattern byte count: %d\n", pattern_bytes);
    return 1;
  }

  printf("Skipping pattern table\n");
  fseek(fp, pattern_bytes, SEEK_CUR);

  return 0;
}

int instrument_table(FILE *fp) {
  int instrument_bytes;

  if((instrument_bytes = fgetc(fp)) == EOF) {
    fprintf(stderr, "unexpected EOF\n");
    return EOF;
  }
  instrument_bytes = instrument_bytes ? instrument_bytes : 256;
  printf("Instrument bytes: %d\n", instrument_bytes);

  if(instrument_bytes % 2 != 0) {
    fprintf(stderr, "Invalid instrument byte count: %d\n", instrument_bytes);
    return 1;
  }

  printf("Skipping instrument table\n");
  fseek(fp, instrument_bytes, SEEK_CUR);

  return 0;
}

int patterns(FILE *fp) {
  /* TODO */
  return 0;
}

int instruments(FILE *fp) {
  /* TODO */
  return 0;
}
