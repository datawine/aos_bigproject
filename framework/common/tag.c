//
// Created by hy on 19-3-19.
//

#include "tag.h"

inline uint32_t* get_function_tag(struct rte_mbuf *pkt) {
    return &(((struct pkt_tag *)&pkt->udata64)->function_tag);
}

inline uint16_t* get_port_tag(struct rte_mbuf *pkt) {
    return &(((struct pkt_tag *)&pkt->udata64)->port_tag);
}

inline uint16_t* get_executor_tag(struct rte_mbuf *pkt) {
    return &(((struct pkt_tag *)&pkt->udata64)->executor_tag);
}