#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

#include <stdint.h>

// Rulare fara sistem de operare (bare-metal, mod poll)
#define NO_SYS                      1
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0

// Alocare memorie pentru stiva de retea
#define MEM_LIBC_MALLOC             0
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    4000
#define MEMP_NUM_TCP_SEG            32
#define MEMP_NUM_ARP_QUEUE          10
#define PBUF_POOL_SIZE              24

// Protocoale de retea necesare pentru Wi-Fi si NTP
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_RAW                    1
#define LWIP_DHCP                   1
#define LWIP_IPV4                   1
#define LWIP_TCP                    1
#define LWIP_UDP                    1
#define LWIP_DNS                    1
#define LWIP_TCP_KEEPALIVE          1
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_NETIF_TX_SINGLE_PBUF   1
#define DHCP_DOES_ARP_CHECK         0
#define LWIP_DHCP_DOES_ACD_CHECK    0

// Configurare specifica pentru SNTP (sincronizare ora prin NTP)
#define SNTP_SERVER_DNS             1
#define SNTP_SET_SYSTEM_TIME_US(sec, us) set_system_time_us(sec, us)

// Declararea functiei tale de callback din empty.c pentru a evita warning-urile de compilare
#ifdef __cplusplus
extern "C" {
#endif
void set_system_time_us(uint32_t sec, uint32_t us);
#ifdef __cplusplus
}
#endif

// Dezactivare statistici inutile pentru a economisi memorie
#define MEM_STATS                   0
#define SYS_STATS                   0
#define MEMP_STATS                  0
#define LINK_STATS                  0

#endif /* _LWIPOPTS_H */