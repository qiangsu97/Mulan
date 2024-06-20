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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rte_malloc.h>
#include <rte_timer.h>
#include <rte_mbuf.h>
#include <rte_mbuf_dyn.h>

#include "../../lib/log/meili_log.h"
#include "stats.h"

#include "../timestamp/timestamp.h"
#include "../rte_reorder/rte_reorder.h"

#include "../../runtime/meili_runtime.h"
#include "../../packet_ordering/packet_ordering.h"
#include "../../packet_timestamping/packet_timestamping.h"

#define GIGA			1000000000.0
#define MEGA			1000000.0

#define STATS_BANNER_LEN	80
#define STATS_BORDER		"+------------------------------------------------------------------------------+\n"

#define STATS_UPDATE_BANNER_LEN 38
#define STATS_UPDATE_BORDER	"+------------------------------------+"

// static uint64_t split_bytes;
// static uint64_t split_bufs;
// static double split_duration;
static double max_split_perf;
static double max_split_rate;

FILE *log_fp;

/* Print banner of total_length characters with str center aligned. */
static inline void
stats_print_banner(const char *str, int total_length)
{
	int pad_left, pad_right;

	/* Remove one for the | characters at start and end. */
	pad_left = (total_length - strlen(str)) / 2 - 1;
	pad_right = (total_length - strlen(str)) % 2 ? pad_left + 1 : pad_left;

	fprintf(stdout, STATS_BORDER "|%*s%s%*s|\n" STATS_BORDER, pad_left, "", str, pad_right, "");
}

/* Print single column update banner. */
static inline void
stats_print_update_banner(const char *str, int total_length)
{
	int pad_left, pad_right;

	/* Remove one for the | characters at start and end. */
	pad_left = (total_length - strlen(str)) / 2 - 1;
	pad_right = (total_length - strlen(str)) % 2 ? pad_left + 1 : pad_left;

	fprintf(stdout, STATS_UPDATE_BORDER "\n|%*s%s%*s|\n" STATS_UPDATE_BORDER "\n", pad_left, "", str, pad_right,
		"");
}

/* Print update banner containing 2 columns. */
static inline void
stats_print_update_banner2(const char *str, const char *str2, int total_length)
{
	int pad_left, pad_right, pad_left2, pad_right2;

	/* remove one for the | characters at start and end. */
	pad_left = (total_length - strlen(str)) / 2 - 1;
	pad_right = (total_length - strlen(str)) % 2 ? pad_left + 1 : pad_left;
	pad_left2 = (total_length - strlen(str2)) / 2 - 1;
	pad_right2 = (total_length - strlen(str2)) % 2 ? pad_left2 + 1 : pad_left2;

	fprintf(stdout,
		STATS_UPDATE_BORDER "    " STATS_UPDATE_BORDER "\n|%*s%s%*s|    |%*s%s%*s|\n" STATS_UPDATE_BORDER
				    "    " STATS_UPDATE_BORDER "\n",
		pad_left, "", str, pad_right, "", pad_left, "", str2, pad_right2, "");
}

/* Store common stats per queue. */
int
stats_init(pl_conf *run_conf)
{
	const int nq = run_conf->cores;
	rb_stats_t *stats;
	int i, j;

	run_conf->input_pkt_stats = rte_zmalloc(NULL, sizeof(pkt_stats_t), 0);
	if (!run_conf->input_pkt_stats)
		goto err_input_pkt_stats;

	stats = rte_malloc(NULL, sizeof(*stats), 0);
	if (!stats)
		goto err_stats;

	stats->rm_stats = rte_zmalloc(NULL, sizeof(run_mode_stats_t) * nq, 128);
	if (!stats->rm_stats)
		goto err_rm_stats;

	stats->lat_stats = rte_zmalloc(NULL, sizeof(lat_stats_t), 64);
	if (!stats->lat_stats)
		goto err_lat_stats;


	stats->lat_stats->min_lat = UINT64_MAX;
	stats->lat_stats->max_lat = 0;
		
	run_conf->stats = stats;

	/* open a log file if neccessary */
	#ifdef ONLY_SPLIT_THROUGHPUT
	log_fp = fopen("throughput_log_2.txt", "w+");
	if(!log_fp){
		MEILI_LOG_ERR("Open log file failed");
		return -EINVAL;
	}
	#endif

	return 0;


err_lat_stats:
	rte_free(stats->rm_stats);
err_rm_stats:
	rte_free(stats);
err_stats:
	rte_free(run_conf->input_pkt_stats);
err_input_pkt_stats:
	MEILI_LOG_ERR("Memory failure when allocating stats.");

	return -ENOMEM;
}

/* show accelerator type in string */
static const char *
stats_regex_dev_to_str(enum meili_regex_dev dev)
{
	if (dev == REGEX_DEV_DPDK_REGEX)
		return "DPDK Regex";
	if (dev == REGEX_DEV_HYPERSCAN)
		return "Hyperscan";
	if (dev == REGEX_DEV_DOCA_REGEX)
		return "Doca Regex";

	return "-";
}

static const char *
stats_comp_dev_to_str(enum meili_comp_dev dev)
{
	if (dev == COMP_DEV_DPDK_COMP)
		return "DPDK Comp";

	return "-";
}

/* show input type in string format */
static const char *
stats_input_type_to_str(enum rxpbench_input_type input)
{
	if (input == INPUT_PCAP_FILE)
		return "PCAP File";
	if (input == INPUT_TEXT_FILE)
		return "Text File";
	if (input == INPUT_LIVE)
		return "DPDK Live";
	if (input == INPUT_REMOTE_MMAP)
		return "Remote mmap";

	return "-";
}

static void
stats_print_config(pl_conf *run_conf)
{
	const char *dpdk_app_mode;
	const char *rxp_prefixes;
	const char *rxp_latency;
	pkt_stats_t *pkt_stats;
	const char *input_file;
	const char *rules_file;
	const char *hs_single;
	const char *app_mode;
	const char *hs_mode;
	const char *hs_left;
	uint32_t iterations;
	uint32_t buf_length;
	uint32_t rxp_match;
	uint32_t slid_win;
	const char *regex;
	const char *input;
	const char *port1;
	const char *port2;
	uint32_t i;

	pkt_stats = run_conf->input_pkt_stats;
	iterations = run_conf->input_iterations;
	buf_length = run_conf->input_buf_len;
	slid_win = run_conf->sliding_window;

	if (run_conf->input_mode == INPUT_LIVE) {
		buf_length = 0;
		iterations = 0;
	}

	if (run_conf->input_mode == INPUT_PCAP_FILE && run_conf->input_app_mode) {
		app_mode = "True";
		buf_length = 0;
	} else {
		app_mode = "False";
	}

	if (run_conf->regex_dev_type != REGEX_DEV_DOCA_REGEX)
		slid_win = 0;

	regex = stats_regex_dev_to_str(run_conf->regex_dev_type);
	input = stats_input_type_to_str(run_conf->input_mode);

	input_file = run_conf->input_mode != INPUT_LIVE ? run_conf->input_file : "-";
	port1 = run_conf->input_mode == INPUT_LIVE ? run_conf->port1 : "-";
	port2 = run_conf->input_mode == INPUT_LIVE && run_conf->port2 ? run_conf->port2 : "-";
	dpdk_app_mode = run_conf->input_mode == INPUT_LIVE && run_conf->input_app_mode ? "True" : "False";
	rules_file = run_conf->raw_rules_file ? run_conf->raw_rules_file : run_conf->compiled_rules_file;

	/* Trim the file names if more than 52 characters. */
	if (strlen(input_file) > 52) {
		input_file += (strlen(input_file) - 52);
	}

	if (strlen(rules_file) > 52) {
		rules_file += (strlen(rules_file) - 52);
	}

	if (run_conf->regex_dev_type == REGEX_DEV_HYPERSCAN) {
		rxp_prefixes = "-";
		rxp_latency = "-";
		rxp_match = 0;
		hs_mode = "HS_MODE_BLOCK";
		hs_single = run_conf->hs_singlematch ? "True" : "False";
		hs_left = run_conf->hs_leftmost ? "True" : "False";
	} else {
		rxp_prefixes = "N/A";
		rxp_latency = "N/A";
		rxp_match = run_conf->rxp_max_matches;
		hs_mode = "-";
		hs_single = "-";
		hs_left = "-";
	}

	stats_print_banner("CONFIGURATION", STATS_BANNER_LEN);

	fprintf(stdout,
		"|%*s|\n"
		"| - RUN SETTINGS -       %*s|\n"
		"|%*s|\n"
		"| INPUT MODE:         %-56s |\n"
		"| REGEX DEV:          %-56s |\n"
		"| INPUT FILE:         %-56s |\n"
		"| RULES INPUT:        %-56s |\n"
		"|%*s|\n"
		"|%*s|\n"
		"| - DPDK LIVE CONFIG -   %*s|\n"
		"|%*s|\n"
		"| DPDK PRIMARY PORT:  %-16s  "
		"  DPDK SECOND  PORT:  %-16s |\n"
		"| APP LAYER MODE:     %-56s |\n"
		"|%*s|\n"
		"|%*s|\n"
		"| - RUN/SEARCH PARAMS -  %*s|\n"
		"|%*s|\n"
		"| INPUT DURATION:     %-16u  "
		"  BUFFER LENGTH:      %-16u |\n"
		"| INPUT PACKETS:      %-16u  "
		"  BUFFER THRESHOLD:   %-16u |\n"
		"| INPUT BYTES:        %-16u  "
		"  BUFFER OVERLAP:     %-16u |\n"
		"| INPUT ITERATIONS:   %-16u  "
		"  GROUP/BATCH SIZE:   %-16u |\n"
		"| SLIDING WINDOW:     %-56u |\n"
		"|%*s|\n"
		"|%*s|\n"
		"| - PRELOADED DATA INFO -%*s|\n"
		"|%*s|\n"
		"| DATA LENGTH:        %-56lu |\n"
		"| APP LAYER MODE:     %-56s |\n"
		"| VALID PACKETS:      %-16lu  "
		"  VLAN/QNQ:           %-16lu |\n"
		"| INVALID LENGTH:     %-16lu  "
		"  IPV4:               %-16lu |\n"
		"| UNSUPPORTED PROT:   %-16lu  "
		"  IPV6:               %-16lu |\n"
		"| NO PAYLOAD:         %-16lu  "
		"  TCP:                %-16lu |\n"
		"| THRESHOLD DROP:     %-16lu  "
		"  UDP:                %-16lu |\n"
		"|%*s|\n"
		"|%*s|\n"
		"| - REGEX DEVICE CONFIG -%*s|\n"
		"|%*s|\n"
		"| RXP MAX MATCHES:    %-16u  "
		"  HS MODE:            %-16s |\n"
		"| RXP MAX PREFIXES:   %-16s  "
		"  HS SINGLE MATCH:    %-16s |\n"
		"| RXP MAX LATENCY:    %-16s  "
		"  HS LEFTMOST MATCH:  %-16s |\n"
		"|%*s|\n"
		"|%*s|\n"
		"| - REGEX COMPILATION -  %*s|\n"
		"|%*s|\n"
		"| SINGLE LINE:        %-16s  "
		"  CASE INSENSITIVE:   %-16s |\n"
		"| MULTI-LINE:         %-16s  "
		"  FREE SPACE:         %-16s |\n"
		"| FORCE COMPILE:      %-56s |\n"
		"|%*s|\n"
		"|%*s|\n"
		"| - PERFORMANCE CONFIG - %*s|\n"
		"|%*s|\n"
		"| NUMBER OF CORES:    %-56u |\n"
		"|%*s|\n",
		78, "", 54, "", 78, "", input, regex, input_file, rules_file, 78, "", 78, "", 54, "", 78, "", port1,
		port2, dpdk_app_mode, 78, "", 78, "", 54, "", 78, "", run_conf->input_duration, buf_length,
		run_conf->input_packets, run_conf->input_len_threshold, run_conf->input_bytes, run_conf->input_overlap,
		iterations, run_conf->input_batches, slid_win, 78, "", 78, "", 54, "", 78, "", run_conf->input_data_len,
		app_mode, pkt_stats->valid_pkts, pkt_stats->vlan + pkt_stats->qnq, pkt_stats->invalid_pkt,
		pkt_stats->ipv4, pkt_stats->unsupported_pkts, pkt_stats->ipv6, pkt_stats->no_payload, pkt_stats->tcp,
		pkt_stats->thres_drop, pkt_stats->udp, 78, "", 78, "", 54, "", 78, "", rxp_match, hs_mode, rxp_prefixes,
		hs_single, rxp_latency, hs_left, 78, "", 78, "", 54, "", 78, "",
		run_conf->single_line ? "True" : "False", run_conf->caseless ? "True" : "False",
		run_conf->multi_line ? "True" : "False", run_conf->free_space ? "True" : "False",
		run_conf->force_compile ? "True" : "False", 78, "", 78, "", 54, "", 78, "", run_conf->cores, 78, "");

	/* Report warnings if any exist. */
	if (run_conf->no_conf_warnings) {
		fprintf(stdout,
			"|%*s|\n"
			"| - CONFIG WARNINGS -    %*s|\n"
			"|%*s|\n",
			78, "", 54, "", 78, "");
		for (i = 0; i < run_conf->no_conf_warnings; i++)
			fprintf(stdout, "| * %-74s |\n", run_conf->conf_warning[i]);
		fprintf(stdout, "|%*s|\n", 78, "");
	}

	fprintf(stdout, STATS_BORDER "\n");
}


#ifndef ONLY_SPLIT_THROUGHPUT
static inline void
stats_print_update_single(run_mode_stats_t *rm1, run_mode_stats_t *rm2, bool total, double duration)
{
	double perf, split_perf, split_rate;

	double perf1, perf2;
	double rate1, rate2;
	double split_perf1, split_perf2;
	double split_rate1, split_rate2;

	char type1[24];
	char type2[24];

	char core1[24];
	char core2[24];
	sprintf(core1, "CORE %02d", rm1->lcore_id);
	GET_STAGE_TYPE_STRING(rm1->self->type, type1);
	if (!rm2) {
		stats_print_update_banner(core1, STATS_UPDATE_BANNER_LEN);
	} else {
		sprintf(core2, "CORE %02d", rm2->lcore_id);
		GET_STAGE_TYPE_STRING(rm2->self->type, type2);
		stats_print_update_banner2(core1, core2, STATS_UPDATE_BANNER_LEN);
	}


	if (!rm2) {
		if(!total){
			fprintf(stdout,"| Stage Type:   %20s |\n",type1);
		}
		
		perf1 = ((rm1->tx_buf_bytes * 8) / duration) / GIGA;
		split_perf1 = (((rm1->tx_buf_bytes - rm1->split_tx_buf_bytes) * 8) / (duration - rm1->split_duration)) / GIGA;
		rate1 = ((rm1->tx_buf_cnt) / duration) / MEGA;
		split_rate1 = ((rm1->tx_buf_cnt - rm1->split_tx_buf_cnt) / (duration - rm1->split_duration)) / MEGA;
		
		rm1->split_tx_buf_bytes = rm1->tx_buf_bytes;
		rm1->split_tx_buf_cnt = rm1->tx_buf_cnt;	
		rm1->split_duration = duration;	

		fprintf(stdout,			
			"| Perf Total (Gbps):  %14.4f |\n"
			"| Perf Total (Mpps):  %14.4f |\n"
			"| Perf Split (Gbps):  %14.4f |\n"
			"| Perf Split (Mpps):  %14.4f |\n",
			perf1, rate1, split_perf1, split_rate1);


		if (total) {
			fprintf(stdout,
				"|%*s|\n"
				"| Duration:     %20.4f |\n"
				"| Regex Perf (total): %14.4f |\n"
				"| Regex Perf (split): %14.4f |\n",
				STATS_UPDATE_BANNER_LEN - 2, "", duration, perf, split_perf);
		}
		fprintf(stdout, STATS_UPDATE_BORDER "\n\n");
	} 
	else {
		if(!total){
			fprintf(stdout,"| Stage Type:   %20s |    | Stage Type:   %20s |\n", type1, type2);
		}
		perf1 = ((rm1->tx_buf_bytes * 8) / duration) / GIGA;
		split_perf1 = (((rm1->tx_buf_bytes - rm1->split_tx_buf_bytes) * 8) / (duration - rm1->split_duration)) / GIGA;
		rate1 = ((rm1->tx_buf_cnt) / duration) / MEGA;
		split_rate1 = ((rm1->tx_buf_cnt - rm1->split_tx_buf_cnt) / (duration - rm1->split_duration)) / MEGA;

		
		rm1->split_tx_buf_bytes = rm1->tx_buf_bytes;
		rm1->split_tx_buf_cnt = rm1->tx_buf_cnt;
		rm1->split_duration = duration;
		
		perf2 = ((rm2->tx_buf_bytes * 8) / duration) / GIGA;
		split_perf2 = (((rm2->tx_buf_bytes - rm2->split_tx_buf_bytes) * 8) / (duration - rm2->split_duration)) / GIGA;
		rate2 = ((rm2->tx_buf_cnt) / duration) / MEGA;
		split_rate2 = ((rm2->tx_buf_cnt - rm2->split_tx_buf_cnt) / (duration - rm2->split_duration)) / MEGA;
		
		rm2->split_tx_buf_bytes = rm2->tx_buf_bytes;
		rm2->split_tx_buf_cnt = rm2->tx_buf_cnt;
		rm2->split_duration = duration;

		fprintf(stdout,
			"| Perf Total (Gbps):  %14.4f |    | Perf Total (Gbps):  %14.4f |\n"
			"| Perf Total (Mpps):  %14.4f |    | Perf Total (Mpps):  %14.4f |\n"
			"| Perf Split (Gbps):  %14.4f |    | Perf Split (Gbps):  %14.4f |\n"
			"| Perf Split (Mpps):  %14.4f |    | Perf Split (Mpps):  %14.4f |\n"
			STATS_UPDATE_BORDER "    " STATS_UPDATE_BORDER "\n\n",
			perf1, perf2, rate1, rate2, split_perf1, split_perf2, split_rate1, split_rate2);
	}
}
#else
static inline void
stats_print_update_single(run_mode_stats_t *rm1, run_mode_stats_t *rm2, bool total, double duration)
{
	double perf, split_perf, split_rate;

	double perf1, perf2;
	double rate1, rate2;
	double split_perf1, split_perf2;
	double split_rate1, split_rate2;

	char type1[24];
	char type2[24];

	
	char core1[24];
	char core2[24];
	sprintf(core1, "CORE %02d", rm1->lcore_id);
	//printf("%d\n", rm1->self->type);
	GET_STAGE_TYPE_STRING(rm1->self->type, type1);
	

	if (rm1->self->type == PL_MAIN) {
		
		
		perf1 = ((rm1->tx_buf_bytes * 8) / duration) / GIGA;
		split_perf1 = (((rm1->tx_buf_bytes - rm1->split_tx_buf_bytes) * 8) / (duration - rm1->split_duration)) / GIGA;
		rate1 = ((rm1->tx_buf_cnt) / duration) / MEGA;
		split_rate1 = ((rm1->tx_buf_cnt - rm1->split_tx_buf_cnt) / (duration - rm1->split_duration)) / MEGA;
		
		rm1->split_tx_buf_bytes = rm1->tx_buf_bytes;
		rm1->split_tx_buf_cnt = rm1->tx_buf_cnt;	
		rm1->split_duration = duration;	

		//fprintf(stdout,	"%14.4f",split_perf1);
		fprintf(log_fp,	"%14.4f\n",split_perf1);
	} 
}
#endif


void
stats_print_update(rb_stats_t *stats, int num_queues, double time, bool end)
{
	run_mode_stats_t total_rm;

	memset(&total_rm, 0, sizeof(run_mode_stats_t));

	run_mode_stats_t *rm_stats = stats->rm_stats;
	int i;

	/* Clear terminal and move cursor to (0, 0). */
	// fprintf(stdout, "\033[2J");
	// fprintf(stdout, "\033[%d;%dH", 0, 0);

	if (end)
		stats_print_banner("END OF RUN PER QUEUE STATS", STATS_BANNER_LEN);
	#ifndef ONLY_SPLIT_THROUGHPUT
	else
		stats_print_banner("SPLIT PER QUEUE STATS", STATS_BANNER_LEN);
	fprintf(stdout, "\n");
	#endif

	
	for (i = 0; i < num_queues; i++) {
		// total_rm.rx_buf_bytes += rm_stats[i].rx_buf_bytes;
		// total_rm.rx_buf_cnt += rm_stats[i].rx_buf_cnt;
		// total_rm.tx_buf_bytes += rm_stats[i].tx_buf_bytes;
		// total_rm.tx_buf_cnt += rm_stats[i].tx_buf_cnt;

		//printf("i=%d, type=%d\n",i , rm_stats[i].self->type);
		/* Only print on even queue numbers. */
		if (!(i % 2) && i + 1 < num_queues)
			stats_print_update_single(&rm_stats[i], &rm_stats[i + 1],false, time);
		else if (!(i % 2))
			stats_print_update_single(&rm_stats[i], NULL, false, time);
		// TODO: we should add custom print inside of stats_print_single
	}

	/* print stats collected from core 0 */
	total_rm.rx_buf_bytes = rm_stats[0].rx_buf_bytes;
	total_rm.rx_buf_cnt = rm_stats[0].rx_buf_cnt;
	total_rm.tx_buf_bytes = rm_stats[0].tx_buf_bytes;
	total_rm.tx_buf_cnt = rm_stats[0].tx_buf_cnt;
	// currently we assume there is only one core for TO
	//stats_print_update_single(&total_rm, , NULL, true, time);
}

int cmpfunc (const void * a, const void * b)
{
   return ( *(int*)a - *(int*)b );
}



static void
stats_print_lat(rb_stats_t *stats, int num_queues, enum meili_regex_dev dev __rte_unused, uint32_t batches, bool lat_mode)
{
	lat_stats_t *lat_stats = stats->lat_stats;
	run_mode_stats_t *run_stats = stats->rm_stats;

	lat_stats_t lat_total;
	uint64_t total_bufs;
	int i;

	int nb_samples = 0;

	memset(&lat_total, 0, sizeof(lat_total));
	lat_total.min_lat = UINT64_MAX;
	lat_total.max_lat = 0;
	total_bufs = 0;

	
	nb_samples = RTE_MIN(lat_stats->nb_sampled, NUMBER_OF_SAMPLE);
	/* sort time_diff_sample */
	qsort(lat_stats->time_diff_sample,nb_samples,sizeof(uint64_t),cmpfunc);

	total_bufs = run_stats->tx_buf_cnt;

	lat_total.tot_lat = lat_stats->tot_lat;
	lat_total.tot_in_lat = lat_stats->tot_in_lat;
	if (lat_stats->min_lat < lat_total.min_lat)
		lat_total.min_lat = lat_stats->min_lat;
	if (lat_stats->max_lat > lat_total.max_lat)
		lat_total.max_lat = lat_stats->max_lat;


	/* Get per core average for some of the total stats. */
	lat_total.tot_lat = total_bufs ? lat_total.tot_lat / total_bufs : 0;
	lat_total.tot_in_lat = total_bufs ? lat_total.tot_in_lat / total_bufs : 0;
	if (lat_total.min_lat == UINT64_MAX)
		lat_total.min_lat = 0;


	stats_print_banner("PACKET LATENCY STATS", STATS_BANNER_LEN);

	if (!lat_mode)
		fprintf(stdout,
			"| ** NOTE: NOT RUNNING IN LATENCY MODE (CAN TURN ON WITH --latency-mode) **    |\n"
			"|%*s|\n",
			78, "");

	
	/* only print tail lat from queue 0 */
	int tail_90_index = (int)(nb_samples*90)/100 ;
	int tail_95_index = (int)(nb_samples*95)/100 ;
	int tail_99_index = (int)(nb_samples*99)/100 ;
	int tail_999_index = (int)(nb_samples*999)/1000 ;
	fprintf(stdout,
		"| PER PACKET LATENCY (usecs) 						       |\n"
		//"| - BATCH SIZE:  		          %-42.4u |\n"
		"| - # OF TOTAL PACKETS:             %-42.4lu |\n"
		"| - # OF SAMPLES FOR TAIL:          %-42.4u |\n"
		"| - MAX LATENCY:                    %-42.4f |\n"
		"| - MIN LATENCY:                    %-42.4f |\n"
		"| - AVERAGE LATENCY:                %-42.4f |\n"
		"| - 90th TAIL LATENCY:              %-42.4f |\n"
		"| - 95th TAIL LATENCY:              %-42.4f |\n"
		"| - 99th TAIL LATENCY:              %-42.4f |\n"
		"| - 99.9th TAIL LATENCY:            %-42.4f |\n"
		"| - AVERAGE QUEUING LATENCY(TX):    %-42.4f |\n"
		"|%*s|\n",
		//batches, 
		total_bufs,nb_samples,(double)lat_total.max_lat / rte_get_timer_hz() * 1000000.0,
		(double)lat_total.min_lat / rte_get_timer_hz() * 1000000.0,
		(double)lat_total.tot_lat / rte_get_timer_hz() * 1000000.0, 
		(double)lat_stats->time_diff_sample[tail_90_index] / rte_get_timer_hz() * 1000000.0, 
		(double)lat_stats->time_diff_sample[tail_95_index] / rte_get_timer_hz() * 1000000.0,
		(double)lat_stats->time_diff_sample[tail_99_index] / rte_get_timer_hz() * 1000000.0, 
		(double)lat_stats->time_diff_sample[tail_999_index] / rte_get_timer_hz() * 1000000.0,
		(double)lat_total.tot_in_lat / rte_get_timer_hz() * 1000000.0,
		78, "");

		// /* print latency breakdown */
		// for (i = 1; i < num_queues; i++) {
			
		// 	lat_stats = (rxp_stats_t *)regex_stats[i].custom;	
		// 	lat_total.tot_lat = total_bufs ? lat_stats->tot_lat / total_bufs : 0;
		// 	lat_total.tot_in_lat = total_bufs ? lat_stats->tot_in_lat / total_bufs : 0;

		// fprintf(stdout,
		// 	"| - CORE:                           %-42d |\n"
		// 	"| - AVERAGE PROCESSING LATENCY:     %-42.4f |\n"
		// 	"| - AVERAGE QUEUING LATENCY(RX):    %-42.4f |\n"
		// 	"|%*s|\n",
		// 	run_stats[i].lcore_id,
		// 	(double)lat_total.tot_lat / rte_get_timer_hz() * 1000000.0, 
		// 	(double)lat_total.tot_in_lat / rte_get_timer_hz() * 1000000.0,
		// 	78, "");
		// }
		fprintf(stdout, STATS_BORDER "\n");

		// for(int k=0;k<100;k++){
		// 	printf("%f\n",(double)lat_stats->time_diff_sample[k]/ rte_get_timer_hz() * 1000000.0);
		// }

		for(int k=tail_99_index; k<nb_samples; k++){
		//for(int k=0; k<nb_samples; k++){
			printf("%f\n",(double)lat_stats->time_diff_sample[k] / rte_get_timer_hz() * 1000000.0);
		}

}


void
stats_print_end_of_run(pl_conf *run_conf, double run_time)
{
	rb_stats_t *stats = run_conf->stats;

	stats_print_update(stats, run_conf->cores, run_time, true);
	stats_print_lat(stats, run_conf->cores, run_conf->regex_dev_type, run_conf->input_batches, run_conf->latency_mode);
	// stats_print_config(run_conf);
	// stats_print_common_stats(stats, run_conf->cores, run_time);

	/* print regex related statistics */
	/* TODO: should store regex stats to regex module */
	//stats_print_custom(run_conf, stats, run_conf->cores);
	/* print pipeline latency information */
	
}

void 
stats_update_time_main(struct rte_mbuf **mbuf, int nb_mbuf, struct pipeline *pl)
{
	pl_conf *run_conf = &pl->conf;
	lat_stats_t *lat_stats = run_conf->stats->lat_stats;

	uint64_t time_diff;
	uint64_t time_end, time_start;

	uint32_t seq_num;
	int sample;
    int sample_index;

	int offset1;
	int offset2;

    struct pipeline_stage *self;
    struct pipeline_stage *parent;

	// struct pkt_ts_state *mystate1 = (struct pkt_ts_state *)pl->ts_start_stage.state;
	// struct pkt_ts_state *mystate2 = (struct pkt_ts_state *)pl->ts_end_stage.state;

    
	for(int i=0; i<nb_mbuf; i++){
		/* Calculate and store latency of packet through HW. */
		//time_mbuf = util_get_64_bit_from_2_32(&mbuf->dynfield1[DF_TIME_HIGH]);

		// OR
        offset1 = pl->ts_start_offset;
	    offset2 = pl->ts_end_offset;

		time_start  = *(RTE_MBUF_DYNFIELD(mbuf[i], offset1, uint64_t *));
		time_end = *(RTE_MBUF_DYNFIELD(mbuf[i], offset2, uint64_t *));
		
		time_diff = (time_end - time_start);

		lat_stats->tot_lat += time_diff;
		if (time_diff < lat_stats->min_lat)
			lat_stats->min_lat = time_diff;
		if (time_diff > lat_stats->max_lat)
			lat_stats->max_lat = time_diff;
        
        #ifdef PKT_LATENCY_SAMPLE_ON
        lat_stats->time_diff_sample[lat_stats->nb_sampled & NUMBER_OF_SAMPLE] = time_diff;
        lat_stats->nb_sampled++;  
        #endif
        
    //     #ifdef PKT_LATENCY_BREAKDOWN_ON
    //     /* record breakdown latency of pipeline stages */
    //     // seq_num = *rte_reorder_seqn(mbuf[i]);
	// 	// sample = seq_num % NUMBER_OF_SAMPLE;
    //     /* pl's tot in latency, temporarily used for queuing latency between last stage and final processing */
    //     self = pl->stages[pl->nb_pl_stages-1][0];
    //     // mystate1 = (struct pkt_ts_state *)self->ts_start_stage.state;
    //     // mystate2 = (struct pkt_ts_state *)pl->ts_end_stage.state;
    //     offset1 = self->ts_end_offset;
    //     offset2 = pl->ts_end_offset;
    //     time_start  = *(RTE_MBUF_DYNFIELD(mbuf[i], offset1, uint64_t *));
    //     time_end = *(RTE_MBUF_DYNFIELD(mbuf[i], offset2, uint64_t *));
        
    //     time_diff = (time_end - time_start);

    //     lat_stats->tot_in_lat += time_diff;

    //     /* traverse pl stages */
    //     // TODO: only register one dynfield for each layer of pipeline, instead of each instance
    //     for(int j=0; j<pl->nb_pl_stages; j++){
    //         for(int k=0; k<pl->nb_inst_per_pl_stage[j]; k++){
    //             // if(i=0){
    //             //     printf("self->worker_qid = %d\n",self->worker_qid);
    //             // }
    //                 self = pl->stages[j][k];
    //             	stats = &run_conf->stats->regex_stats[self->worker_qid];
	//                 lat_stats = (rxp_stats_t *)stats->custom;
    //                 //mystate1 = (struct pkt_ts_state *)self->ts_start_stage.state;

    //                 /* self's tot in latency */
    //                 if(j==0){
    //                     //mystate2 = (struct pkt_ts_state *)pl->ts_start_stage.state;
    //                     offset2 = pl->ts_start_offset;
    //                 }
    //                 else{
    //                     parent = pl->stages[j-1][0];
    //                     //mystate2 = (struct pkt_ts_state *)parent->ts_end_stage.state;
    //                     offset2 = parent->ts_end_offset;
    //                 }
    //                 offset1 = self->ts_start_offset;
	                
    //                 time_end  = *(RTE_MBUF_DYNFIELD(mbuf[i], offset1, uint64_t *));
    //                 time_start = *(RTE_MBUF_DYNFIELD(mbuf[i], offset2, uint64_t *));
                    
    //                 time_diff = (time_end - time_start);

    //                 lat_stats->tot_in_lat += time_diff;

    //                 /* self's tot latency */
    //                 //mystate2 = (struct pkt_ts_state *)self->ts_end_stage.state;
    //                 offset2 = self->ts_end_offset;
    //                 time_start = *(RTE_MBUF_DYNFIELD(mbuf[i], offset1, uint64_t *));
    //                 time_end = *(RTE_MBUF_DYNFIELD(mbuf[i], offset2, uint64_t *));
                    
    //                 time_diff = (time_end - time_start);

    //                 lat_stats->tot_lat += time_diff;
    //         }
    //     }   
    // #endif 
	}

}


void
stats_clean(pl_conf *run_conf)
{
	rb_stats_t *stats = run_conf->stats;
	rte_free(stats->rm_stats);
	rte_free(stats);
	rte_free(run_conf->input_pkt_stats);
}
