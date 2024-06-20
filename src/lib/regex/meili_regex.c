/* Copyright (c) 2024, Meili Authors */

#include "meili_regex.h"
#include "../log/meili_log.h"
#include "../../runtime/meili_runtime.h"

int meili_regex_init(pl_conf *run_conf){
    
	int ret;

	/* Register corresponding regex device operation functions according to regex_dev_type in pl_conf */
	ret = regex_dev_register(run_conf);
	if (ret) {
		MEILI_LOG_ERR("Regex dev registration error");
		return -EINVAL;
	}


	/* Compile rules if necessary or directly use compiled rules to program regex device */
	ret = regex_dev_compile_rules(run_conf);
	if (ret) {
		MEILI_LOG_ERR("Regex dev rule compilation error");
		return -EINVAL;
	}

	/* Initalize regex device(program regex device, construct neccessary structures such as queues, operation buffers, etc.) */
	ret = regex_dev_init(run_conf);
	if (ret) {
		MEILI_LOG_ERR("Failed initialising regex device");
		return -EINVAL;
	}
	return 0;
}