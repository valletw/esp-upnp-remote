/*
 * MIT License
 * Copyright (c) 2024 William Vallet
 */

#include "udp_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define LOGGER_TAG  "udp_client"

#define UDP_CLIENT_MULTICAST_TTL    127

typedef struct
{
    int socket;
    struct sockaddr_in dest;
} udp_client_handle_t;

// Allocate client handle.
static udp_client_handle_t* udp_client_handle_allocate(void);
// Deallocate client handle.
static void udp_client_handle_release(udp_client_handle_t * const handle);
// Manage multicast connection.
static bool udp_client_multicast_bind(const udp_client_handle_t * const handle);

static udp_client_handle_t* udp_client_handle_allocate(void)
{
    // Allocate client handle.
    udp_client_handle_t * const hdl =
        (udp_client_handle_t *) malloc(sizeof(udp_client_handle_t));
    if (hdl == NULL)
        return NULL;
    memset(hdl, 0, sizeof(udp_client_handle_t));
    return hdl;
}

static void udp_client_handle_release(udp_client_handle_t * const handle)
{
    assert(handle);
    free(handle);
}

static bool udp_client_multicast_bind(const udp_client_handle_t * const handle)
{
    assert(handle);
    // Bind to any address.
    struct sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = handle->dest.sin_port;
    if (0 > bind(
            handle->socket, (struct sockaddr *) &addr, sizeof(struct sockaddr_in)))
    {
        ESP_LOGE(LOGGER_TAG, "Bind socket failed socket=%d errno=%d",
            handle->socket, errno);
        return false;
    }
    // Set multicast TTL.
    uint8_t ttl = UDP_CLIENT_MULTICAST_TTL;
    if (0 > setsockopt(
            handle->socket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(uint8_t)))
    {
        ESP_LOGE(LOGGER_TAG, "TTL configuration failed socket=%d errno=%d",
            handle->socket, errno);
        return false;
    }
    // Get IP source.
    esp_netif_ip_info_t ip_info = { 0 };
    esp_err_t err = esp_netif_get_ip_info(
        esp_netif_get_default_netif(), &ip_info);
    if (err != ESP_OK)
    {
        ESP_LOGE(LOGGER_TAG, "Get IP address failed socket=%d error=0x%x",
            handle->socket, err);
        return false;
    }
    // Assign the multicast source interface.
    struct in_addr in_addr = { 0 };
    inet_addr_from_ip4addr(&in_addr, &ip_info.ip);
    if (0 > setsockopt(
            handle->socket, IPPROTO_IP, IP_MULTICAST_IF, &in_addr,
            sizeof(struct in_addr)))
    {
        ESP_LOGE(
            LOGGER_TAG,
            "IP multicast source configuration failed socket=%d errno=%d",
            handle->socket, errno);
        return false;
    }
    // Verify multicast IP.
    struct ip_mreq mreq = { 0 };
    mreq.imr_interface.s_addr = IPADDR_ANY;
    mreq.imr_multiaddr.s_addr = handle->dest.sin_addr.s_addr;
    if (!IP_MULTICAST(ntohl(mreq.imr_multiaddr.s_addr)))
        ESP_LOGW(LOGGER_TAG, "Multicast address is not a valid multicast address");
    // Set IP to multicast group.
    if (0 > setsockopt(
            handle->socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq,
            sizeof(struct ip_mreq)))
    {
        ESP_LOGE(
            LOGGER_TAG, "IP membership configuration failed socket=%d errno=%d",
            handle->socket, errno);
        return false;
    }
    ESP_LOGD(LOGGER_TAG, "Multicast configuration success socket=%d",
        handle->socket);
    return true;
}

uintptr_t udp_client_open(
    const char * const ip_str, uint16_t port, bool multicast)
{
    if (ip_str == NULL || port == 0u)
        return 0;
    ESP_LOGI(LOGGER_TAG, "Opening for ip='%s' port=%d", ip_str, port);
    // Allocate client handle.
    udp_client_handle_t * const hdl = udp_client_handle_allocate();
    if (hdl == NULL)
        return 0;
    // Open client socket.
    int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0)
    {
        ESP_LOGE(LOGGER_TAG, "Open failed errno=%d", errno);
        udp_client_handle_release(hdl);
        return 0;
    }
    hdl->socket = sock;
    ESP_LOGD(LOGGER_TAG, "Open socket=%d ip='%s' port=%d",
        hdl->socket, ip_str, port);
    // Register destination address.
    hdl->dest.sin_family = AF_INET;
    hdl->dest.sin_addr.s_addr = inet_addr(ip_str);
    hdl->dest.sin_port = htons(port);
    // Set multicast configuration.
    if (multicast && !udp_client_multicast_bind(hdl))
    {
        ESP_LOGE(LOGGER_TAG, "Multicast configuration failed");
        if (0 != udp_client_close((uintptr_t) hdl))
            udp_client_handle_release(hdl);
        return 0;
    }
    return (uintptr_t) hdl;
}

int udp_client_close(uintptr_t handle)
{
    if (handle == 0)
        return -1;
    udp_client_handle_t * const hdl = (udp_client_handle_t *) handle;
    // Shutdown connection.
    shutdown(hdl->socket, SHUT_RD);
    ESP_LOGD(LOGGER_TAG, "Shutdown socket=%d", hdl->socket);
    // Destroy socket.
    if (-1 == close(hdl->socket) && (errno == EINTR || errno == EIO))
    {
        ESP_LOGE(LOGGER_TAG, "Close failed socket=%d errno=%d",
            hdl->socket, errno);
        return -1;
    }
    ESP_LOGD(LOGGER_TAG, "Close socket=%d", hdl->socket);
    hdl->socket = 0;
    // Release handle memory.
    udp_client_handle_release(hdl);
    return 0;
}

int udp_client_set_timeout(
    uintptr_t handle, const struct timeval * const timeout)
{
    if (handle == 0)
        return -1;
    const udp_client_handle_t * const hdl = (udp_client_handle_t *) handle;
    ESP_LOGD(LOGGER_TAG, "Set timeout socket=%d timeout=%lld.%06ld",
        hdl->socket, timeout->tv_sec, timeout->tv_usec);
    return setsockopt(
        hdl->socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}

int udp_client_poll(uintptr_t handle)
{
    if (handle == 0)
        return -1;
    const udp_client_handle_t * const hdl = (udp_client_handle_t *) handle;
    struct timeval timeout = {
        .tv_sec = 2,
        .tv_usec = 0,
    };
    fd_set fd;
    FD_ZERO(&fd);
    FD_SET(hdl->socket, &fd);
    if (0 > select(hdl->socket + 1, &fd, NULL, NULL, &timeout))
    {
        ESP_LOGE(LOGGER_TAG, "Poll failed socket=%d errno=%d",
            hdl->socket, errno);
        return -1;
    }
    return (FD_ISSET(hdl->socket, &fd)) ? 1 : 0;
}

int udp_client_read(
    uintptr_t handle, uint8_t * const payload, size_t size,
    struct sockaddr * const from)
{
    if (handle == 0 || payload == NULL || size == 0u)
        return -1;
    const udp_client_handle_t * const hdl = (udp_client_handle_t *) handle;
    // Try to receive data.
    struct sockaddr_storage source;
    socklen_t source_size = sizeof(source);
    int len = recvfrom(
        hdl->socket, payload, size, 0, (struct sockaddr *) &source,
        &source_size);
    // Error during reception.
    if (len < 0)
    {
        ESP_LOGE(LOGGER_TAG, "Received failed socket=%d errno=%d",
            hdl->socket, errno);
        return -1;
    }
    if (from != NULL)
        memcpy(from, &source, sizeof(struct sockaddr));
    char from_name[32] = "unknown";
    if (source.ss_family == AF_INET)
    {
        inet_ntoa_r(
            ((struct sockaddr_in *) &source)->sin_addr, from_name,
            sizeof(from_name) - 1u);
    }
    ESP_LOGD(LOGGER_TAG, "Receive socket=%d size=%d from='%s'",
        hdl->socket, len, from_name);
    return len;
}

int udp_client_write(uintptr_t handle, const uint8_t * const payload, size_t size)
{
    if (handle == 0 || payload == NULL || size == 0u)
        return -1;
    const udp_client_handle_t * const hdl = (udp_client_handle_t *) handle;
    // Try to send payload.
    if (0 > sendto(
            hdl->socket, payload, size, 0, (struct sockaddr *) &hdl->dest,
            sizeof(hdl->dest)))
    {
        ESP_LOGE(LOGGER_TAG, "Send failed socket=%d errno=%d",
            hdl->socket, errno);
        return -1;
    }
    ESP_LOGD(LOGGER_TAG, "Send socket=%d size=%u", hdl->socket, size);
    return size;
}
