#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Interpreter.h"
#include "Parser.h"
#include "Tree.h"
#include "Scanner.h"
#include "error.h"

static Scanner scan;
static int failed;

static char *next(void)   { return nextScanner(scan); }
static char *curr(void)   { return currScanner(scan); }
static int cmp(char *s)   { return cmpScanner(scan, s); }
static int eat(char *s)   { return eatScanner(scan, s); }

static void f_words(T_words t);
static void f_command(T_command t);
static void f_pipeline(T_pipeline t);
static void f_sequence(T_sequence t);

static void syntax(const char *message) {
  if (!failed)
    fprintf(stderr, "syntax error at position %d: %s\n",
            posScanner(scan), message);
  failed = 1;
}

static int is_word(void) {
  char *s = curr();
  return s && !cmp("|") && !cmp("&") && !cmp(";") &&
    !cmp("<") && !cmp(">") && !cmp("(") && !cmp(")") &&
    !cmp("{") && !cmp("}");
}

static T_words p_words(void) {
  T_words words = NULL;
  T_words *tail = &words;
  while (is_word()) {
    *tail = new_words();
    (*tail)->word = new_word();
    (*tail)->word->s = strdup(curr());
    if (!(*tail)->word->s)
      ERROR("strdup() failed");
    next();
    tail = &(*tail)->words;
  }
  return words;
}

static char *p_filename(void) {
  if (!is_word()) {
    syntax("expected a redirection filename");
    return NULL;
  }
  char *name = strdup(curr());
  if (!name)
    ERROR("strdup() failed");
  next();
  return name;
}

static T_command p_command(void) {
  T_words words = p_words();
  if (!words) {
    syntax("expected a command");
    return NULL;
  }
  T_command command = new_command();
  command->words = words;
  if (eat("<"))
    command->input = p_filename();
  if (!failed && eat(">"))
    command->output = p_filename();
  if (failed) {
    f_command(command);
    return NULL;
  }
  return command;
}

static T_pipeline p_pipeline(void) {
  T_pipeline pipeline = NULL;
  T_pipeline *tail = &pipeline;
  do {
    T_command command = p_command();
    if (!command) {
      f_pipeline(pipeline);
      return NULL;
    }
    *tail = new_pipeline();
    (*tail)->command = command;
    tail = &(*tail)->pipeline;
  } while (eat("|"));
  return pipeline;
}

static T_sequence p_pipeline_as_sequence(void) {
  T_pipeline pipeline = p_pipeline();
  if (!pipeline)
    return NULL;
  T_sequence sequence = new_sequence();
  sequence->pipeline = pipeline;
  if (eat("&"))
    sequence->op = "&";
  else if (eat(";"))
    sequence->op = ";";
  else if (curr()) {
    syntax("expected ';', '&', or end of line");
    f_sequence(sequence);
    return NULL;
  }
  return sequence;
}

/* Parse and execute one pipeline at a time, allowing builtins to change the
   environment of subsequent pipelines. A syntax error discards this line. */
extern void interpret(char *line, int *eof, Jobs jobs) {
  scan = newScanner(line);
  failed = 0;
  while (!*eof && !failed && curr()) {
    T_sequence sequence = p_pipeline_as_sequence();
    if (!sequence)
      break;
    interpretSequence(sequence, eof, jobs);
    f_sequence(sequence);
  }
  freeScanner(scan);
}

static void f_words(T_words t) {
  while (t) {
    T_words rest = t->words;
    free(t->word->s);
    free(t->word);
    free(t);
    t = rest;
  }
}

static void f_command(T_command t) {
  if (!t)
    return;
  f_words(t->words);
  free(t->input);
  free(t->output);
  free(t);
}

static void f_pipeline(T_pipeline t) {
  while (t) {
    T_pipeline rest = t->pipeline;
    f_command(t->command);
    free(t);
    t = rest;
  }
}

static void f_sequence(T_sequence t) {
  while (t) {
    T_sequence rest = t->sequence;
    f_pipeline(t->pipeline);
    free(t);
    t = rest;
  }
}
