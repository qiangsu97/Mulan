/* Copyright (c) 2024, Meili Authors */

#ifndef _INCLUDE_MEILI_FLOW_H
#define _INCLUDE_MEILI_FLOW_H

#include "./meili_net.h"
#include "./meili_pkt.h"

#ifdef MEILI_PKT_DPDK_BACKEND

#include <rte_common.h>
#include <rte_ip.h>
#include <rte_mbuf.h>
#include <rte_tcp.h>
#include <rte_thash.h>
#include <rte_udp.h>
#include <string.h>

extern uint8_t rss_symmetric_key[40];

#ifdef RTE_MACHINE_CPUFLAG_SSE4_2
#include <rte_hash_crc.h>
#define DEFAULT_HASH_FUNC rte_hash_crc
#else
#include <rte_jhash.h>
#define DEFAULT_HASH_FUNC rte_jhash
#endif

typedef struct _meili_flow_table {
    struct rte_hash *hash;
    char *data;
    int cnt;
    int entry_size;
}meili_flow_table;

struct ipv4_5tuple {
    uint8_t proto;
    uint32_t src_addr;
    uint32_t dst_addr;
    uint16_t src_port;
    uint16_t dst_port;
};

/* from l2_forward example, but modified to include port. This should
 * be automatically included in the hash functions since it hashes
 * the struct in 4byte chunks. */
union ipv4_5tuple_host {
        struct {
                uint8_t pad0;
                uint8_t proto;
                uint16_t virt_port;
                uint32_t ip_src;
                uint32_t ip_dst;
                uint16_t port_src;
                uint16_t port_dst;
        };
        //__m128i xmm;
};

meili_flow_table *
flow_table_create(int cnt, int entry_size);

int
flow_table_add_pkt(meili_flow_table *table, struct rte_mbuf *pkt, char **data);

int
flow_table_lookup_pkt(meili_flow_table *table, struct rte_mbuf *pkt, char **data);

int32_t
flow_table_remove_pkt(meili_flow_table *table, struct rte_mbuf *pkt);

int
flow_table_add_key(meili_flow_table *table, struct ipv4_5tuple *key, char **data);

int
flow_table_lookup_key(meili_flow_table *table, struct ipv4_5tuple *key, char **data);

int32_t
flow_table_remove_key(meili_flow_table *table, struct ipv4_5tuple *key);

int32_t
flow_table_iterate(meili_flow_table *table, const void **key, void **data, uint32_t *next);

void
flow_table_free(meili_flow_table *table);

/* TODO(@sdnfv): Add function to calculate hash and then make lookup/get
 * have versions with precomputed hash values */
// hash_sig_t
// flow_table_hash(struct rte_mbuf *buf);

static inline void
_flow_table_print_key(struct ipv4_5tuple *key) {
        printf("IP: %" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8, key->src_addr & 0xFF, (key->src_addr >> 8) & 0xFF,
               (key->src_addr >> 16) & 0xFF, (key->src_addr >> 24) & 0xFF);
        printf("-%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8 " ", key->dst_addr & 0xFF, (key->dst_addr >> 8) & 0xFF,
               (key->dst_addr >> 16) & 0xFF, (key->dst_addr >> 24) & 0xFF);
        printf("Port: %d %d Proto: %d\n", key->src_port, key->dst_port, key->proto);
}

static inline char *
flow_table_get_data(meili_flow_table *table, int32_t index) {
        return &table->data[index * table->entry_size];
}

static inline int
flow_table_fill_key(struct ipv4_5tuple *key, struct rte_mbuf *pkt) {
        struct rte_ipv4_hdr *ipv4_hdr;
        struct rte_tcp_hdr *tcp_hdr;
        struct rte_udp_hdr *udp_hdr;

        if (unlikely(!meili_pkt_is_ipv4(pkt))) {
                return -EPROTONOSUPPORT;
        }
        ipv4_hdr = meili_ipv4_hdr_safe(pkt);
        memset(key, 0, sizeof(struct ipv4_5tuple));
        key->proto = ipv4_hdr->next_proto_id;
        key->src_addr = ipv4_hdr->src_addr;
        key->dst_addr = ipv4_hdr->dst_addr;
        if (key->proto == IP_PROTO_TCP) {
                tcp_hdr = meili_tcp_hdr_safe(pkt);
                key->src_port = tcp_hdr->src_port;
                key->dst_port = tcp_hdr->dst_port;
        } else if (key->proto == IP_PROTO_UDP) {
                udp_hdr = meili_udp_hdr_safe(pkt);
                key->src_port = udp_hdr->src_port;
                key->dst_port = udp_hdr->dst_port;
        } else {
                key->src_port = 0;
                key->dst_port = 0;
        }
        return 0;
}

static inline int
flow_table_fill_key_symmetric(struct ipv4_5tuple *key, struct rte_mbuf *pkt) {
        if (flow_table_fill_key(key, pkt) < 0) {
                return -EPROTONOSUPPORT;
        }

        if (key->dst_addr > key->src_addr) {
                uint32_t temp = key->dst_addr;
                key->dst_addr = key->src_addr;
                key->src_addr = temp;
        }

        if (key->dst_port > key->src_port) {
                uint16_t temp = key->dst_port;
                key->dst_port = key->src_port;
                key->src_port = temp;
        }

        return 0;
}

/* Hash a flow key to get an int. From L3 fwd example */
static inline uint32_t
flow_table_ipv4_hash_crc(const void *data, __rte_unused uint32_t data_len, uint32_t init_val) {
        union ipv4_5tuple_host *k;
        uint32_t t;
        const uint32_t *p;

        k = (union ipv4_5tuple_host*) malloc(sizeof(union ipv4_5tuple_host));
        rte_memcpy(k, data, sizeof(union ipv4_5tuple_host));

        t = k->proto;
        p = (const uint32_t *)&k->port_src;

#ifdef RTE_MACHINE_CPUFLAG_SSE4_2
        init_val = rte_hash_crc_4byte(t, init_val);
        init_val = rte_hash_crc_4byte(k->ip_src, init_val);
        init_val = rte_hash_crc_4byte(k->ip_dst, init_val);
        init_val = rte_hash_crc_4byte(*p, init_val);
#else  /* RTE_MACHINE_CPUFLAG_SSE4_2 */
        init_val = rte_jhash_1word(t, init_val);
        init_val = rte_jhash_1word(k->ip_src, init_val);
        init_val = rte_jhash_1word(k->ip_dst, init_val);
        init_val = rte_jhash_1word(*p, init_val);
#endif /* RTE_MACHINE_CPUFLAG_SSE4_2 */
        return (init_val);
}

/*software caculate RSS*/
static inline uint32_t
calculate_softrss(struct ipv4_5tuple *key) {
        union rte_thash_tuple tuple;
        uint8_t rss_key_be[RTE_DIM(rss_symmetric_key)];
        uint32_t rss_l3l4;

        rte_convert_rss_key((uint32_t *)rss_symmetric_key, (uint32_t *)rss_key_be, RTE_DIM(rss_symmetric_key));

        tuple.v4.src_addr = rte_be_to_cpu_32(key->src_addr);
        tuple.v4.dst_addr = rte_be_to_cpu_32(key->dst_addr);
        tuple.v4.sport = rte_be_to_cpu_16(key->src_port);
        tuple.v4.dport = rte_be_to_cpu_16(key->dst_port);

        rss_l3l4 = rte_softrss_be((uint32_t *)&tuple, RTE_THASH_V4_L4_LEN, rss_key_be);

        return rss_l3l4;
}
#else
#endif /* DPDK backend */

#endif  // _INCLUDE_MEILI_FLOW_H