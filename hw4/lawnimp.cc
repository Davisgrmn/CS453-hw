#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>

#define LAWNIMP
#include "lawnimp.h"
#undef LAWNIMP
#include "error.h"
#include "pthread_check.h"

using namespace std;

// Normal ticks last one second. Tests can shorten them without changing
// the random mole lifetimes. nanosleep affects only the calling thread.
static void tsleep(LawnRep lawn, int ticks) {
  long ms = (long)ticks * lawn->tick_ms;
  struct timespec delay = {ms / 1000, (ms % 1000) * 1000000};
  while (nanosleep(&delay, &delay) == -1)
    if (errno != EINTR)
      ERROR("nanosleep() failed");
}

static int text() {
  char* v=getenv("DISPLAY");
  return !(v && *v);
}

// Serialize each complete text event explicitly. No simulation delay or
// queue operation holds this lock, and race tools can see the ordering.
static pthread_mutex_t text_mutex = PTHREAD_MUTEX_INITIALIZER;

static void write_event(int x, int y, const char *function, const char *message) {
  PTHREAD_CHECK(pthread_mutex_lock(&text_mutex));
  printf("(%d,%d) %ld %s %s\n",x,y,(long)syscall(SYS_gettid),function,message);
  PTHREAD_CHECK(pthread_mutex_unlock(&text_mutex));
}

#define WR(X,Y,MSG) write_event(X,Y,__func__,MSG)
#define WR0 { WR(0,0,""); return 0; }

// This is a noninteractive simulation: finish workers before closing.
static void keep_running(Fl_Widget *, void *) {}

static void stop_window(void *window) {
  ((Fl_Window *)window)->hide(); // Called by FLTK on its main thread.
}

extern LINKAGE void* lawnimp_new(int lawnsize, int molesize) {
  if (text()) WR0;
  if (Fl::lock())
    ERROR("FLTK thread locking is unavailable");
  int size=lawnsize*molesize;
  Fl_Window* w=new Fl_Window(size,size,"Whack-a-Mole");
  w->callback(keep_running);
  w->end();
  w->show();
  return w;
}

extern LINKAGE void* lawnimp_run(LawnRep l) {
  (void)l;
  if (text()) WR0;
  Fl::run();
  Fl::unlock(); // Balance the initial main-thread lock before joining workers.
  return 0;
}

extern LINKAGE void* lawnimp_mole(MoleRep m) {
  LawnRep l=(LawnRep)m->lawn;
  if (text()) {
    WR(m->x,m->y,"creating");
    tsleep(l,m->vim0);
    WR(m->x,m->y,"created");
    return 0;
  }
  Fl_Window* w=(Fl_Window*)l->window;
  tsleep(l,m->vim0);
  Fl::lock();
  w->begin();
  Fl_Box* b=new Fl_Box(m->x,m->y,m->size,m->size);
  b->box(FL_OVAL_BOX);
  b->color(FL_GREEN);
  w->end();
  w->redraw();
  Fl::unlock();
  Fl::awake();
  return b;
}

extern LINKAGE void lawnimp_whack(MoleRep m) {
  LawnRep l=(LawnRep)m->lawn;
  if (text()) {
    WR(m->x,m->y,"whacking");
    tsleep(l,m->vim1);
    WR(m->x,m->y,"whacked");
    tsleep(l,m->vim2);
    WR(m->x,m->y,"expired");
    return;
  }
  Fl_Window* w=(Fl_Window*)l->window;
  Fl_Box* b=(Fl_Box*)m->box;
  tsleep(l,m->vim1);
  Fl::lock();
  b->color(FL_RED);
  w->redraw();
  Fl::unlock();
  Fl::awake();
  tsleep(l,m->vim2);
  Fl::lock();
  b->hide();
  w->remove(b);
  delete b;
  w->redraw();
  Fl::unlock();
  Fl::awake();
}

extern LINKAGE void lawnimp_stop(void* w) {
  if (text()) {
    WR(0,0,"finished");
    return;
  }
  if (Fl::awake(stop_window,w))
    ERROR("could not schedule the FLTK shutdown callback");
}

extern LINKAGE void lawnimp_free(void* w) {
  if (text()) {
    WR(0,0,"freed");
    return;
  }
  delete (Fl_Window*)w;
}
