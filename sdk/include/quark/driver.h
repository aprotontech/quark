

#ifndef __QUARK_SDK_DRIVER__
#define __QUARK_SDK_DRIVER__

#include "system.h"

/////////////////////////////////////////////////////
// WIFI
// driver/wifi/wifi.h
int rc_set_wifi(const char* ssid, const char* password);

int rc_get_wifi_local_ip(char ip[16]);

int rc_get_wifi_status(int* wifi_is_connected);

//////////////////////////////////////////////////
// PLAYER
// driver/player/rc_player.h

typedef void* rc_player;

typedef int (*player_write_data)(rc_player player, char* data, int len);

rc_player rc_player_init(rc_buf_queue queue, player_write_data writer);

int rc_player_restart(rc_player player, int code, int nbits_per_sample,
                      int samples, int channels);

int rc_player_uninit(rc_player player);

#endif