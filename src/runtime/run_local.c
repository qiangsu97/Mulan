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

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>

//#include "./regex/regex_dev.h"

#include "../utils/utils.h"
#include "../packet_ordering/packet_ordering.h"
#include "../packet_timestamping/packet_timestamping.h"

#include "run_mode.h"
#include "pipeline.h"


// TODO: clean the code

// #define NB_PRELOADED_BUF (1<<8)  /* should be 2^n */  

// /* for ipv4 pkt header */
// // #define UDP_SAMPLE_SRC_PORT 12345
// // #define UDP_SAMPLE_DST_PORT 54321
// // #define IPV4_SAMPLE_SRC 
// // #define IPV4_SAMPLE_DST (uint32_t) RTE_IPV4(10, 0, 0, 2)

// //#define IP_LIST

// #ifdef IP_LIST
// #define NB_IP_GROUP_MAX 9

// uint32_t ipv4_src_list[NB_IP_GROUP_MAX] = {
// 	RTE_IPV4(100, 255, 127, 63),
// 	RTE_IPV4(240, 10, 10, 78),
// 	RTE_IPV4(25, 17, 109, 130),
// 	RTE_IPV4(24, 186, 208, 16),
// 	RTE_IPV4(24,75,104,242),
// 	RTE_IPV4(93,89,182,53),
// 	RTE_IPV4(146,237,48,72),	
// 	RTE_IPV4(182,237,87,191),
// 	RTE_IPV4(164,92,25,106)
// };

// uint32_t ipv4_dst_list[NB_IP_GROUP] = {
// 	RTE_IPV4(100, 74, 45, 2),
// 	RTE_IPV4(100, 74, 45, 2),
// 	RTE_IPV4(52, 118, 247, 18),
// 	RTE_IPV4(181, 111, 89, 124),
// 	RTE_IPV4(52,118,247,18),
// 	RTE_IPV4(216,69,128,2),
// 	RTE_IPV4(52,118,196,102),
// 	RTE_IPV4(52,118,247,19),
// 	RTE_IPV4(52,118,247,19)
// };

// #else

// /* preset one */
// #define IPV4_SA_START (uint32_t) RTE_IPV4(10, 0, 0, 0)
// #define IPV4_SA_END (uint32_t) RTE_IPV4(10, 0, 0, 255)

// #define IPV4_DA_START (uint32_t) RTE_IPV4(192, 0, 0, 0)
// #define IPV4_DA_END (uint32_t) RTE_IPV4(192, 0, 0, 255) 

// #endif

// #define IPV4_SP_START 12345
// #define IPV4_SP_END 12345

// #define IPV4_DP_START 54321
// #define IPV4_DP_END 54321

// #define IPV4_PROTO IP_PROTOCOL_UDP

// /* other presets... */


// struct file_state{
// 	char *data;
// 	uint32_t data_len; 		/* total length of input file */
// 	uint32_t data_off;		/* current position of input file */
// 	uint32_t max_buf_len;
// 	uint32_t len_cnt;		/* current position of lens table */
// 	uint32_t total_lens;	/* total number of lengths in length table */
// 	uint16_t *lens;			/* length table */
// 	uint32_t overlap;		/* buffer overlap from previous frame */
// 	uint32_t max_iter; 
// 	uint32_t iter_cnt;
// 	bool file_done;			/* file done flag */

// 	int src_index;


// 	/* mbuf related */
// 	struct rte_mempool *mbuf_pool;
// 	struct rte_mbuf *preloaded_mbuf[NB_PRELOADED_BUF];
// 	int nb_preloaded_buf;
// 	int nb_consumed_buf;
// 	struct rte_mbuf_ext_shared_info *shinfo;

// };


// int prepare_file_mbufs(struct file_state *fs){
// 	uint32_t buf_len = 0;

// 	const uint32_t data_len = fs->data_len;
// 	const uint32_t total_lens = fs->total_lens;
// 	const uint32_t max_buf_len = fs->max_buf_len;
// 	const uint32_t overlap = fs->overlap;
// 	fs->nb_preloaded_buf = 0;
// 	fs->nb_consumed_buf = 0;
// 	fs->file_done = false;

// 	srand(time(NULL));
// 	struct meili_pkt info;
// 	memset(&info, 0x00, sizeof(struct meili_pkt));
// 	/* single flow */

// 	char *mbuf_data;


	

// 	//bool push_buf = false;

// 	/* read packets until  */
// 	while(fs->nb_preloaded_buf < NB_PRELOADED_BUF ){
// 		// && !fs->file_done
// 		/* manual packet lengths take priority. */
// 		/* set value for push_buf flag */
// 		if (total_lens) {
// 			/* this is manual lengths, user-defined pkt lengths are stored in lens, and the # of user-defined lengths are stored in total lengths */
// 			buf_len = fs->lens[fs->len_cnt];
// 			fs->len_cnt++;

// 			if (fs->len_cnt == total_lens) {
// 				fs->file_done = true;
// 			}
// 		} 
// 		else if (fs->data_off + max_buf_len >= data_len) {
// 			/* reach input file length */
// 			buf_len = data_len - fs->data_off; // last buffer truncated
// 			fs->file_done = true;
// 		} 
// 		else{
// 			buf_len = max_buf_len;
// 		}

// 		/* allocate a buffer */

// 		fs->preloaded_mbuf[fs->nb_preloaded_buf] = rte_pktmbuf_alloc(fs->mbuf_pool);
// 		if (!fs->preloaded_mbuf[fs->nb_preloaded_buf]) {
// 			MEILI_LOG_ERR("Failed to get mbuf from pool.");
// 			return -ENOMEM;
// 		}
// 		//rte_pktmbuf_attach_extbuf(fs->preloaded_mbuf[fs->nb_preloaded_buf], &fs->data[fs->data_off], 0, buf_len, fs->shinfo);
// 		mbuf_data = rte_pktmbuf_append(fs->preloaded_mbuf[fs->nb_preloaded_buf], buf_len);
		
// 		if(mbuf_data){
// 			rte_memcpy(mbuf_data, &fs->data[fs->data_off], buf_len);
// 		}
// 		else{
// 			MEILI_LOG_ERR("Append payload failed");
// 		}
		
// 		if(fs->nb_preloaded_buf == 1){
// 			printf("data_len=%d, pkt_len = %d\n", fs->preloaded_mbuf[fs->nb_preloaded_buf]->data_len, fs->preloaded_mbuf[fs->nb_preloaded_buf]->pkt_len);
// 		}
		
// 		// fs->preloaded_mbuf[fs->nb_preloaded_buf]->data_len = buf_len;
// 		// fs->preloaded_mbuf[fs->nb_preloaded_buf]->pkt_len = buf_len;


		
// 		#ifdef IP_LIST 
// 			info.sa = ipv4_src_list[fs->src_index % NB_IP_GROUP];
// 			info.da = ipv4_dst_list[fs->src_index % NB_IP_GROUP];
// 		#else
// 			info.sa = IPV4_SA_START + fs->src_index;
// 			// currently only change source addr
// 			info.da = IPV4_DA_START;
// 		#endif
		
// 		info.sp = IPV4_SP_START;
// 		info.dp = IPV4_DP_START;
// 		info.proto = IPV4_PROTO;
// 		info.length = buf_len;

// 		fs->src_index ++;
// 		//printf("info.sa=%x\n",info.sa);
// 		if(info.sa >= IPV4_SA_END){
// 			fs->src_index = 0;
// 		}

		

// 		/* add an ipv4 hdr */
// 		add_udp_hdr(fs->preloaded_mbuf[fs->nb_preloaded_buf], &info);


// 		fs->nb_preloaded_buf++;
// 		fs->data_off += buf_len;
// 		fs->data_off -= overlap;

// 		/* file done, read from beginning */
// 		if(fs->file_done){
// 			fs->iter_cnt++;
// 			fs->data_off = 0;
// 			fs->len_cnt = 0;
// 			fs->file_done = false;
// 		}
		
// 	}
// 	return 0;	
// }

// int rte_file_rx_burst(struct file_state *fs, struct rte_mbuf **mbuf ,int batch_size){
// 	uint32_t buf_len = 0;
// 	uint32_t batch_cnt = 0;

// 	const uint32_t data_len = fs->data_len;
// 	const uint32_t total_lens = fs->total_lens;
// 	const uint32_t max_buf_len = fs->max_buf_len;
// 	const uint32_t overlap = fs->overlap;

// 	bool push_buf = false;

// 	/* read packets until a batch has been read */
// 	while(!force_quit 
// 	//&& fs->iter_cnt < fs->max_iter 
// 	//&& (!max_cycles || cycles <= max_cycles)
// 	&& push_buf == false){
// 		/* manual packet lengths take priority. */
// 		/* set value for push_buf flag */
// 		mbuf[batch_cnt] = fs->preloaded_mbuf[fs->nb_consumed_buf];

// 		fs->nb_consumed_buf ++;
// 		fs->nb_consumed_buf &= NB_PRELOADED_BUF - 1;
// 		// debug
// 		//printf("using buf %d\n",fs->nb_consumed_buf);


// 		/* when batch_cnt reaches batch_size, push this batch */
// 		if (++batch_cnt == batch_size) {
// 			push_buf = true;	
// 		}
		
// 	}
// 	return batch_cnt;
// }


// /* read data from preloaded file */
// static int
// run_local(struct pipeline *pl)
// {
// 	pl_conf *run_conf = &pl->conf;
// 	/* always run on main core */
// 	int qid=0;
// 	const uint32_t max_duration = run_conf->input_duration;
// 	uint32_t batch_size = run_conf->input_batches;
// 	uint32_t batch_size_in = 0;
// 	uint32_t batch_size_out = 0;
// 	int batch_cnt = 0;
// 	rb_stats_t *stats = run_conf->stats;
// 	run_mode_stats_t *rm_stats = &stats->rm_stats[qid];
// 	// TODO: rename regex_stats_t to other names
// 	//regex_stats_t *regex_stats = &stats->regex_stats[qid];

// 	struct file_state fs;

// 	fs.data_off = 0;
// 	fs.len_cnt = 0;
// 	fs.iter_cnt = 0;
// 	fs.max_buf_len = run_conf->input_buf_len;
// 	fs.max_iter = run_conf->input_iterations;
// 	fs.total_lens = run_conf->input_len_cnt;
// 	fs.data_len = run_conf->input_data_len;
// 	fs.overlap = run_conf->input_overlap;
// 	fs.lens = run_conf->input_lens;
// 	fs.data = run_conf->input_data;
// 	fs.src_index = 0;

// 	fs.mbuf_pool = pl->mbuf_pool;
// 	fs.shinfo = &(run_conf->shinfo);

// 	/* time keeping */
// 	uint64_t prev_cycles;
// 	uint64_t max_cycles;
// 	uint64_t cycles;
// 	double run_time;
// 	uint64_t start;
// 	double rate = 0;

// 	bool main_lcore;
	
// 	int ret;
// 	int flag = 0;

// 	/* special pipeline stages */
// 	// for baseline implementation, each stage can only use one core 
// 	/* stages have already been init in pipeline_init */
// 	struct pipeline_stage *seq_stage = &pl->seq_stage;
// 	struct pipeline_stage *reorder_stage = &pl->reorder_stage;

// 	struct rte_mbuf *mbuf_in[MAX_PKTS_BURST];
// 	struct rte_mbuf *mbuf[MAX_PKTS_BURST];
// 	//struct rte_mbuf *mbuf_out[MAX_PKTS_BURST];
// 	struct rte_mbuf **mbuf_out;
// 	int nb_enq;
// 	int to_enq;
// 	int tot_enq;

// 	int nb_deq;

// 	int ring_in_index = 0;
// 	int ring_out_index = 0;
// 	int nb_first_stage = pl->nb_pl_stages? pl->nb_inst_per_pl_stage[0]:1;
// 	int nb_last_stage = pl->nb_pl_stages? pl->nb_inst_per_pl_stage[pl->nb_pl_stages-1]:1;

	
// 	/* Keep coverity check happy by initialising. */
// 	memset(&mbuf[0], '\0', sizeof(struct rte_mbuf *) * batch_size);
// 	//memset(&mbuf_out[0], '\0', sizeof(struct rte_mbuf *) * batch_size);
// 	/* Convert duration to cycles. */
// 	max_cycles = max_duration * rte_get_timer_hz();
// 	prev_cycles = 0;
// 	cycles = 0;

// 	main_lcore = rte_lcore_id() == rte_get_main_lcore();



// 	// if (main_lcore){
// 	// 	stats_print_update(stats, run_conf->cores, 0.0, false);
// 	// }
		
// 	/* several different versions of batch size */
// 	//batch_size_in = 1;
// 	// batch_size_in = nb_first_stage? batch_size * nb_first_stage:batch_size;
// 	batch_size_in = batch_size;

// 	batch_size_out = MAX_PKTS_BURST;
// 	//batch_size_out = batch_size * nb_last_stage;
// 	//batch_size_out = batch_size;

// 	prepare_file_mbufs(&fs);

// 	start = rte_rdtsc();

// 	MEILI_LOG_INFO("batch_size_in = %d, batch_size_out = %d",batch_size_in, batch_size_out);

// 	/* start reading packets */
// 	while (!force_quit 
// 			//&& fs.iter_cnt < fs.max_iter 
// 			&& (!max_cycles || cycles <= max_cycles)) 
// 		{
// 			batch_cnt = 0;
			
// 			#ifdef RATE_LIMIT_BPS_ON
// 			/* limit the loading speed based on bps */
// 			run_time = (double)cycles / rte_get_timer_hz();
// 			rate = ((rm_stats->rx_buf_bytes * 8) / run_time)/ 1000000000.0;
// 			if(rate < RATE_Gbps){
// 				batch_cnt = rte_file_rx_burst(&fs, mbuf_in, batch_size_in);
// 			}
// 			else{;}
// 			#elif defined(RATE_LIMIT_PPS_ON)
// 			/* limit the loading speed based on pps */
// 			run_time = (double)cycles / rte_get_timer_hz();
// 			rate = ((rm_stats->rx_buf_cnt ) / run_time)/ 1000000.0;
// 			if(rate < RATE_Mpps){
// 				batch_cnt = rte_file_rx_burst(&fs, mbuf_in, batch_size_in);
// 			}
// 			else{;}
// 			#elif defined(LATENCY_MODE_ON)
// 			/* load pkts batch after previous batch finished to measure latency */
// 			if(rm_stats->tx_buf_cnt == rm_stats->rx_buf_cnt){
// 				batch_cnt = rte_file_rx_burst(&fs, mbuf_in, batch_size_in);	
// 			}
// 			#elif defined(ONLY_MAIN_MODE_ON)
// 			/* debug for not lauching work threads and only run the main thread, without any enqueuing/dequeuing */
// 			batch_cnt = rte_file_rx_burst(&fs, mbuf, batch_size_in);
// 			#else
// 			/* normal running mode */
// 			batch_cnt = rte_file_rx_burst(&fs, mbuf_in, batch_size_in);
// 			#endif
			
// 			// if(batch_cnt < 0 ) {
// 			// 	return batch_cnt;
// 			// }


// 			// // debug
// 			// //printf("pushing batch...\n");
// 			// //printf("batch_cnt=%d\n",batch_cnt);

// 			for(int k=0; k<batch_cnt ; k++) {
// 				rm_stats->rx_buf_cnt++;
// 				#ifndef ONLY_MAIN_MODE_ON
// 				rm_stats->rx_buf_bytes += mbuf_in[k]->data_len;
// 				#else
// 				rm_stats->rx_buf_bytes += mbuf[k]->data_len;
// 				#endif
// 			}

// 			/* start of partition/end2end time keeping */ 
// 			#ifndef LATENCY_AGGREGATION
// 			#ifdef LATENCY_END2END
// 			pkt_ts_exec(pl->ts_start_offset, mbuf_in, batch_cnt);
// 			#endif
// 			#ifdef LATENCY_PARTITION
// 			pkt_ts_exec(pl->ts_start_offset, mbuf_in, batch_cnt);
// 			#endif
// 			#endif

// 			/* sequencing packets */
// 			seq_exec(seq_stage, mbuf_in, batch_cnt);
			

// 			/* TODO: currently we do not consider different flows, should direct flows with less volume to remote workers based on a table */

// 			/* put packets into first ring_in */
// 			tot_enq = 0;
// 			//to_enq = RTE_MIN(batch_cnt,batch_size_in);
// 			to_enq = batch_cnt;
// 			nb_enq = 0;
// 			#ifndef ONLY_MAIN_MODE_ON
// 			#ifdef SHARED_BUFFER
// 			while(tot_enq < batch_cnt){
// 				nb_enq = rte_ring_enqueue_burst(pl->ring_in, (void *)(&mbuf_in[tot_enq]), to_enq, NULL);
// 				tot_enq += nb_enq;
// 				to_enq -= nb_enq;
// 			}
// 			#else
// 			while(tot_enq < batch_cnt){
// 				nb_enq = rte_ring_enqueue_burst(pl->ring_in[ring_in_index], (void *)(&mbuf_in[tot_enq]), to_enq, NULL);
// 				tot_enq += nb_enq;
// 				to_enq -= nb_enq;
// 			}
// 			ring_in_index = (ring_in_index+1)%nb_first_stage;
// 			#endif

// 			#else 
// 			#ifdef SHARED_BUFFER
// 			;
// 			#else
// 			ring_in_index = (ring_in_index+1)%nb_first_stage;
// 			#endif

// 			#endif
			
// 			/* end of partition time keeping */ 
// 			#ifndef LATENCY_AGGREGATION
// 			#ifndef LATENCY_END2END
// 			#ifdef LATENCY_PARTITION
// 			pkt_ts_exec(pl->ts_end_offset, mbuf_in, batch_cnt);
// 			#endif
// 			#endif
// 			#endif

// 			/* start of aggregation time keeping */  
// 			#ifndef LATENCY_PARTITION
// 			#ifndef LATENCY_END2END
// 			#ifdef LATENCY_AGGREGATION
// 			pkt_ts_exec(pl->ts_start_offset, mbuf_in, batch_cnt);
// 			#endif
// 			#endif
// 			#endif


// 			/* read packets from last ring_out(reorder) */
// 			/* reorder packets based on sequence number */
// 			#ifndef ONLY_MAIN_MODE_ON
// 			#ifdef SHARED_BUFFER
// 			batch_cnt = rte_ring_dequeue_burst(pl->ring_out,(void *)mbuf, batch_size_out, NULL);
// 			#else
// 			batch_cnt = rte_ring_dequeue_burst(pl->ring_out[ring_out_index],(void *)mbuf, batch_size_out, NULL);
// 			ring_out_index = (ring_out_index+1)%nb_last_stage;
// 			#endif

// 			#else
// 			#ifdef SHARED_BUFFER
// 			;
// 			#else
// 			ring_out_index = (ring_out_index+1)%nb_last_stage;
// 			#endif

// 			#endif


// 			//reorder_exec(reorder_stage, mbuf, batch_cnt, mbuf_out, &nb_deq);
// 			mbuf_out = mbuf;
// 			nb_deq = batch_cnt;

// 			/* end of aggregation/end2end time keeping */
// 			#ifndef LATENCY_PARTITION
// 			#ifdef LATENCY_END2END
// 			pkt_ts_exec(pl->ts_end_offset, mbuf_out, nb_deq);
// 			#endif
// 			#ifdef LATENCY_AGGREGATION
// 			pkt_ts_exec(pl->ts_end_offset, mbuf_out, nb_deq);
// 			#endif
// 			#endif

			
// 			// //pkt_ts_exec(pl->ts_end_offset, mbuf, batch_cnt);
// 			#ifdef LATENCY_MODE_ON
// 	  	        stats_update_time_main(mbuf_out, nb_deq, pl);
// 		    #endif

// 			//debug
// 			//printf("batch_cnt = %d, ring_in_index = %d\n",batch_cnt, ring_in_index);
// 			if( likely(nb_deq > 0) ) {
// 				rm_stats->tx_batch_cnt ++;
// 			}
// 			for (int i = 0; i < nb_deq; i++) {
// 				rm_stats->tx_buf_cnt++;
// 				rm_stats->tx_buf_bytes += mbuf_out[i]->data_len;
// 			}

// 			// /* for post-processing of reorder */ 
// 			// if(nb_deq >= 0 ){
// 			// 	rm_stats->tx_batch_cnt ++;
// 			// }
// 			// for (int i = 0; i < nb_deq; i++) {
// 			// 	rm_stats->tx_buf_cnt++;
// 			// 	rm_stats->tx_buf_bytes += mbuf_out[i]->data_len;
// 			// 	/* here we simply free the mbuf */
// 			// 	if (rte_mbuf_refcnt_read(mbuf_out[i]) == 1) {
// 			// 		//printf("freeing pkts\n");
// 			// 		rte_pktmbuf_detach_extbuf(mbuf_out[i]);
// 			// 		rte_pktmbuf_free(mbuf_out[i]);
// 			// 	}
// 			// }
			

// 			cycles = rte_rdtsc() - start;

// 			if (!main_lcore){
// 				continue;
// 			}
			

// 			if (cycles - prev_cycles > STATS_INTERVAL_CYCLES) {
// 				run_time = (double)cycles / rte_get_timer_hz();
// 				prev_cycles = cycles;
// 				stats_print_update(stats, run_conf->cores, run_time, false);
// 			}
// 		}

// 	return 0;
// }

static int
run_local(struct pipeline *pl){return 0;}

void
run_local_reg(run_func_t *funcs)
{
	funcs->run = run_local;
}
