/* Copyright (c) 2024, Meili Authors */

#ifndef _INCLUDE_PORT_UTILS_H
#define _INCLUDE_PORT_UTILS_H

#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>

struct rx_stats {
    uint64_t rx[RTE_MAX_ETHPORTS];
};

struct tx_stats {
    uint64_t tx[RTE_MAX_ETHPORTS];
    uint64_t tx_drop[RTE_MAX_ETHPORTS];
};

struct port_info {
    uint8_t num_ports;
    uint8_t id[RTE_MAX_ETHPORTS];
    uint8_t init[RTE_MAX_ETHPORTS];
    struct rte_ether_addr mac[RTE_MAX_ETHPORTS];
    volatile struct rx_stats rx_stats;
    volatile struct tx_stats tx_stats;
};

/*
 * Updates the ether_addr struct with a fake, safe MAC address.
 */
static inline int
get_fake_macaddr(struct rte_ether_addr *mac_addr) {
    uint16_t *mac_addr_bytes = (uint16_t *)((struct rte_ether_addr *)(mac_addr)->addr_bytes);
    mac_addr_bytes[0] = 2;
    mac_addr_bytes[1] = 0;
    mac_addr_bytes[2] = 0;
    return 0;
}

/*
 * Tries to fetch the MAC address of the port_id.
 * Return 0 if port is valid, -1 if port is invalid.
 */
static inline int
get_port_macaddr(uint8_t port_id, struct rte_ether_addr *mac_addr) {
    if (!rte_eth_dev_is_valid_port(port_id)) {
        return -1;
    }
    rte_eth_macaddr_get(port_id, mac_addr);
    return 0;
}

#endif