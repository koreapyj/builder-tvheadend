/* tvhepg-extract — CLI: extract EPG (current programme by default) from a .ts file as JSON. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "libtvhepg.h"

int main(int argc, char **argv)
{
  tvhepg_mode_t mode = TVHEPG_NOW;
  const char *charset = NULL, *lang = NULL, *file = NULL;
  int pretty = 0;

  for (int i = 1; i < argc; i++) {
    if      (!strcmp(argv[i], "--now")) mode = TVHEPG_NOW;
    else if (!strcmp(argv[i], "--all")) mode = TVHEPG_ALL;
    else if (!strcmp(argv[i], "--pretty")) pretty = 1;
    else if (!strcmp(argv[i], "--charset") && i + 1 < argc) charset = argv[++i];
    else if (!strcmp(argv[i], "--lang") && i + 1 < argc) lang = argv[++i];
    else if (argv[i][0] != '-') file = argv[i];
  }
  if (!file) {
    fprintf(stderr, "usage: %s [--now|--all] [--charset CS] [--lang L] [--pretty] FILE.ts\n", argv[0]);
    return 2;
  }

  if (tvhepg_init(charset)) return 1;
  if (tvhepg_feed_file(file, mode)) return 1;
  char *json = tvhepg_to_json(mode, lang, pretty);
  fputs(json, stdout);
  free(json);
  tvhepg_done();
  return 0;
}
