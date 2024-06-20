/* Copyright (c) 2024, Meili Authors */

/* timestamp library similar to rte_reorder */
/* use dyn field of mbuf to store two timestamps(start/end) */
#include <stdlib.h>
#include <stdio.h>

#include "timestamp.h"

// int timestamp_start_dynfield_offset = -1;
// int timestamp_end_dynfield_offset = -1;

int used_names = 0;

// char ts_name[16] = "PL_MAIN_TIMESTAMP_START_DNYFIELD";

char ts_names[8] = {'0', '1', '2', '3', '4', '5', '6', '7'};

/* register a timestamp */
int
timestamp_init()
{

    int ret;
	struct rte_mbuf_dynfield timestamp_start_dynfield_desc = {
		.name = ts_names[used_names],
		.size = sizeof(timestamp_t),
		.align = __alignof__(timestamp_t),
	};



	ret = rte_mbuf_dynfield_register(&timestamp_start_dynfield_desc);
	printf("dnyfield offset = %d\n",ret);
	if (ret < 0) {
		// RTE_LOG(ERR, REORDER,
		// 	"Failed to register mbuf field for reorder sequence number, rte_errno: %i\n",
		// 	rte_errno);
		//rte_errno = ENOMEM;
		return -ENOMEM;
	}

	used_names++;

    return ret;
}





