#ifndef PTHREAD_CHECK_H
#define PTHREAD_CHECK_H

#include <string.h>
#include "error.h"

/* Pthread routines return an error number directly; they do not set errno. */
#define PTHREAD_CHECK(call) do { \
  int pthread_error_ = (call); \
  if (pthread_error_) \
    ERROR("%s: %s", #call, strerror(pthread_error_)); \
} while (0)

#endif
