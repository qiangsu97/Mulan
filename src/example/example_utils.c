/* Copyright (c) 2024, Meili Authors*/

#include <math.h>
#include "./libs/sha/sha1.h"
#include "../lib/meili.h"
#include "../runtime/meili_runtime.h"
#include "example.h"

// User-customized dataplane functions
static uint32_t count_bits_64(uint8_t *packet, 
              uint32_t p_len)
{
    uint64_t v, set_bits = 0;
    uint64_t *ptr = (uint64_t *) packet;
    uint64_t *end = (uint64_t *) (packet + p_len);

    while(end > ptr){
        v = *ptr++;
        v = v - ((v >> 1) & 0x5555555555555555);
        v = (v & 0x3333333333333333) + ((v >> 2) & 0x3333333333333333);
        v = (v + (v >> 4)) & 0x0F0F0F0F0F0F0F0F;
        set_bits += (v * 0x0101010101010101) >> (sizeof(v) - 1) * CHAR_BIT;
    }
    return set_bits;
}

static uint32_t
simple_entropy(uint32_t set_bits, 
               uint32_t total_bits)
{
    uint32_t ret;

    ret = (-set_bits) * (log2(set_bits) - log2(total_bits)) -
          (total_bits - set_bits) * (log2(total_bits - set_bits) - 
          log2(total_bits)) + log2(total_bits);

    return ret;
}

int ddos_check(struct pipeline_stage *self, meili_pkt *pkt) {
    const unsigned char *payload = NULL;
    uint32_t p_len = 0;
    struct EXAMPLE_state *mystate = (struct EXAMPLE_state *)self->state;
    int flag = 0; // indicate whether there's an attack
    uint32_t bits;
    uint32_t set ;

    payload = meili_pkt_payload(pkt);
    p_len = meili_pkt_payload_len(pkt);
    bits = p_len * 8;
    set = count_bits_64((uint8_t *)payload, p_len);

    mystate->p_tot[mystate->head] = bits;
    mystate->p_set[mystate->head] = set;
    mystate->p_entropy[mystate->head] = simple_entropy(set, bits);
    mystate->packet_count++;

    if (mystate->packet_count >= mystate->p_window) {
        uint32_t k, total_set = 0, total_bits = 0, sum_entropy = 0;

        for (k = 0; k < mystate->p_window; k++) {
            total_set += mystate->p_set[k];
            total_bits += mystate->p_tot[k];
            sum_entropy += mystate->p_entropy[k];
        }

        uint32_t joint_entropy = simple_entropy(total_set, total_bits);
        if (mystate->threshold < (sum_entropy - joint_entropy)) {
            if (!flag) {
                flag = 1;
            }
        }
    }
    mystate->head = (mystate->head + 1) % mystate->p_window;
    
    return flag;
}
int url_check(struct pipeline_stage *self, meili_pkt *pkt){
    Meili.regex(self, pkt);
    return 0;
}

int ipsec(struct pipeline_stage *self, meili_pkt *pkt){

    char hash_out[SHA_HASH_SIZE+2];
    const unsigned char *pkt_buf;
    int length = 0;

    pkt_buf = meili_pkt_payload(pkt);
    length = meili_pkt_payload_len(pkt);
    SHA1(hash_out, pkt_buf, length);
    
    return 0;
}