

#ifndef __QUARK_SDK_SYSTEM__
#define __QUARK_SDK_SYSTEM__

// framework/system/include/rc_thread.h
int rc_sleep(int ms);


// framework/system/include/rc_event.h
typedef void* rc_event;

rc_event rc_event_init();

int rc_event_wait(rc_event event, int timeout_ms);

int rc_event_signal(rc_event event);

int rc_event_uninit(rc_event event);

#endif