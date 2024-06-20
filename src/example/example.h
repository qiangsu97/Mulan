/* Copyright (c) 2024, Meili Authors*/

#ifndef _EXAMPLE_H
#define _EXAMPLE_H

#define DDOS_DEFAULT_WINDOW 0xFFF
#define DDOS_DEFAULT_THRESH 1200

MEILI_STATE_DECLS(EXAMPLE)
uint32_t threshold;
uint32_t p_window;
uint32_t *p_set;
uint32_t *p_tot;
uint32_t *p_entropy;
uint32_t head;
uint32_t packet_count;
MEILI_STATE_DECLS_END

int ddos_check(struct pipeline_stage *self, meili_pkt *pkt);
int ipsec(struct pipeline_stage *self, meili_pkt *pkt);
int url_check(struct pipeline_stage *self, meili_pkt *pkt);

#endif