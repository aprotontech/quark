

#ifndef __QUARK_SDK_SYSTEM__
#define __QUARK_SDK_SYSTEM__

// framework/system/include/rc_thread.h
int rc_sleep(int ms);

typedef void* rc_thread;

typedef void* (*rc_thread_function)(void* arg);

rc_thread rc_thread_create(rc_thread_function func, void* arg);

int rc_thread_join(rc_thread thread);


// framework/system/include/rc_event.h
typedef void* rc_event;

rc_event rc_event_init();

int rc_event_wait(rc_event event, int timeout_ms);

int rc_event_signal(rc_event event);

int rc_event_uninit(rc_event event);


// framework/system/include/rc_mutex.h
typedef void* rc_mutex;

rc_mutex rc_mutex_create(void* attr);

int rc_mutex_lock(rc_mutex mutex);
int rc_mutex_unlock(rc_mutex mutex);
int rc_mutex_destroy(rc_mutex mutex);

#endif