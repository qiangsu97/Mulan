/*
 * Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
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

#ifndef _INCLUDE_LOG_H_
#define _INCLUDE_LOG_H_

#include "../conf/meili_conf.h"

enum meili_log_level {
	MEILI_LOG_LEVEL_ERROR,
	MEILI_LOG_LEVEL_WARNING,
	MEILI_LOG_LEVEL_INFO,
	MEILI_LOG_LEVEL_ALERT,
};

void meili_log(pl_conf *run_conf, enum meili_log_level level, const char *message, ...);

#define MEILI_LOG(run_conf, level, format...)		meili_log(run_conf, MEILI_LOG_LEVEL_##level, format)


#define MEILI_LOG_ERR(format...)					MEILI_LOG(NULL, ERROR, format)
#define MEILI_LOG_WARN(format...)					MEILI_LOG(NULL, WARNING, format)
#define MEILI_LOG_INFO(format...)					MEILI_LOG(NULL, INFO, format)
#define MEILI_LOG_ALERT(format...)					MEILI_LOG(NULL, ALERT, format)
#define MEILI_LOG_WARN_REC(run_conf, format...)		MEILI_LOG(run_conf, WARNING, format)

#endif /* _INCLUDE_LOG_H_ */
