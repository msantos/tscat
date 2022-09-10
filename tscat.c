/* Copyright (c) 2020-2022, Michael Santos <michael.santos@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

#include "getnline.h"
#include "restrict_process.h"
#include "strtonum.h"

#define TS_VERSION "0.3.0"

enum { TS_WR_BLOCK = 0, TS_WR_DROP, TS_WR_EXIT };

typedef struct {
  int output;
  char *label;
  char *format;
  int write_error;
  int print_timestamp;
} ts_state_t;

static int tscatin(ts_state_t *s);
static int tscatout(ts_state_t *s, char *buf, size_t buflen);
static void usage(void);

extern char *__progname;

static const struct option long_options[] = {
    {"format", required_argument, NULL, 'f'},
    {"output", required_argument, NULL, 'o'},
    {"write-error", required_argument, NULL, 'W'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}};

int main(int argc, char *argv[]) {
  int ch;
  ts_state_t s = {0};
  time_t now;
  const char *errstr = NULL;

  now = time(NULL);
  if (now == -1)
    err(EXIT_FAILURE, "time");

  /* Initialize timezone before enabling process restrictions.
   *
   * localtime(3) reads /etc/timezone.
   */
  (void)localtime(&now);

  if (restrict_process_init() < 0)
    err(EXIT_FAILURE, "restrict_process_init");

  if (setvbuf(stdout, NULL, _IOLBF, 0) < 0)
    err(EXIT_FAILURE, "setvbuf");

  s.output = STDOUT_FILENO;
  s.print_timestamp = 1;

  while ((ch = getopt_long(argc, argv, "f:ho:W:", long_options, NULL)) != -1) {
    switch (ch) {
    case 'f':
      s.format = optarg;
      break;
    case 'o':
      s.output = strtonum(optarg, 0, 3, &errstr);
      if (errstr != NULL)
        errx(2, "strtonum: %s", errstr);
      break;
    case 'W':
      if (strcmp(optarg, "block") == 0)
        s.write_error = TS_WR_BLOCK;
      else if (strcmp(optarg, "drop") == 0)
        s.write_error = TS_WR_DROP;
      else if (strcmp(optarg, "exit") == 0)
        s.write_error = TS_WR_EXIT;
      else
        errx(2, "invalid option: %s: block|drop|exit", optarg);

      break;
    case 'h':
    default:
      usage();
      exit(0);
    }
  }

  argc -= optind;
  argv += optind;

  s.label = (argc == 0) ? "" : argv[0];

  if (s.format == NULL)
    s.format = "%FT%T%z";

  if ((s.write_error != TS_WR_BLOCK) && (s.output & STDOUT_FILENO) &&
      (fcntl(fileno(stdout), F_SETFL, O_NONBLOCK) < 0))
    err(EXIT_FAILURE, "fcntl");

  if ((s.write_error != TS_WR_BLOCK) && (s.output & STDERR_FILENO) &&
      (fcntl(fileno(stderr), F_SETFL, O_NONBLOCK) < 0))
    err(EXIT_FAILURE, "fcntl");

  if (restrict_process_stdin() < 0)
    err(EXIT_FAILURE, "restrict_process_stdin");

  if (tscatin(&s) < 0)
    err(EXIT_FAILURE, "tscatin");

  return 0;
}

static int tscatin(ts_state_t *s) {
  char *buf = NULL;
  size_t buflen = 0;
  ssize_t n;

  while ((n = getnline(&buf, &buflen, 4096, stdin)) != -1) {
    if (tscatout(s, buf, n) < 0) {
      if (errno == EAGAIN && s->write_error == TS_WR_DROP)
        continue;
      return -1;
    }
  }

  free(buf);
  if (ferror(stdin))
    return -1;
  return 0;
}

static int tscatout(ts_state_t *s, char *buf, size_t n) {
  char timestamp[64] = {0};
  time_t now;
  struct tm *tm;
  int nl;

  if (n == 0)
    return 0;

  nl = (buf[n - 1] == '\n');

  now = time(NULL);
  if (now == -1)
    return -1;

  tm = localtime(&now);

  /* Linux:
   * If the length of the result string (including the terminating
   * null byte) would exceed max bytes, then strftime() returns 0, and the
   * contents of the array are undefined.
   *
   * Note that the return value 0 does not necessarily indicate an error.
   * For example, in many locales %p yields an empty string.  An  empty
   * format string will likewise yield an empty string.
   *
   * OpenBSD:
   * Note that while this implementation of strftime() will always NUL
   * terminate buf, other implementations may not do so when maxsize is not
   * large enough to store the entire time string.  The contents of buf are
   * implementation specific in this case.
   */
  if (strftime(timestamp, sizeof(timestamp) - 1, s->format, tm) == 0)
    timestamp[0] = '\0';

  if (s->print_timestamp) {
    if (s->output & STDOUT_FILENO)
      if (fprintf(stdout, "%s%s%s%s", timestamp,
                  timestamp[0] == '\0' ? "" : " ", s->label,
                  s->label[0] == '\0' ? "" : " ") < 0)
        return -1;

    if (s->output & STDERR_FILENO)
      if (fprintf(stderr, "%s%s%s%s", timestamp,
                  timestamp[0] == '\0' ? "" : " ", s->label,
                  s->label[0] == '\0' ? "" : " ") < 0)
        return -1;
  }

  if (s->output & STDOUT_FILENO)
    if (fprintf(stdout, "%s", buf) < 0)
      return -1;

  if (s->output & STDERR_FILENO)
    if (fprintf(stderr, "%s", buf) < 0)
      return -1;

  s->print_timestamp = nl;

  return 0;
}

static void usage() {
  (void)fprintf(
      stderr,
      "[OPTION] [<LABEL>]\n"
      "Timestamp stdin to stdout/stderr\n"
      "version: %s (using %s mode process restriction)\n\n"
      "-o, --output <1|2|3>      stdout=1, stderr=2, both=3 (default: 1)\n"
      "-f, --format <fmt>        timestamp format (see strftime(3)) (default: "
      "%%F%%T%%z)\n"
      "-W, --write-error <exit|drop|block>\n"
      "                          behaviour if write buffer is full (default: "
      "block)\n"
      "-h, --help                usage summary\n",
      TS_VERSION, RESTRICT_PROCESS);
}
