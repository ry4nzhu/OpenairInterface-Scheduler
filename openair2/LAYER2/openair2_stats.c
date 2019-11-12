#include<openair2_stats.h>

static mapping rrc_status_names[] = {
  {"RRC_INACTIVE", 0},
  {"RRC_IDLE", 1},
  {"RRC_SI_RECEIVED",2},
  {"RRC_CONNECTED",3},
  {"RRC_RECONFIGURED",4},
  {"RRC_HO_EXECUTION",5},
  {NULL, -1}
};

extern RAN_CONTEXT_t RC;

void dump_eNB_statistics(module_id_t module_idP) {
    eNB_MAC_INST *eNB = RC.mac[module_idP];
    for (CC_id=0 ; CC_id < MAX_NUM_CCs; CC_id++) {
        printf("eNB %d CC %d Frame %d: Active UEs %d, Available PRBs %d, nCCE %d, Scheduling decisions %d, Missed Deadlines %d\n",
                    eNB_id, CC_id, eNB->frame,
                    eNB->eNB_stats[CC_id].num_dlactive_UEs,
                    eNB->eNB_stats[CC_id].available_prbs,
                    eNB->eNB_stats[CC_id].available_ncces,
                    eNB->eNB_stats[CC_id].sched_decisions,
                    eNB->eNB_stats[CC_id].missed_deadlines);
        printf("BCCH , NB_TX_MAC = %d, transmitted bytes (TTI %d, total %d) MCS (TTI %d)\n",
                    eNB->eNB_stats[CC_id].total_num_bcch_pdu,
                    eNB->eNB_stats[CC_id].bcch_buffer,
                    eNB->eNB_stats[CC_id].total_bcch_buffer,
                    eNB->eNB_stats[CC_id].bcch_mcs);
        printf("PCCH , NB_TX_MAC = %d, transmitted bytes (TTI %d, total %d) MCS (TTI %d)\n",
                    eNB->eNB_stats[CC_id].total_num_pcch_pdu,
                    eNB->eNB_stats[CC_id].pcch_buffer,
                    eNB->eNB_stats[CC_id].total_pcch_buffer,
                    eNB->eNB_stats[CC_id].pcch_mcs);

        eNB->eNB_stats[CC_id].dlsch_bitrate=((eNB->eNB_stats[CC_id].dlsch_bytes_tx*8)/((eNB->frame + 1)*10));
        eNB->eNB_stats[CC_id].total_dlsch_pdus_tx+=eNB->eNB_stats[CC_id].dlsch_pdus_tx;
        eNB->eNB_stats[CC_id].total_dlsch_bytes_tx+=eNB->eNB_stats[CC_id].dlsch_bytes_tx;
        eNB->eNB_stats[CC_id].total_dlsch_bitrate=((eNB->eNB_stats[CC_id].total_dlsch_bytes_tx*8)/((eNB->frame + 1)*10));

        eNB->eNB_stats[CC_id].ulsch_bitrate=((eNB->eNB_stats[CC_id].ulsch_bytes_rx*8)/((eNB->frame + 1)*10));
        eNB->eNB_stats[CC_id].total_ulsch_bitrate=((eNB->eNB_stats[CC_id].total_ulsch_bytes_rx*8)/((eNB->frame + 1)*10));

        // DLSCH bitrate
        printf("DLSCH bitrate (TTI %u, avg %u) kbps, Transmitted bytes (TTI %u, total %u), Transmitted PDU (TTI %u, total %u) \n",
                    eNB->eNB_stats[CC_id].dlsch_bitrate,
                    eNB->eNB_stats[CC_id].total_dlsch_bitrate,
                    eNB->eNB_stats[CC_id].dlsch_bytes_tx,
                    eNB->eNB_stats[CC_id].total_dlsch_bytes_tx,
                    eNB->eNB_stats[CC_id].dlsch_pdus_tx,
                    eNB->eNB_stats[CC_id].total_dlsch_pdus_tx);
        
        // ULSCH bitrate
        printf("ULSCH bitrate (TTI %u, avg %u) kbps, Received bytes (TTI %u, total %u), Received PDU (TTI %lu, total %u) \n",
                    eNB->eNB_stats[CC_id].ulsch_bitrate,
                    eNB->eNB_stats[CC_id].total_ulsch_bitrate,
                    eNB->eNB_stats[CC_id].ulsch_bytes_rx,
                    eNB->eNB_stats[CC_id].total_ulsch_bytes_rx,
                    eNB->eNB_stats[CC_id].ulsch_pdus_rx,
                    eNB->eNB_stats[CC_id].total_ulsch_pdus_rx);
        
        // TODO: Add UE-specific parameters

        
    }
}