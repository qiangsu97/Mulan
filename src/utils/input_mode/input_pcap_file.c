/*
 * Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
 *
 * This software product is a proprietary product of NVIDIA CORPORATION &
 * AFFILIATES (the "Company") and all right, title, and interest in and to the
 * software product, including all associated intellectual property rights, are
 * and shall remain exclusively with the Company.
 *
 * This software product is governed by the End User License Agreement
 * provided with the software product.
 *
 */
/* Modified by Meili Authors */ 
/* Copyright (c) 2024, Meili Authors */

#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <rte_malloc.h>

#include "input.h"
#include "../utils.h"


// TODO: update pcap read function

// static inline void
// input_pcap_print_snap_len_warning(pl_conf *run_conf)
// {
// 	static bool warning = false;

// 	if (!warning)
// 		MEILI_LOG_WARN_REC(run_conf, "PCAP cap length < packet length. Potential unexpected behaviour.");
// 	warning = true;
// }

// static int
// input_pcap_file_read(pl_conf *run_conf)
// {
// 	const uint32_t len_thres = run_conf->input_len_threshold;
// 	const bool per_pkt_len = run_conf->input_app_mode;
// 	const uint32_t pkts_max = run_conf->input_packets;
// 	const uint32_t bytes_max = run_conf->input_bytes;
// 	const char *file = run_conf->input_file;
// 	struct pcap_pkthdr pkt_header = {};
// 	char errbuf[PCAP_ERRBUF_SIZE];
// 	const unsigned char *pkt_data;
// 	pkt_stats_t *pkt_stats;
// 	pcap_t *pcap_handle;
// 	uint32_t bytes_cnt;
// 	uint32_t pkts_cnt;
// 	uint64_t data_len;
// 	uint32_t pay_len;
// 	uint32_t cpy_len;
// 	uint16_t *lens;
// 	char *data_ptr;
// 	int rte_ptype;
// 	char *data;
// 	int off;
// 	int i;

// 	pcap_handle = pcap_open_offline(file, errbuf);
// 	if (!pcap_handle) {
// 		MEILI_LOG_ERR("Failed to open pcap file: %s.", file);
// 		return -EINVAL;
// 	}

// 	pkt_stats = run_conf->input_pkt_stats;
// 	pkts_cnt = 0;
// 	bytes_cnt = 0;
// 	rte_ptype = 0;
// 	lens = NULL;

// 	/* Read size is min bytes of file size, num pkts and num bytes. */
// 	while ((pkt_data = pcap_next(pcap_handle, &pkt_header)) && (!pkts_max || pkts_cnt < pkts_max) &&
// 	       (!bytes_max || bytes_cnt < bytes_max)) {
// 		if (per_pkt_len) {
// 			off = util_get_app_layer_payload(pkt_data, &pay_len, &rte_ptype);
// 			/* Fails if protocol is unrecognized. */
// 			if (off < 0)
// 				continue;

// 			if (pkt_header.caplen < pkt_header.len)
// 				input_pcap_print_snap_len_warning(run_conf);

// 			if (pkt_header.caplen <= (uint32_t)off)
// 				continue;

// 			if (pkt_header.caplen - off < pay_len)
// 				pay_len = pkt_header.caplen - off;

// 			if (pay_len <= len_thres || pay_len > MAX_REGEX_BUF_SIZE)
// 				continue;

// 			bytes_cnt += pay_len;
// 		} else {
// 			bytes_cnt += pkt_header.caplen;
// 		}

// 		pkts_cnt++;
// 	}

// 	/* Reset the pcap_handle. */
// 	pcap_close(pcap_handle);
// 	pcap_handle = pcap_open_offline(file, errbuf);
// 	if (!pcap_handle) {
// 		MEILI_LOG_ERR("Failed to open pcap file: %s.", file);
// 		return -EINVAL;
// 	}

// 	/* Determine data length as the max bytes or total read in first run. */
// 	bytes_cnt = bytes_max && (bytes_max < bytes_cnt) ? bytes_max : bytes_cnt;

// 	if (!bytes_cnt) {
// 		MEILI_LOG_ERR("No data extracted from PCAP file.");
// 		return -EINVAL;
// 	}

// 	/* Note: for pcap files, it may take up large space */
// 	data = rte_malloc(NULL, bytes_cnt, 0);
// 	if (!data) {
// 		MEILI_LOG_ERR("Failed to allocate memory for pcap file - reduce num bytes or packet input size.");
// 		return -ENOMEM;
// 	}

// 	if (per_pkt_len) {
// 		/* If working packet by packet, create an array of lengths. */
// 		lens = rte_malloc(NULL, sizeof(uint16_t) * pkts_cnt, 0);
// 		if (!lens) {
// 			rte_free(data);
// 			MEILI_LOG_ERR("Memory failure in allocating pcap lengths array.");
// 			return -ENOMEM;
// 		}
// 	}

// 	data_len = bytes_cnt;
// 	data_ptr = data;
// 	bytes_cnt = 0;
// 	i = 0;

// 	while ((pkt_data = pcap_next(pcap_handle, &pkt_header)) && bytes_cnt < data_len) {
// 		cpy_len = pkt_header.caplen;
// 		off = 0;

// 		if (per_pkt_len) {
// 			rte_ptype = 0;
// 			off = util_get_app_layer_payload(pkt_data, &cpy_len, &rte_ptype);
// 			if (off < 0) {
// 				pkt_stats->unsupported_pkts++;
// 				continue;
// 			}

// 			if (cpy_len == 0) {
// 				/* Thres check will catch this but get stats. */
// 				pkt_stats->no_payload++;
// 				continue;
// 			}

// 			if (pkt_header.caplen <= (uint32_t)off) {
// 				pkt_stats->invalid_pkt++;
// 				continue;
// 			}

// 			if (pkt_header.caplen - off < cpy_len)
// 				cpy_len = pkt_header.caplen - off;

// 			if (cpy_len <= len_thres || cpy_len > MAX_REGEX_BUF_SIZE) {
// 				pkt_stats->thres_drop++;
// 				continue;
// 			}

// 			stats_update_pkt_stats(pkt_stats, rte_ptype);

// 		} else {
// 			/* If not parsing packets, consider all valid. */
// 			pkt_stats->valid_pkts++;
// 		}

// 		if (bytes_cnt + cpy_len > data_len)
// 			cpy_len = data_len - bytes_cnt;

// 		memcpy(data_ptr, pkt_data + off, cpy_len);
// 		bytes_cnt += cpy_len;
// 		data_ptr += cpy_len;
// 		if (per_pkt_len)
// 			lens[i++] = cpy_len;
// 	}

// 	run_conf->input_data = data;
// 	run_conf->input_data_len = data_len;
// 	if (per_pkt_len) {
// 		run_conf->input_lens = lens;
// 		run_conf->input_len_cnt = pkts_cnt;
// 	}

// 	return 0;
// }

// static void
// input_pcap_file_clean(pl_conf *run_conf)
// {
// 	rte_free(run_conf->input_data);
// 	rte_free(run_conf->input_lens);
// }

static int
input_pcap_file_read(pl_conf *run_conf){return 0;}

static void
input_pcap_file_clean(pl_conf *run_conf)
{
	;
}

void
input_pcap_file_reg(input_func_t *funcs)
{
	funcs->init = input_pcap_file_read;
	funcs->clean = input_pcap_file_clean;
}
