/* Copyright (c) 2024, Meili Authors*/

/* Intrusion Detection Example */

#include "../lib/meili.h"
#include "../runtime/meili_runtime.h"
#include "example.h"


MEILI_INIT(EXAMPLE)
/* allocate space for pipeline state */
self->state = (struct EXAMPLE_state *)malloc(sizeof(struct EXAMPLE_state));
// printf("initializing example app\n");
struct EXAMPLE_state *mystate = (struct EXAMPLE_state *)self->state;
if(!mystate){
    return -ENOMEM;
}

memset(self->state, 0x00, sizeof(struct EXAMPLE_state));

mystate->threshold = DDOS_DEFAULT_THRESH;
mystate->p_window = DDOS_DEFAULT_WINDOW;
mystate->p_set = calloc(mystate->p_window, sizeof(uint32_t));
mystate->p_tot = calloc(mystate->p_window, sizeof(uint32_t));
mystate->p_entropy = calloc(mystate->p_window, sizeof(uint32_t));
mystate->head = 0;

return 0;
MEILI_END_DECLS


MEILI_FREE(EXAMPLE)
struct EXAMPLE_state *mystate = (struct EXAMPLE_state *)self->state;
free(mystate->p_set);
free(mystate->p_tot);
free(mystate->p_entropy);
free(mystate);
return 0;
MEILI_END_DECLS


// Meili dataplane API invocation
MEILI_EXEC(EXAMPLE)
// printf("operating on packets by example app\n");
Meili.pkt_flt(self, &ddos_check, pkt);
Meili.pkt_flt(self, &url_check, pkt);
Meili.pkt_trans(self, &ipsec, pkt);
// Meili.AES(pkt, ERY_TAG,int BLK_SIZE);
MEILI_END_DECLS

MEILI_REGISTER(EXAMPLE)