/* Copyright (c) 2024, Meili Authors */

#ifndef _INCLUDE_PIPELINE_RUNTIME_H
#define _INCLUDE_PIPELINE_RUNTIME_H

#include "pipeline.h"
#include "run_mode.h"

int register_meili_apis();

int meili_regex_init(pl_conf *run_conf);

int meili_compression_init();

int meili_runtime_init(struct pipeline *pl, pl_conf *run_conf, char *err);

#endif /* _INCLUDE_PIPELINE_RUNTIME_H */