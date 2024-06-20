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

#include <rte_ethdev.h>
#include <rte_lcore.h>

#include <unistd.h>
#include "run_mode.h"
#include "pipeline.h"

#include "../utils/input_mode/dpdk_live_shared.h"
#include "../utils/utils.h"
#include "../utils/net/port_utils.h"

#include "../packet_ordering/packet_ordering.h"
#include "../packet_timestamping/packet_timestamping.h"
#include "../utils/rte_reorder/rte_reorder.h"

static uint16_t primary_port_id;
static uint16_t second_port_id;

struct rte_ether_addr primary_mac_addr;
struct rte_ether_addr second_mac_addr;


static void
update_addr(struct rte_mbuf *m, unsigned dest_portid)
{
	struct rte_ether_hdr *eth;
	struct rte_ipv4_hdr *iph;
	void *tmp;

	eth = (struct rte_ether_hdr *)(rte_pktmbuf_mtod(m, uint8_t*));
	iph = (struct rte_ipv4_hdr*)(rte_pktmbuf_mtod(m, uint8_t*) + sizeof(struct rte_ether_hdr));


	
	tmp = &eth->d_addr.addr_bytes[0];
	/* proj91: b8:ce:f6:83:b8:fc */
	//*((uint64_t *)tmp) = 0xfcb883f6ceb8; 
	/* proj92: b8:ce:f6:88:b2:2e */
	*((uint64_t *)tmp) =  0x2eb288f6ceb8; 
	//iph->src_addr = rte_cpu_to_be_32(info->sa);
    //iph->dst_addr = rte_cpu_to_be_32((uint32_t)RTE_IPV4(100, 100, 100, 92));

	
	/* some random macs 02:00:00:00:00:xx */
	//*((uint64_t *)tmp) = 0x000000000002 + ((uint64_t)dest_portid << 40);

	// /* proj88 p1: 08:c0:eb:8e:d6:87 */
	// *((uint64_t *)tmp) = 0x87d68eebc008; 

	/* src addr */
	rte_ether_addr_copy(&second_mac_addr, &eth->s_addr);

}

#ifdef MEILI_MODE
static int
run_dpdk(struct pipeline *pl)
{
	pl_conf *run_conf = &pl->conf;
	/* always run on main core */
	int qid = 0;
	const uint32_t max_duration = run_conf->input_duration;
	uint32_t batch_size = run_conf->input_batches;
	uint32_t batch_size_in = 0;
	uint32_t batch_size_out = 0;
	int batch_cnt = 0;
	int batch_cnt_enq = 0;
	int batch_cnt_wait_on_enq = 0;
	int batch_cnt_wait_on_deq = 0; /* used for throughput and latency mode. */
	int batch_cnt_tot_enq = 0;
	int batch_cnt_deq = 0;
	int batch_cnt_tot_deq = 0;
	int nb_out_ring_remain= 0;
	rb_stats_t *stats = run_conf->stats;
	run_mode_stats_t *rm_stats = &stats->rm_stats[qid];
	// TODO: rename regex_stats_t to other names
	//regex_stats_t *regex_stats = &stats->regex_stats[qid];

	/* for packet transmission */	
	uint16_t cur_rx, cur_tx, tx_port_id, rx_port_id;
	const unsigned char *pkt;
	pkt_stats_t *pkt_stats;
	dpdk_egress_t *dpdk_tx;
	bool dual_port;

	/* time keeping */
	uint64_t prev_cycles;
	uint64_t prev_cycles_debug = 0;
	uint64_t max_cycles;
	uint64_t cycles;
	double run_time;
	uint64_t start;
	double rate = 0;

	bool main_lcore;
	
	int ret;
	int flag = 0;

	// for baseline implementation, each stage can only use one core 
	/* special pipeline stages(have already been init in pipeline_init) */
	struct pipeline_stage *seq_stage = &pl->seq_stage;
	struct pipeline_stage *reorder_stage = &pl->reorder_stage;

	struct rte_mbuf *mbuf_in[MAX_PKTS_BURST];
	struct rte_mbuf *mbuf[MAX_PKTS_BURST];
	// debug for not reordering 
	//struct rte_mbuf *mbuf_out[MAX_PKTS_BURST];
	struct rte_mbuf **mbuf_out;
	int nb_enq;
	int to_enq;
	int tot_enq;

	int nb_deq_reorder;

	int nb_tx;
	int to_tx;
	int tot_tx;


	//debug
	int ring_in_index = 0;
	int ring_out_index = 0;
	int nb_first_stage = pl->nb_pl_stages? pl->nb_inst_per_pl_stage[0]:1;
	int nb_last_stage = pl->nb_pl_stages? pl->nb_inst_per_pl_stage[pl->nb_pl_stages-1]:1;

	int temp;

	/* Keep coverity check happy by initialising. */
	memset(&mbuf[0], '\0', sizeof(struct rte_mbuf *) * batch_size);
	//debug for not reordering 
	//memset(&mbuf_out[0], '\0', sizeof(struct rte_mbuf *) * batch_size);

	/* Convert duration to cycles. */
	max_cycles = max_duration * rte_get_timer_hz();
	prev_cycles = 0;
	cycles = 0;

	main_lcore = rte_lcore_id() == rte_get_main_lcore();

	printf("main lcore: %d\n", main_lcore);


	/* Get ports */
	pkt_stats = &rm_stats->pkt_stats;
	dpdk_tx = rte_malloc(NULL, sizeof(*dpdk_tx), 64);
	if (!dpdk_tx) {
		MEILI_LOG_ERR("Memory failure on tx queue.");
		return -ENOMEM;
	}


	MEILI_LOG_INFO("Checking port %s",run_conf->port1);
	ret = rte_eth_dev_get_port_by_name(run_conf->port1, &primary_port_id);
	if (ret) {
		MEILI_LOG_ERR("Cannot find port %s.", run_conf->port1);
		return -EINVAL;
	}
	ret = get_port_macaddr(primary_port_id, &primary_mac_addr);
	if(ret){
		MEILI_LOG_ERR("Cannot get port %s eth addr.", run_conf->port1);
		return -EINVAL;
	}

	/* second port is for sending traffic out of the pipeline */
	dual_port = false;
	if (run_conf->port2) {
		MEILI_LOG_INFO("Checking port %s",run_conf->port2);
		ret = rte_eth_dev_get_port_by_name(run_conf->port2, &second_port_id);
		if (ret) {
			MEILI_LOG_ERR("Cannot find port %s.", run_conf->port2);
			return -EINVAL;
		}
		ret = get_port_macaddr(second_port_id, &second_mac_addr);
		if(ret){
			MEILI_LOG_ERR("Cannot get port %s eth addr.", run_conf->port1);
			return -EINVAL;
		}
		dual_port = true;
	}

	if(dual_port){
		MEILI_LOG_INFO("Using dual port, %s-port %d-rx, %s-port %d-tx",run_conf->port1, primary_port_id, run_conf->port2, second_port_id);
	}

	
		
	// dpdk_tx->port_cnt[PRIM_PORT_IDX] = 0;
	// dpdk_tx->port_cnt[SEC_PORT_IDX] = 0;

	/* Default mode ingresses and egresses on the primary port. */
	cur_rx = primary_port_id;
	cur_tx = second_port_id;
	// rx_port_id = PRIM_PORT_IDX;
	// tx_port_id = SEC_PORT_IDX;

		
	/* several different versions of batch size */
	//batch_size_in = 1;
	// batch_size_in = nb_first_stage? batch_size * nb_first_stage:batch_size;
	batch_size_in = batch_size;

	batch_size_out = MAX_PKTS_BURST;
	//batch_size_out = batch_size * nb_last_stage;
	//batch_size_out = batch_size;


	start = rte_rdtsc();

	MEILI_LOG_INFO("Eth batch size = %d, batch_size_in = %d, batch_size_out = %d",DEFAULT_ETH_BATCH_SIZE, batch_size_in, batch_size_out);

	// /* temporary remedy for reorder bug */
	// #ifdef RATE_LIMIT_BPS_ON
	// #elif defined(RATE_LIMIT_PPS_ON)
	// #elif defined(LATENCY_MODE_ON)
	// #elif defined(ONLY_MAIN_MODE_ON)
	// #else
	// /* normal running mode */
	// do{
	// 	batch_cnt = rte_eth_rx_burst(cur_rx, qid, mbuf_in, 256);
	// }while(!batch_cnt);
	// /* put first batch directly into reorder to avoid bug */
	// seq_exec(seq_stage, mbuf_in, batch_cnt);
	// reorder_exec(reorder_stage, mbuf_in, batch_cnt, mbuf_out, &nb_deq_reorder);
	// printf("first enq to/deq from reorder=%d/%d\n",batch_cnt,nb_deq_reorder);
	// for (int i = 0; i < nb_deq_reorder; i++) {
	// 	/* here we simply free the mbuf */
	// 	rte_pktmbuf_free(mbuf_out[i]);
	// }
	// #endif


	/* start reading packets from eth */
	while (!force_quit 
			&& (!max_cycles || cycles <= max_cycles)) 
		{

			/* Hint: rte_eth_rx_burst(dpdk_port_id, queue_id, mbuf_pointer_array, batch_size) */
			/* for main core, queue_id is always 0 */
			#ifdef RATE_LIMIT_BPS_ON
			/* limit the loading speed based on bps */
			run_time = (double)cycles / rte_get_timer_hz();
			rate = ((rm_stats->rx_buf_bytes * 8) / run_time)/ 1000000000.0;
			if(rate < RATE_Gbps){
				batch_cnt = rte_eth_rx_burst(cur_rx, qid, mbuf_in, DEFAULT_ETH_BATCH_SIZE);
			}
			else{;}
			#elif defined(RATE_LIMIT_PPS_ON)
			/* limit the loading speed based on pps */
			run_time = (double)cycles / rte_get_timer_hz();
			rate = ((rm_stats->rx_buf_cnt ) / run_time)/ 1000000.0;
			if(rate < RATE_Mpps){
				batch_cnt = rte_eth_rx_burst(cur_rx, qid, mbuf_in, DEFAULT_ETH_BATCH_SIZE);
			}
			else{;}
			#elif defined(LATENCY_MODE_ON)
			/* load pkts batch after previous batch finished to measure latency */
			/* In latency mode, inner loop will only exit when batch_cnt_tot_deq == batch_cnt_tot_enq to ensure all pkts enqueued has been dequeued from the pipeline.  */
			/* However, we do not count total # of pkts dequeued from reorder stage but count that from the last worker stages, as batch_cnt_tot_deq. This is because some pkts may remain in reorder stage until more pkts arrive. */
			batch_cnt = rte_eth_rx_burst(cur_rx, qid, mbuf_in, DEFAULT_ETH_BATCH_SIZE);	
			#elif defined(ONLY_MAIN_MODE_ON)
			/* debug for not lauching work threads and only run the main thread, without any enqueuing/dequeuing */
			batch_cnt = rte_eth_rx_burst(cur_rx, qid, mbuf, DEFAULT_ETH_BATCH_SIZE);
			#else
			/* normal running mode */
			batch_cnt = rte_eth_rx_burst(cur_rx, qid, mbuf_in, DEFAULT_ETH_BATCH_SIZE);
			#endif
			



			if(batch_cnt <= 0){
				/* no pkt received, directly goto get packets out if there is packet waiting to be dequeued */
				//printf("no packet received, batch_cnt_wait_on_deq = %d\n",batch_cnt_wait_on_deq);
				if(batch_cnt_wait_on_deq > 0){
					goto aggregate_packets;
				}
				else{
					continue;
				}
			}

			// // debug for eth device
			// if(batch_cnt > 0){
			// 	printf("pushing batch...\n");
			// 	printf("batch_cnt=%d\n",batch_cnt);
			// }

			
			for(int k=0; k<batch_cnt ; k++) {
				rm_stats->rx_buf_cnt++;
				#ifndef ONLY_MAIN_MODE_ON
				rm_stats->rx_buf_bytes += mbuf_in[k]->data_len;
				// // debug for correct pkt length
				// printf("data_len = %d\n",mbuf_in[k]->data_len);
				// printf("pkt_len = %d\n",mbuf_in[k]->pkt_len);
				#else
				rm_stats->rx_buf_bytes += mbuf[k]->data_len;
				#endif		
			}

			batch_cnt_wait_on_enq = batch_cnt;
			batch_cnt_tot_enq = 0;
			batch_cnt_tot_deq = 0;
			/* Receiving packets is separated from enqueuing to pipeline stages. This inner loop is for enqueuing to pipeline stages */
			/* Note that if latency mode is on, we must finish all processing of this batch, then proceed to next batch. 
			 * This is done via inner-inner loop, which waits for all enqueued(a batch, for per-pkt latency, batch_size_in = 1) packets to dequeue. 
			 */
			while (!force_quit && batch_cnt_wait_on_enq > 0)
			{
				
				/* # of pkts to enqueue this round. Note that ethernet device could receive less */
				batch_cnt_enq = RTE_MIN(batch_size_in, batch_cnt_wait_on_enq);


				//#ifdef LATENCY_MODE_ON
				batch_cnt_wait_on_deq += batch_cnt_enq;
				//#endif
				// // debug
				// printf("batch_cnt_wait_on_enq = %d\n",batch_cnt_wait_on_enq);

				/* start of partition/end2end time keeping */ 
				#ifndef LATENCY_AGGREGATION
				#ifdef LATENCY_END2END
				pkt_ts_exec(pl->ts_start_offset, &mbuf_in[batch_cnt_tot_enq], batch_cnt_enq);
				#endif
				#ifdef LATENCY_PARTITION
				pkt_ts_exec(pl->ts_start_offset, &mbuf_in[batch_cnt_tot_enq], batch_cnt_enq);
				#endif
				#endif

				

				/* TODO: currently we do not consider different flows, should direct flows with less volume to remote workers based on a table */

				/* put packets into first ring_in */
				tot_enq = 0;
				to_enq = batch_cnt_enq; /* batch_cnt_enq is always less equal than batch_size_in, no need to conduct RTE_MIN */
				nb_enq = 0;
				#ifndef ONLY_MAIN_MODE_ON
					/* direct pkts to local/remote pipelines */
					/* TODO: we temporarily distinguish two modes: all local pipeline and all remote pipeline. We should merge all run modes into one enqueue function.  */

					// /* All packets should be forwarded to remote pipelines. ONLY for measurement of traffic routing throughput and latency.*/
					// #ifdef ALL_REMOTE_ON_ARRIVAL 
					// 	/* prefetch pkts to be redirected and modify their mac */
					// 	for(int i=0; i<to_enq; i++){
					// 		//rte_prefetch0(rte_pktmbuf_mtod(mbuf_in[batch_cnt_tot_enq+i], void *));
					// 		//update_addr(mbuf_in[batch_cnt_tot_enq+i], cur_tx); 
					// 		update_addr(mbuf_in[batch_cnt_tot_enq+i], cur_rx); 
					// 		//printf("%d-%d ",mbuf_in[batch_cnt_tot_enq+i]->data_len, mbuf_in[batch_cnt_tot_enq+i]->pkt_len);
					// 	}
					// 	//printf("\n");
					// 	/* Use second port to transmit packets */
					// 	while(tot_enq < batch_cnt_enq){
					// 		nb_enq = rte_eth_tx_burst(cur_rx, qid, &mbuf_in[batch_cnt_tot_enq+tot_enq], to_enq);
					// 		// //debug 
					// 		//printf("to_tx=%d, nb_txed=%d\n",to_enq,nb_enq);
					// 		tot_enq += nb_enq;
					// 		to_enq -= nb_enq;
					// 	}
					// 	#ifndef LATENCY_AGGREGATION
					// 	#ifndef LATENCY_END2END
					// 	#ifdef LATENCY_PARTITION
					// 	pkt_ts_exec(pl->ts_end_offset, &mbuf_in[batch_cnt_tot_enq], batch_cnt_enq);
					// 	stats_update_time_main(&mbuf_in[batch_cnt_tot_enq], batch_cnt_enq, pl);
					// 	#endif
					// 	#endif
					// 	#endif

					// 	batch_cnt_wait_on_deq = 0;
					// 	/* pkts are all redirected to remote pipelines */
					// 	/* stats recording for throughput */
					// 	if( likely(batch_cnt_enq > 0) ) {
					// 		rm_stats->tx_batch_cnt ++;
					// 	}
					// 	for (int i = 0; i < batch_cnt_enq; i++) {
					// 		rm_stats->tx_buf_cnt++;
					// 		rm_stats->tx_buf_bytes += mbuf_in[batch_cnt_tot_enq+i]->data_len;

					// 		rte_pktmbuf_free(mbuf_in[batch_cnt_tot_enq+i]);
					// 	}

					// 	/* goto next pipeline batch without dequeuing from local rings, as pkts are ALL REMOTE */
					// 	goto finish_pipeline_batch;
					// #endif /* ALL_REMOTE_ON_ARRIVAL */
					
					/* sequencing packets that are processed locally */
					seq_exec(seq_stage, &mbuf_in[batch_cnt_tot_enq], batch_cnt_enq);
					//debug
					//printf("enqueue batch\n");
					#ifdef SHARED_BUFFER
					while(tot_enq < batch_cnt_enq && !force_quit){
						nb_enq = rte_ring_enqueue_burst(pl->ring_in, (void *)(&mbuf_in[batch_cnt_tot_enq+tot_enq]), to_enq, NULL);
						tot_enq += nb_enq;
						to_enq -= nb_enq;
					}
					#else
					while(tot_enq < batch_cnt_enq && !force_quit){
						nb_enq = rte_ring_enqueue_burst(pl->ring_in[ring_in_index], (void *)(&mbuf_in[batch_cnt_tot_enq+tot_enq]), to_enq, NULL);
						tot_enq += nb_enq;
						to_enq -= nb_enq;
					}

				

					// // debug
					//printf("enqueue to ring %d\n",ring_in_index);

					ring_in_index = (ring_in_index+1)%nb_first_stage;
					/* debug for only using one pipeline */
					//temp = (ring_in_index+1)%nb_first_stage;
					#endif
				#else 
					#ifdef SHARED_BUFFER
					;
					#else
					/* round-robin change the stage instance for each batch_size_in */
					ring_in_index = (ring_in_index+1)%nb_first_stage;
					#endif
				#endif /* ifndef ONLY_MAIN_MODE_ON */
				

				/* end of partition time keeping */ 
				#ifndef LATENCY_AGGREGATION
				#ifndef LATENCY_END2END
				#ifdef LATENCY_PARTITION
				pkt_ts_exec(pl->ts_end_offset, &mbuf_in[batch_cnt_tot_enq], batch_cnt_enq);
				#endif
				#endif
				#endif

				/* start of aggregation time keeping */  
				#ifndef LATENCY_PARTITION
				#ifndef LATENCY_END2END
				#ifdef LATENCY_AGGREGATION
				pkt_ts_exec(pl->ts_start_offset, &mbuf_in[batch_cnt_tot_enq], batch_cnt_enq);
				#endif
				#endif
				#endif


				/* Note: 
				* In latency mode, we want to acquire per-packet latency. So we keep waiting on the packet that has been enqueued in this inner-inner loop. 
				* In throughput mode, we do not need to wait for inflight packets
				*/
				#ifdef LATENCY_MODE_ON /* start of inner-inner loop */
				while(batch_cnt_wait_on_deq > 0){
				#endif

					/* read packets from last ring_out(reorder) */
					/* reorder packets based on sequence number */
				aggregate_packets:
					#ifndef ONLY_MAIN_MODE_ON

						#ifdef SHARED_BUFFER
							batch_cnt_deq = rte_ring_dequeue_burst(pl->ring_out,(void *)mbuf, batch_size_out, NULL);
						#else
							//batch_cnt_deq = rte_ring_dequeue_burst(pl->ring_out[ring_out_index],(void *)mbuf, batch_size_out, NULL);
							batch_cnt_deq = rte_ring_dequeue_burst(pl->ring_out[ring_out_index],(void *)mbuf, batch_size_out, &nb_out_ring_remain);
							/* debug */
							#ifdef LATENCY_MODE_ON 
							/* In latency mode, do not change the ring buffer index in the inner-inner loop. Only change the index after inner-inner loop ends and simulate the mod operation here. */
							temp = (ring_out_index+1)%nb_last_stage;
							#else
							/* debug for only using one pipeline */
							//temp = (ring_out_index+1)%nb_last_stage;
							ring_out_index = (ring_out_index+1)%nb_last_stage;
							#endif
						#endif

					#else
						#ifdef SHARED_BUFFER
						;
						#else
						ring_out_index = (ring_out_index+1)%nb_last_stage;
						#endif

					#endif

					//test
					//reorder_exec(reorder_stage, mbuf, batch_cnt_deq, mbuf_out, &nb_deq_reorder);
					// // if(batch_cnt_deq >0 && *rte_reorder_seqn(mbuf[0]) < 1024){
					// // 	printf("sample seq num=%d\n",*rte_reorder_seqn(mbuf[0]));
					// // }

					// // debug for reorder
					// cycles = rte_rdtsc() - start;
					// if (cycles - prev_cycles_debug > STATS_INTERVAL_CYCLES) {
					//printf("batch_cnt_deq = %d\n",batch_cnt_deq);
					// 	printf("nb_out_ring_remain = %d\n",nb_out_ring_remain);
					// 	printf("nb_deq_reorder = %d\n",nb_deq_reorder);
					// 	prev_cycles_debug = cycles;
					// }
					mbuf_out = mbuf;
					nb_deq_reorder = batch_cnt_deq;


					/* end of aggregation/end2end time keeping */
					#ifndef LATENCY_PARTITION
					#ifdef LATENCY_END2END
					pkt_ts_exec(pl->ts_end_offset, mbuf_out, nb_deq_reorder);
					#endif
					#ifdef LATENCY_AGGREGATION
					pkt_ts_exec(pl->ts_end_offset, mbuf_out, nb_deq_reorder);
					#endif
					#endif


				
					#ifdef LATENCY_MODE_ON
					/* Update stats including 1) main thread latency stats, 2) breakdown latency stats(if PKT_LATENCY_BREAKDOWN_ON is defined) 
					 * and 3) collect pkt latency sample(if PKT_LATENCY_SAMPLE_ON is defined) 
					 */
					stats_update_time_main(mbuf_out, nb_deq_reorder, pl);
					#endif

					if( likely(nb_deq_reorder > 0) ) {
						// printf("nb_deq_reorder=%d\n",nb_deq_reorder);
						rm_stats->tx_batch_cnt ++;
					}
					for (int i = 0; i < nb_deq_reorder; i++) {
						rm_stats->tx_buf_cnt++;
						rm_stats->tx_buf_bytes += mbuf_out[i]->data_len;
					}

					/* transmit pkts to destination */
					#ifdef ALL_REMOTE_AFTER_PROCESSING
						to_tx = nb_deq_reorder;
						tot_tx = 0;
						/* prefetch pkts to be redirected and modify their mac */
						for(int i=0; i<to_tx; i++){
							rte_prefetch0(rte_pktmbuf_mtod(mbuf_out[i], void *));
							update_addr(mbuf_out[i], cur_tx); 
						}

						while(tot_tx < nb_deq_reorder){
							nb_tx = rte_eth_tx_burst(cur_tx, qid, mbuf_out, to_tx);
							// //debug 
							// printf("to_tx=%d, nb_txed=%d\n",to_tx,nb_tx);
							tot_tx += nb_tx;
							to_tx -= nb_tx;
						}
					#endif /* ALL_REMOTE_AFTER_PROCESSING */

					/* post-processing of pkts */ 
					for (int i = 0; i < nb_deq_reorder; i++) {
						/* here we simply free the mbuf */
						rte_pktmbuf_free(mbuf_out[i]);
					}

					/* change inner loop counters */
					//debug
					// if(batch_cnt > 0){
					// 	printf("\n%d pkts received from ethernet\n",batch_cnt);
					// 	printf("batch_cnt_enq = %d\n",batch_cnt_enq);
					// 	printf("batch_cnt_wait_on_enq = %d\n",batch_cnt_wait_on_enq);
					// 	printf("batch_cnt_tot_enq = %d\n",batch_cnt_tot_enq);
					// 	printf("batch_cnt_deq = %d\n",batch_cnt_deq);
					// 	printf("nb_deq_reorder = %d\n",nb_deq_reorder);
					// 	printf("batch_cnt_tot_deq = %d\n",batch_cnt_tot_deq);
					// 	printf("rx_buf_cnt = %ld, tx_buf_cnt = %ld\n",rm_stats->rx_buf_cnt, rm_stats->tx_buf_cnt);
					// }

				
					

					/* # of pkts still left in the pipeline */
					batch_cnt_wait_on_deq -= nb_deq_reorder;


					//debug 
					// printf("wait on dequeue from ring %d\n",ring_out_index);
					//printf("batch_cnt_wait_on_deq = %d, nb_deq_reorder = %d\n",batch_cnt_wait_on_deq, nb_deq_reorder);
				#ifdef LATENCY_MODE_ON

				}/* End of inner-inner loop. Proceed */

				/* In latency mode, the inner-inner loop ends, move to next pair of rte_ring. */
				/* In normal mode, simply move to next pair of rte_ring right after dequeue operation. */
				ring_out_index = (ring_out_index+1)%nb_last_stage;
				/* debug for only using one pipeline in latency mode */
				//temp = (ring_out_index+1)%nb_last_stage;
				#endif

			finish_pipeline_batch:
				/* # of pkts wait to be enqueued */
				batch_cnt_wait_on_enq -= batch_cnt_enq; 
				/* # of pkts already enqueued in total(this round) */
				batch_cnt_tot_enq += batch_cnt_enq; 

			}/* End of inner loop. Proceed to process next pipeline batch. */

			/* Print pipeline stats every 1s */
			cycles = rte_rdtsc() - start;

			if (!main_lcore){
				continue;
			}
			
			if (cycles - prev_cycles > STATS_INTERVAL_CYCLES) {
				run_time = (double)cycles / rte_get_timer_hz();
				prev_cycles = cycles;
				stats_print_update(stats, run_conf->cores, run_time, false);
			}
		}/* End of outer loop. Proceed to receive and process next eth batch. */
	printf("Exiting on main core\n");	
	return 0;
}
#endif


#ifdef ALL_REMOTE_ON_ARRIVAL
static int
run_dpdk(struct pipeline *pl)
{
	pl_conf *run_conf = &pl->conf;
	/* always run on main core */
	int qid = 0;
	const uint32_t max_duration = run_conf->input_duration;
	uint32_t batch_size = run_conf->input_batches;
	uint32_t batch_size_in = 0;
	uint32_t batch_size_out = 0;
	int batch_cnt = 0;
	int batch_cnt_enq = 0;
	int batch_cnt_wait_on_enq = 0;
	int batch_cnt_wait_on_deq = 0; /* used for throughput and latency mode. */
	int batch_cnt_tot_enq = 0;
	int batch_cnt_deq = 0;
	int batch_cnt_tot_deq = 0;
	int nb_out_ring_remain= 0;
	rb_stats_t *stats = run_conf->stats;
	run_mode_stats_t *rm_stats = &stats->rm_stats[qid];
	// TODO: rename regex_stats_t to other names
	//regex_stats_t *regex_stats = &stats->regex_stats[qid];

	/* for packet transmission */	
	uint16_t cur_rx, cur_tx, tx_port_id, rx_port_id;
	const unsigned char *pkt;
	pkt_stats_t *pkt_stats;
	dpdk_egress_t *dpdk_tx;
	bool dual_port;

	/* time keeping */
	uint64_t prev_cycles;
	uint64_t prev_cycles_debug = 0;
	uint64_t max_cycles;
	uint64_t cycles;
	double run_time;
	uint64_t start;
	double rate = 0;

	bool main_lcore;
	
	int ret;
	int flag = 0;

	// for baseline implementation, each stage can only use one core 
	/* special pipeline stages(have already been init in pipeline_init) */
	struct pipeline_stage *seq_stage = &pl->seq_stage;
	struct pipeline_stage *reorder_stage = &pl->reorder_stage;

	struct rte_mbuf *mbuf_in[MAX_PKTS_BURST];
	struct rte_mbuf *mbuf[MAX_PKTS_BURST];
	// debug for not reordering 
	//struct rte_mbuf *mbuf_out[MAX_PKTS_BURST];
	struct rte_mbuf **mbuf_out;
	int nb_enq;
	int to_enq;
	int tot_enq;

	int nb_deq_reorder;

	int nb_tx;
	int to_tx;
	int tot_tx;


	//debug
	int ring_in_index = 0;
	int ring_out_index = 0;
	int nb_first_stage = pl->nb_pl_stages? pl->nb_inst_per_pl_stage[0]:1;
	int nb_last_stage = pl->nb_pl_stages? pl->nb_inst_per_pl_stage[pl->nb_pl_stages-1]:1;

	int temp;

	/* Keep coverity check happy by initialising. */
	memset(&mbuf[0], '\0', sizeof(struct rte_mbuf *) * batch_size);
	//debug for not reordering 
	//memset(&mbuf_out[0], '\0', sizeof(struct rte_mbuf *) * batch_size);

	/* Convert duration to cycles. */
	max_cycles = max_duration * rte_get_timer_hz();
	prev_cycles = 0;
	cycles = 0;

	main_lcore = rte_lcore_id() == rte_get_main_lcore();


	/* Get ports */
	pkt_stats = &rm_stats->pkt_stats;
	dpdk_tx = rte_malloc(NULL, sizeof(*dpdk_tx), 64);
	if (!dpdk_tx) {
		MEILI_LOG_ERR("Memory failure on tx queue.");
		return -ENOMEM;
	}


	MEILI_LOG_INFO("Checking port %s",run_conf->port1);
	ret = rte_eth_dev_get_port_by_name(run_conf->port1, &primary_port_id);
	if (ret) {
		MEILI_LOG_ERR("Cannot find port %s.", run_conf->port1);
		return -EINVAL;
	}
	ret = get_port_macaddr(primary_port_id, &primary_mac_addr);
	if(ret){
		MEILI_LOG_ERR("Cannot get port %s eth addr.", run_conf->port1);
		return -EINVAL;
	}

	/* second port is for sending traffic out of the pipeline */
	dual_port = false;
	if (run_conf->port2) {
		MEILI_LOG_INFO("Checking port %s",run_conf->port2);
		ret = rte_eth_dev_get_port_by_name(run_conf->port2, &second_port_id);
		if (ret) {
			MEILI_LOG_ERR("Cannot find port %s.", run_conf->port2);
			return -EINVAL;
		}
		ret = get_port_macaddr(second_port_id, &second_mac_addr);
		if(ret){
			MEILI_LOG_ERR("Cannot get port %s eth addr.", run_conf->port1);
			return -EINVAL;
		}
		dual_port = true;
	}

	if(dual_port){
		MEILI_LOG_INFO("Using dual port, %s-port %d-rx, %s-port %d-tx",run_conf->port1, primary_port_id, run_conf->port2, second_port_id);
	}

	
		
	// dpdk_tx->port_cnt[PRIM_PORT_IDX] = 0;
	// dpdk_tx->port_cnt[SEC_PORT_IDX] = 0;

	/* Default mode ingresses and egresses on the primary port. */
	cur_rx = primary_port_id;
	cur_tx = second_port_id;
	// rx_port_id = PRIM_PORT_IDX;
	// tx_port_id = SEC_PORT_IDX;

		
	/* several different versions of batch size */
	//batch_size_in = 1;
	// batch_size_in = nb_first_stage? batch_size * nb_first_stage:batch_size;
	batch_size_in = batch_size;

	batch_size_out = MAX_PKTS_BURST;
	//batch_size_out = batch_size * nb_last_stage;
	//batch_size_out = batch_size;


	start = rte_rdtsc();

	MEILI_LOG_INFO("Using test mode: ALL_REMOTE_ON_ARRIVAL");
	MEILI_LOG_INFO("Eth batch size = %d, batch_size_in = %d, batch_size_out = %d",DEFAULT_ETH_BATCH_SIZE, batch_size_in, batch_size_out);


	/* start reading packets from eth */
	while (!force_quit 
			&& (!max_cycles || cycles <= max_cycles)) 
		{
			batch_cnt = 0;
			batch_cnt = rte_eth_rx_burst(cur_rx, qid, mbuf_in, DEFAULT_ETH_BATCH_SIZE);

			if(batch_cnt <= 0){
				/* no pkt received, directly goto get packets out if there is packet waiting to be dequeued */
				continue;
			}

			
			for(int k=0; k<batch_cnt ; k++) {
				rm_stats->rx_buf_cnt++;
				#ifndef ONLY_MAIN_MODE_ON
				rm_stats->rx_buf_bytes += mbuf_in[k]->data_len;

				#else
				rm_stats->rx_buf_bytes += mbuf[k]->data_len;
				#endif		
			}

			
			/* Receiving packets is separated from enqueuing to pipeline stages. This inner loop is for enqueuing to pipeline stages */
			/* Note that if latency mode is on, we must finish all processing of this batch, then proceed to next batch. 
			 * This is done via inner-inner loop, which waits for all enqueued(a batch, for per-pkt latency, batch_size_in = 1) packets to dequeue. 
			 */
				

			if( likely(batch_cnt > 0) ) {
				rm_stats->tx_batch_cnt ++;
			}
			for (int i = 0; i < batch_cnt; i++) {
				rm_stats->tx_buf_cnt++;
				rm_stats->tx_buf_bytes += mbuf_in[i]->data_len;
			}

			/* sequencing packets that are processed locally */
			seq_exec(seq_stage, mbuf_in, batch_cnt);



			batch_cnt_wait_on_deq += batch_cnt;

			/* put packets into first ring_in */
			tot_enq = 0;
			to_enq =  batch_cnt;
			nb_enq = 0;
			

			/* All packets should be forwarded to remote pipelines. ONLY for measurement of traffic routing throughput and latency.*/
			/* prefetch pkts to be redirected and modify their mac */
			for(int i=0; i<to_enq; i++){
				//rte_prefetch0(rte_pktmbuf_mtod(mbuf_in[batch_cnt_tot_enq+i], void *));
				//update_addr(mbuf_in[batch_cnt_tot_enq+i], cur_tx); 
				update_addr(mbuf_in[batch_cnt_tot_enq+i], cur_rx); 
				//printf("%d-%d ",mbuf_in[batch_cnt_tot_enq+i]->data_len, mbuf_in[batch_cnt_tot_enq+i]->pkt_len);
			}
			/* Use second port to transmit packets */
			while(tot_enq < batch_cnt){
				//nb_enq = rte_eth_tx_burst(cur_tx, qid, &mbuf_in[tot_enq], to_enq);
				nb_enq = rte_eth_tx_burst(cur_rx, qid, &mbuf_in[tot_enq], to_enq);
				// //debug 
				//printf("to_tx=%d, nb_txed=%d\n",to_enq,nb_enq);
				tot_enq += nb_enq;
				to_enq -= nb_enq;
			}

			/* all sent */
			batch_cnt_wait_on_deq = 0;
			/* pkts are all redirected to remote pipelines */
			/* stats recording for throughput */

			for (int i = 0; i < batch_cnt; i++) {
				rte_pktmbuf_free(mbuf_in[i]);
			}


			/* Print pipeline stats every 1s */
			cycles = rte_rdtsc() - start;

			if (!main_lcore){
				continue;
			}
			
			if (cycles - prev_cycles > STATS_INTERVAL_CYCLES) {
				run_time = (double)cycles / rte_get_timer_hz();
				prev_cycles = cycles;
				stats_print_update(stats, run_conf->cores, run_time, false);
			}
		}/* End of outer loop. Proceed to receive and process next eth batch. */

	return 0;
}
#endif

#ifdef BASELINE_MODE
static int
run_dpdk(struct pipeline *pl)
{
	pl_conf *run_conf = &pl->conf;
	/* always run on main core */
	int qid = 0;
	const uint32_t max_duration = run_conf->input_duration;
	uint32_t batch_size = run_conf->input_batches;
	uint32_t batch_size_in = 0;
	uint32_t batch_size_out = 0;
	int batch_cnt = 0;
	int batch_cnt_enq = 0;
	int batch_cnt_wait_on_enq = 0;
	int batch_cnt_wait_on_deq = 0; /* used for throughput and latency mode. */
	int batch_cnt_tot_enq = 0;
	int batch_cnt_deq = 0;
	int batch_cnt_tot_deq = 0;
	int nb_out_ring_remain= 0;
	rb_stats_t *stats = run_conf->stats;
	run_mode_stats_t *rm_stats = &stats->rm_stats[qid];
	// TODO: rename regex_stats_t to other names
	//regex_stats_t *regex_stats = &stats->regex_stats[qid];

	/* for packet transmission */	
	uint16_t cur_rx, cur_tx, tx_port_id, rx_port_id;
	const unsigned char *pkt;
	pkt_stats_t *pkt_stats;
	dpdk_egress_t *dpdk_tx;
	bool dual_port;

	/* time keeping */
	uint64_t prev_cycles;
	uint64_t prev_cycles_debug = 0;
	uint64_t max_cycles;
	uint64_t cycles;
	double run_time;
	uint64_t start;
	double rate = 0;

	bool main_lcore;
	
	int ret;
	int flag = 0;

	// for baseline implementation, each stage can only use one core 
	/* special pipeline stages(have already been init in pipeline_init) */
	struct pipeline_stage *seq_stage = &pl->seq_stage;
	struct pipeline_stage *reorder_stage = &pl->reorder_stage;

	struct rte_mbuf *mbuf_in[MAX_PKTS_BURST];
	struct rte_mbuf *mbuf_out[MAX_PKTS_BURST];
	struct rte_mbuf **mbuf_out_temp = mbuf_out;
	int nb_enq;
	int to_enq;
	int tot_enq;

	int nb_deq_reorder;

	int nb_tx;
	int to_tx;
	int tot_tx;


	//debug
	int ring_in_index = 0;
	int ring_out_index = 0;
	int nb_first_stage = pl->nb_pl_stages? pl->nb_inst_per_pl_stage[0]:1;
	int nb_last_stage = pl->nb_pl_stages? pl->nb_inst_per_pl_stage[pl->nb_pl_stages-1]:1;

	struct pipeline_stage *self;


	/* Keep coverity check happy by initialising. */
	memset(&mbuf_in[0], '\0', sizeof(struct rte_mbuf *) * batch_size);
	memset(&mbuf_out[0], '\0', sizeof(struct rte_mbuf *) * batch_size);



	/* Convert duration to cycles. */
	max_cycles = max_duration * rte_get_timer_hz();
	prev_cycles = 0;
	cycles = 0;

	main_lcore = rte_lcore_id() == rte_get_main_lcore();


	/* Get ports */
	pkt_stats = &rm_stats->pkt_stats;
	dpdk_tx = rte_malloc(NULL, sizeof(*dpdk_tx), 64);
	if (!dpdk_tx) {
		MEILI_LOG_ERR("Memory failure on tx queue.");
		return -ENOMEM;
	}


	MEILI_LOG_INFO("Checking port %s",run_conf->port1);
	ret = rte_eth_dev_get_port_by_name(run_conf->port1, &primary_port_id);
	if (ret) {
		MEILI_LOG_ERR("Cannot find port %s.", run_conf->port1);
		return -EINVAL;
	}
	ret = get_port_macaddr(primary_port_id, &primary_mac_addr);
	if(ret){
		MEILI_LOG_ERR("Cannot get port %s eth addr.", run_conf->port1);
		return -EINVAL;
	}

	/* second port is for sending traffic out of the pipeline */
	dual_port = false;
	if (run_conf->port2) {
		MEILI_LOG_INFO("Checking port %s",run_conf->port2);
		ret = rte_eth_dev_get_port_by_name(run_conf->port2, &second_port_id);
		if (ret) {
			MEILI_LOG_ERR("Cannot find port %s.", run_conf->port2);
			return -EINVAL;
		}
		ret = get_port_macaddr(second_port_id, &second_mac_addr);
		if(ret){
			MEILI_LOG_ERR("Cannot get port %s eth addr.", run_conf->port1);
			return -EINVAL;
		}
		dual_port = true;
	}

	if(dual_port){
		MEILI_LOG_INFO("Using dual port, %s-port %d-rx, %s-port %d-tx",run_conf->port1, primary_port_id, run_conf->port2, second_port_id);
	}

	
		
	// dpdk_tx->port_cnt[PRIM_PORT_IDX] = 0;
	// dpdk_tx->port_cnt[SEC_PORT_IDX] = 0;

	/* Default mode ingresses and egresses on the primary port. */
	cur_rx = primary_port_id;
	cur_tx = second_port_id;
	// rx_port_id = PRIM_PORT_IDX;
	// tx_port_id = SEC_PORT_IDX;

		
	/* several different versions of batch size */
	//batch_size_in = 1;
	// batch_size_in = nb_first_stage? batch_size * nb_first_stage:batch_size;
	batch_size_in = batch_size;

	batch_size_out = MAX_PKTS_BURST;
	//batch_size_out = batch_size * nb_last_stage;
	//batch_size_out = batch_size;


	start = rte_rdtsc();

	MEILI_LOG_INFO("Using test mode: BASELINE MODE");
	MEILI_LOG_INFO("Eth batch size = %d, batch_size_in = %d, batch_size_out = %d",DEFAULT_ETH_BATCH_SIZE, batch_size_in, batch_size_out);

	self = pl->stages[0][0];
	/* start reading packets from eth */
	while (!force_quit 
			&& (!max_cycles || cycles <= max_cycles)) 
		{
			batch_cnt = 0;
			batch_cnt_tot_enq = 0;
			batch_cnt = rte_eth_rx_burst(cur_rx, qid, mbuf_in, DEFAULT_ETH_BATCH_SIZE);

			batch_cnt_wait_on_enq = batch_cnt;
			for(int k=0; k<batch_cnt ; k++) {
				rm_stats->rx_buf_cnt++;
				rm_stats->rx_buf_bytes += mbuf_in[k]->data_len;
			}

			if(batch_cnt <= 0){
				/* no pkt received */
				continue;
			}
			#ifdef LATENCY_END2END
			while(batch_cnt_wait_on_enq > 0){
			#endif
				nb_enq = RTE_MIN(batch_size_in, batch_cnt_wait_on_enq);
				batch_cnt_wait_on_deq += nb_enq;
				batch_cnt_wait_on_enq -= nb_enq;


			#ifdef LATENCY_END2END
			pkt_ts_exec(pl->ts_start_offset, &mbuf_in[batch_cnt_tot_enq], nb_enq);
			#endif
				/* directly processing packets */
				#ifdef LATENCY_END2END
				while(batch_cnt_wait_on_deq > 0){
				#endif
				pipeline_stage_exec_safe(self, &mbuf_in[batch_cnt_tot_enq], nb_enq, &mbuf_out_temp, &batch_cnt_deq);
				batch_cnt_wait_on_deq -= batch_cnt_deq;
				nb_enq = 0;
				#ifdef LATENCY_END2END
				}
				#endif
				
				//printf("batch_cnt_deq=%d\n",batch_cnt_deq);
				#ifdef LATENCY_END2END
				pkt_ts_exec(pl->ts_end_offset, mbuf_out_temp, batch_cnt_deq);
				stats_update_time_main(mbuf_out_temp, batch_cnt_deq, pl);
				#endif
				if( likely(batch_cnt_deq > 0) ) {
					rm_stats->tx_batch_cnt ++;
				}
				for (int i = 0; i < batch_cnt_deq; i++) {
					rm_stats->tx_buf_cnt++;
					rm_stats->tx_buf_bytes += mbuf_out_temp[i]->data_len;
					rte_pktmbuf_free(mbuf_out_temp[i]);
				}
				batch_cnt_tot_enq += nb_enq;
			#ifdef LATENCY_END2END
			}
			#endif


			/* Print pipeline stats every 1s */
			cycles = rte_rdtsc() - start;

			if (!main_lcore){
				continue;
			}
			
			if (cycles - prev_cycles > STATS_INTERVAL_CYCLES) {
				run_time = (double)cycles / rte_get_timer_hz();
				prev_cycles = cycles;
				stats_print_update(stats, run_conf->cores, run_time, false);
			}
		}/* End of outer loop. Proceed to receive and process next eth batch. */

	return 0;
}
#endif


void
run_dpdk_reg(run_func_t *funcs)
{
	funcs->run = run_dpdk;
}
