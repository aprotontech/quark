

#ifndef __QUARK_SDK_DRIVER__
#define __QUARK_SDK_DRIVER__

// driver/wifi/wifi.h
int rc_set_wifi(const char* ssid, const char* password);

// driver/wifi/wifi.h
int rc_get_wifi_local_ip(char ip[16]);

int rc_get_wifi_status();

#endif