#ifndef APP_NET_WIFI_H_
#define APP_NET_WIFI_H_

/* Longest wifi_connect() waits for association and a DHCP lease. */
#define WIFI_CONNECT_TIMEOUT_MS 30000

/* Connects to Wi-Fi with the given SSID and PSK. Returns 0, or the error. */
int wifi_connect(const char *ssid, const char *psk);

/* Drops the Wi-Fi. A failure is only logged, there is nothing for the caller to do about it. */
void wifi_disconnect(void);

#endif /* APP_NET_WIFI_H_ */
