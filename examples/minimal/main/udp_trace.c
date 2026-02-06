#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include <string.h>

static const char *TAG = "UDP_TRACE";

ssize_t __real_lwip_sendto(int s, const void *dataptr, size_t size, int flags,
                           const struct sockaddr *to, socklen_t tolen);
ssize_t __real_lwip_recvfrom(int s, void *mem, size_t len, int flags,
                             struct sockaddr *from, socklen_t *fromlen);

static void log_sockaddr(const char *prefix, const struct sockaddr *addr, socklen_t addrlen, size_t bytes)
{
    if (!addr || addrlen < sizeof(struct sockaddr_in)) {
        ESP_LOGI(TAG, "%s: bytes=%u (no addr)", prefix, (unsigned)bytes);
        return;
    }
    const struct sockaddr_in *in = (const struct sockaddr_in *)addr;
    char ip[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &in->sin_addr, ip, sizeof(ip));
    ESP_LOGI(TAG, "%s: %s:%u bytes=%u", prefix, ip, ntohs(in->sin_port), (unsigned)bytes);
}

static void log_stun_txid(const char *prefix, const uint8_t *buf, size_t size)
{
    if (!buf || size < 20) {
        return;
    }
    uint16_t msg_type = (uint16_t)((buf[0] << 8) | buf[1]);
    uint16_t msg_len = (uint16_t)((buf[2] << 8) | buf[3]);
    uint32_t cookie = (uint32_t)((buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7]);
    if (cookie != 0x2112A442) {
        return;
    }
    ESP_LOGI(TAG, "%s: type=0x%04x len=%u txid=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
             prefix,
             msg_type,
             (unsigned)msg_len,
             buf[8], buf[9], buf[10], buf[11], buf[12], buf[13],
             buf[14], buf[15], buf[16], buf[17], buf[18], buf[19]);
}

ssize_t __wrap_lwip_sendto(int s, const void *dataptr, size_t size, int flags,
                           const struct sockaddr *to, socklen_t tolen)
{
    if (size >= 20) {
        const uint8_t *b = (const uint8_t *)dataptr;
        if (b[0] == 0x00 && b[1] == 0x01 && b[4] == 0x21 && b[5] == 0x12) {
            log_sockaddr("STUN send", to, tolen, size);
            log_stun_txid("STUN send txid", b, size);
        }
    }
    return __real_lwip_sendto(s, dataptr, size, flags, to, tolen);
}

ssize_t __wrap_lwip_recvfrom(int s, void *mem, size_t len, int flags,
                             struct sockaddr *from, socklen_t *fromlen)
{
    ssize_t ret = __real_lwip_recvfrom(s, mem, len, flags, from, fromlen);
    if (ret > 0 && (size_t)ret >= 20) {
        const uint8_t *b = (const uint8_t *)mem;
        if (b[0] == 0x01 && b[1] == 0x01 && b[4] == 0x21 && b[5] == 0x12) {
            log_sockaddr("STUN recv", from, fromlen ? *fromlen : 0, (size_t)ret);
            log_stun_txid("STUN recv txid", b, (size_t)ret);
        }
    }
    return ret;
}
