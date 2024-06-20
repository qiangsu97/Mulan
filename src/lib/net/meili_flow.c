/* Copyright (c) 2024, Meili Authors */

#include "meili_flow.h"

#ifdef MEILI_PKT_DPDK_BACKEND
#include <rte_cycles.h>
#include <rte_ether.h>
#include <rte_hash.h>
#include <rte_lcore.h>
#include <rte_malloc.h>

uint8_t rss_symmetric_key[40] = {
    0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a,
    0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a,
    0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a,
};

/* Create a new flow table made of an rte_hash table and a fixed size
 * data array for storing values. Only supports IPv4 5-tuple lookups. */
meili_flow_table *
flow_table_create(int cnt, int entry_size) {
        struct rte_hash *hash;
        struct rte_hash_parameters *ipv4_hash_params;
        meili_flow_table *ft;
        int status;

        ipv4_hash_params = (struct rte_hash_parameters *) rte_malloc(NULL, sizeof(struct rte_hash_parameters), 0);
        if (!ipv4_hash_params) {
                return NULL;
        }

        char *name = rte_malloc(NULL, 64, 0);
        /* create ipv4 hash table. use core number and cycle counter to get a unique name. */
        ipv4_hash_params->entries = cnt;
        ipv4_hash_params->key_len = sizeof(struct ipv4_5tuple);
        ipv4_hash_params->hash_func = NULL;
        ipv4_hash_params->hash_func_init_val = 0;
        ipv4_hash_params->name = name;
        ipv4_hash_params->socket_id = rte_socket_id();
        snprintf(name, 64, "flow_table_%d-%" PRIu64, rte_lcore_id(), rte_get_tsc_cycles());

        // if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
        //         hash = rte_hash_create(ipv4_hash_params);
        // } else {
        //         status = onvm_nflib_request_ft(ipv4_hash_params);
        //         if (status < 0) {
        //                 return NULL;
        //         }
        //         hash = rte_hash_find_existing(name);
        // }
        hash = rte_hash_create(ipv4_hash_params);

        rte_free(name);
        if (!hash) {
                return NULL;
        }
        ft = (meili_flow_table *) rte_calloc("table", 1, sizeof(meili_flow_table), 0);
        if (!ft) {
                rte_hash_free(hash);
                return NULL;
        }
        ft->hash = hash;
        ft->cnt = cnt;
        ft->entry_size = entry_size;
        /* Create data array for storing values */
        ft->data = rte_calloc("entry", cnt, entry_size, 0);
        if (!ft->data) {
                rte_hash_free(hash);
                rte_free(ft);
                return NULL;
        }
        return ft;
}

/* Add an entry in flow table and set data to point to the new value.
Returns:
 index in the array on success
 -EPROTONOSUPPORT if packet is not ipv4.
 -EINVAL if the parameters are invalid.
 -ENOSPC if there is no space in the hash for this key.
*/
int
flow_table_add_pkt(meili_flow_table *table, struct rte_mbuf *pkt, char **data) {
        int32_t tbl_index;
        struct ipv4_5tuple key;
        int err;

        err = flow_table_fill_key(&key, pkt);
        if (err < 0) {
                return err;
        }
        tbl_index = rte_hash_add_key_with_hash(table->hash, (const void *)&key, pkt->hash.rss);
        if (tbl_index >= 0) {
                *data = &table->data[tbl_index * table->entry_size];
        }
        return tbl_index;
}

/* Lookup an entry in flow table and set data to point to the value.
   Returns:
    index in the array on success
    -ENOENT if the key is not found.
    -EINVAL if the parameters are invalid.
*/
int
flow_table_lookup_pkt(meili_flow_table *table, struct rte_mbuf *pkt, char **data) {
        int32_t tbl_index;
        struct ipv4_5tuple key;
        int ret;

        ret = flow_table_fill_key(&key, pkt);
        if (ret < 0) {
                return ret;
        }
        tbl_index = rte_hash_lookup_with_hash(table->hash, (const void *)&key, pkt->hash.rss);
        if (tbl_index >= 0) {
                *data = flow_table_get_data(table, tbl_index);
        }
        return tbl_index;
}

/* Removes an entry from the flow table
   Returns:
    A positive value that can be used by the caller as an offset into an array of user data. This value is unique for
   this key, and is the same value that was returned when the key was added.
    -ENOENT if the key is not found.
    -EINVAL if the parameters are invalid.
*/
int32_t
flow_table_remove_pkt(meili_flow_table *table, struct rte_mbuf *pkt) {
        struct ipv4_5tuple key;
        int ret;

        ret = flow_table_fill_key(&key, pkt);
        if (ret < 0) {
                return ret;
        }
        return rte_hash_del_key_with_hash(table->hash, (const void *)&key, pkt->hash.rss);
}

int
flow_table_add_key(meili_flow_table *table, struct ipv4_5tuple *key, char **data) {
        int32_t tbl_index;
        uint32_t softrss;

        softrss = calculate_softrss(key);

        tbl_index = rte_hash_add_key_with_hash(table->hash, (const void *)key, softrss);
        if (tbl_index >= 0) {
                *data = flow_table_get_data(table, tbl_index);
        }

        return tbl_index;
}

int
flow_table_lookup_key(meili_flow_table *table, struct ipv4_5tuple *key, char **data) {
        int32_t tbl_index;
        uint32_t softrss;

        softrss = calculate_softrss(key);

        tbl_index = rte_hash_lookup_with_hash(table->hash, (const void *)key, softrss);
        if (tbl_index >= 0) {
                *data = flow_table_get_data(table, tbl_index);
        }

        return tbl_index;
}

int32_t
flow_table_remove_key(meili_flow_table *table, struct ipv4_5tuple *key) {
        uint32_t softrss;

        softrss = calculate_softrss(key);
        return rte_hash_del_key_with_hash(table->hash, (const void *)key, softrss);
}

/* Iterate through the hash table, returning key-value pairs.
   Parameters:
     key: Output containing the key where current iterator was pointing at
     data: Output containing the data associated with key. Returns NULL if data was not stored.
     next: Pointer to iterator. Should be 0 to start iterating the hash table. Iterator is incremented after each call
   of this function.
   Returns:
     Position where key was stored, if successful.
    -EINVAL if the parameters are invalid.
    -ENOENT if end of the hash table.
 */
int32_t
flow_table_iterate(meili_flow_table *table, const void **key, void **data, uint32_t *next) {
        int32_t tbl_index = rte_hash_iterate(table->hash, key, data, next);
        if (tbl_index >= 0) {
                *data = flow_table_get_data(table, tbl_index);
        }

        return tbl_index;
}

/* Clears a flow table and frees associated memory */
void
flow_table_free(meili_flow_table *table) {
        rte_hash_reset(table->hash);
        rte_hash_free(table->hash);
        rte_free(table->data);
        rte_free(table);
}
#else
#endif