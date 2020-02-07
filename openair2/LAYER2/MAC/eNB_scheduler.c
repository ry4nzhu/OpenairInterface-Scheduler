/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file eNB_scheduler.c
 * \brief eNB scheduler top level function operates on per subframe basis
 * \author  Navid Nikaein and Raymond Knopp
 * \date 2010 - 2014
 * \email: navid.nikaein@eurecom.fr
 * \version 0.5
 * @ingroup _mac

 */

#include "assertions.h"
#include "targets/RT/USER/lte-softmodem.h"
#include "LAYER2/MAC/mac.h"
#include "LAYER2/MAC/mac_extern.h"
// #include "LAYER2/openair2_stats.h"

#include "LAYER2/MAC/mac_proto.h"
#include "common/utils/LOG/log.h"
#include "common/utils/LOG/vcd_signal_dumper.h"
#include "UTIL/OPT/opt.h"
#include "OCG.h"
#include "OCG_extern.h"

#include "RRC/LTE/rrc_extern.h"
#include "RRC/L2_INTERFACE/openair_rrc_L2_interface.h"

//#include "LAYER2/MAC/pre_processor.c"
#include "pdcp.h"

//Agent-related headers
#include "flexran_agent_extern.h"
#include "flexran_agent_mac.h"

/* for fair round robin SCHED */
#include "eNB_scheduler_fairRR.h"

#if defined(ENABLE_ITTI)
#include "intertask_interface.h"
#endif

#include "assertions.h"

#define ENABLE_MAC_PAYLOAD_DEBUG
#define DEBUG_eNB_SCHEDULER 1

extern RAN_CONTEXT_t RC;

// additional include for getting UE type

#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

// define shared key to infer app type
// define additional app type
#define SHM_KEY 0x1234
#define VideoStreaming 0
#define Web 1
#define VoIP 2

typedef struct shmseg{
    int val;
} shmseg_t;

int shmid = -1;
int _UE_app_type = 0;

int get_ue_app_type() {
  shmseg_t *shmp = NULL;

  if (shmid == -1) {
    shmid = shmget(SHM_KEY, sizeof(struct shmseg), 0666|IPC_CREAT);
    if (shmid == -1) {
      perror("Shared memory error");
      return 1;
    }

    // Attach to the segment to get a pointer to it.
    shmp = shmat(shmid, NULL, 0);
    if (shmp == (void *) -1) {
      perror("Shared memory attach");
      return 1;
    }
    if (shmp == NULL) {
        perror("didnot get shared\n");
        return -1;
    }
  }

  /* Transfer blocks of data from shared memory to stdout*/
  _UE_app_type = shmp->val;
  switch (_UE_app_type) {
    case VideoStreaming:
      printf("UE Application Type: Video Streaming\n");
      break;
    case Web:
      printf("UE Application Type: Web Browsing\n");
      break;
    case VoIP:
      printf("UE Application Type: VoIP\n");
      break;
    default:
      break;
  }

  // Detach
  if (shmid != -1) {
    if (shmdt(shmp) == -1) {
      perror("shmdt");
      return 1;
    }
  }
  shmid = -1;

  return 0;
}



uint16_t pdcch_order_table[6] = { 31, 31, 511, 2047, 2047, 8191 };

static mapping rrc_status_names[] = {
  {"RRC_INACTIVE", 0},
  {"RRC_IDLE", 1},
  {"RRC_SI_RECEIVED",2},
  {"RRC_CONNECTED",3},
  {"RRC_RECONFIGURED",4},
  {"RRC_HO_EXECUTION",5},
  {NULL, -1}
};

void dump_eNB_statistics(module_id_t module_idP) {


  eNB_MAC_INST *eNB = RC.mac[module_idP];
	UE_list_t *UE_list = &eNB->UE_list;
	int CC_id = 0;
	int eNB_id = 0;


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

      if (get_ue_app_type() == 0) // success
        ;
      // CC //

        // check UE_list has active UEs first
      if (UE_list->num_UEs > 0) {
         for (int UE_id=UE_list->head; UE_id>=0; UE_id=UE_list->next[UE_id]) {
             for (int i=0; i<UE_list->numactiveCCs[UE_id]; i++) {
                 CC_id=UE_list->ordered_CCids[i][UE_id];

                 UE_list->eNB_UE_stats[CC_id][UE_id].dlsch_bitrate=((UE_list->eNB_UE_stats[CC_id][UE_id].TBS*8)/((eNB->frame + 1)*10));
                 UE_list->eNB_UE_stats[CC_id][UE_id].total_dlsch_bitrate= ((UE_list->eNB_UE_stats[CC_id][UE_id].total_pdu_bytes*8)/((eNB->frame + 1)*10));
                 UE_list->eNB_UE_stats[CC_id][UE_id].total_overhead_bytes+=  UE_list->eNB_UE_stats[CC_id][UE_id].overhead_bytes;
                 UE_list->eNB_UE_stats[CC_id][UE_id].avg_overhead_bytes=((UE_list->eNB_UE_stats[CC_id][UE_id].total_overhead_bytes*8)/((eNB->frame + 1)*10));

                 UE_list->eNB_UE_stats[CC_id][UE_id].ulsch_bitrate=((UE_list->eNB_UE_stats[CC_id][UE_id].ulsch_TBS*8)/((eNB->frame + 1)*10));
                 UE_list->eNB_UE_stats[CC_id][UE_id].total_ulsch_bitrate= ((UE_list->eNB_UE_stats[CC_id][UE_id].total_pdu_bytes_rx*8)/((eNB->frame + 1)*10));

                 printf("[MAC] UE %d (DLSCH),status %s, RNTI %x : CQI %d, MCS1 %d, MCS2 %d, RB (tx %d, retx %d, total %d), ncce (tx %d, retx %d) \n",
                             UE_id,
                             map_int_to_str(rrc_status_names, UE_list->eNB_UE_stats[CC_id][UE_id].rrc_status),
                             UE_list->eNB_UE_stats[CC_id][UE_id].crnti,
                             UE_list->UE_sched_ctrl[UE_id].dl_cqi[CC_id],
                             UE_list->eNB_UE_stats[CC_id][UE_id].dlsch_mcs1,
                             UE_list->eNB_UE_stats[CC_id][UE_id].dlsch_mcs2,
                             UE_list->eNB_UE_stats[CC_id][UE_id].rbs_used,
                             UE_list->eNB_UE_stats[CC_id][UE_id].rbs_used_retx,
                             UE_list->eNB_UE_stats[CC_id][UE_id].total_rbs_used,
                             UE_list->eNB_UE_stats[CC_id][UE_id].ncce_used,
                             UE_list->eNB_UE_stats[CC_id][UE_id].ncce_used_retx
                             );

                 printf("[MAC] DLSCH bitrate (TTI %d, avg %d), Transmitted bytes "
                             "(TTI %d, total %"PRIu64"), Total Transmitted PDU %d, Overhead "
                             "(TTI %"PRIu64", total %"PRIu64", avg %"PRIu64")\n",
                             UE_list->eNB_UE_stats[CC_id][UE_id].dlsch_bitrate,
                             UE_list->eNB_UE_stats[CC_id][UE_id].total_dlsch_bitrate,
                             UE_list->eNB_UE_stats[CC_id][UE_id].TBS,
                             UE_list->eNB_UE_stats[CC_id][UE_id].total_pdu_bytes,
                             UE_list->eNB_UE_stats[CC_id][UE_id].total_num_pdus,
                             UE_list->eNB_UE_stats[CC_id][UE_id].overhead_bytes,
                             UE_list->eNB_UE_stats[CC_id][UE_id].total_overhead_bytes,
                             UE_list->eNB_UE_stats[CC_id][UE_id].avg_overhead_bytes
                             );


                 printf("[MAC] UE %d (ULSCH), Status %s, Failute timer %d, RNTI %x : rx power (normalized %d,  target %d), MCS (pre %d, post %d), RB (rx %d, retx %d, total %d), Current TBS %d \n",
                         UE_id,
                         map_int_to_str(rrc_status_names, UE_list->eNB_UE_stats[CC_id][UE_id].rrc_status),
                         UE_list->UE_sched_ctrl[UE_id].ul_failure_timer,
                         UE_list->eNB_UE_stats[CC_id][UE_id].crnti,
                         UE_list->eNB_UE_stats[CC_id][UE_id].normalized_rx_power,
                         UE_list->eNB_UE_stats[CC_id][UE_id].target_rx_power,
                         UE_list->eNB_UE_stats[CC_id][UE_id].ulsch_mcs1,
                         UE_list->eNB_UE_stats[CC_id][UE_id].ulsch_mcs2,
                         UE_list->eNB_UE_stats[CC_id][UE_id].rbs_used_rx,
                         UE_list->eNB_UE_stats[CC_id][UE_id].rbs_used_retx_rx,
                         UE_list->eNB_UE_stats[CC_id][UE_id].total_rbs_used_rx,
                         UE_list->eNB_UE_stats[CC_id][UE_id].ulsch_TBS
                     );

                 printf("[MAC] ULSCH bitrate (TTI %d, avg %d), received bytes (total %"PRIu64"),"
                         "Total received PDU %d, Total errors %d\n",
                         UE_list->eNB_UE_stats[CC_id][UE_id].ulsch_bitrate,
                         UE_list->eNB_UE_stats[CC_id][UE_id].total_ulsch_bitrate,
                         UE_list->eNB_UE_stats[CC_id][UE_id].total_pdu_bytes_rx,
                         UE_list->eNB_UE_stats[CC_id][UE_id].total_num_pdus_rx,
                         UE_list->eNB_UE_stats[CC_id][UE_id].num_errors_rx);

                 printf("[MAC] Received PHR PH = %d (db)\n", UE_list->UE_template[CC_id][UE_id].phr_info);
                 printf("[MAC] Estimated size LCGID[0][1][2][3] = %u %u %u %u\n",
                         UE_list->UE_template[CC_id][UE_id].ul_buffer_info[LCGID0],
                         UE_list->UE_template[CC_id][UE_id].ul_buffer_info[LCGID1],
                         UE_list->UE_template[CC_id][UE_id].ul_buffer_info[LCGID2],
                         UE_list->UE_template[CC_id][UE_id].ul_buffer_info[LCGID3]
                         );
             }

//             PROTOCOL_CTXT_SET_BY_MODULE_ID(&ctxt,
//                             eNB_id,
//                             ENB_FLAG_YES,
//                             UE_list->eNB_UE_stats[0][UE_id].crnti,//UE_PCCID(eNB_id,UE_id)][UE_id].crnti,
//                             eNB->frame,
//                             eNB->subframe,
//                             eNB_id);
            /*
            rlc_status = rlc_stat_req(&ctxt,
                        SRB_FLAG_YES,
                        DCCH,
                        &stat_rlc_mode,
                        &stat_tx_pdcp_sdu,
                        &stat_tx_pdcp_bytes,
                        &stat_tx_pdcp_sdu_discarded,
                        &stat_tx_pdcp_bytes_discarded,
                        &stat_tx_data_pdu,
                        &stat_tx_data_bytes,
                        &stat_tx_retransmit_pdu_by_status,
                        &stat_tx_retransmit_bytes_by_status,
                        &stat_tx_retransmit_pdu,
                        &stat_tx_retransmit_bytes,
                        &stat_tx_control_pdu,
                        &stat_tx_control_bytes,
                        &stat_rx_pdcp_sdu,
                        &stat_rx_pdcp_bytes,
                        &stat_rx_data_pdus_duplicate,
                        &stat_rx_data_bytes_duplicate,
                        &stat_rx_data_pdu,
                        &stat_rx_data_bytes,
                        &stat_rx_data_pdu_dropped,
                        &stat_rx_data_bytes_dropped,
                        &stat_rx_data_pdu_out_of_window,
                        &stat_rx_data_bytes_out_of_window,
                        &stat_rx_control_pdu,
                        &stat_rx_control_bytes,
                        &stat_timer_reordering_timed_out,
                        &stat_timer_poll_retransmit_timed_out,
                        &stat_timer_status_prohibit_timed_out);

            if (rlc_status == RLC_OP_STATUS_OK) {
            len+=sprintf(&buffer[len],"[RLC] DCCH Mode %s, NB_SDU_TO_TX = %d (bytes %d)\tNB_SDU_TO_TX_DISC %d (bytes %d)\n",
                    (stat_rlc_mode==RLC_MODE_AM)? "AM": (stat_rlc_mode==RLC_MODE_UM)?"UM":"NONE",
                    stat_tx_pdcp_sdu,
                    stat_tx_pdcp_bytes,
                    stat_tx_pdcp_sdu_discarded,
                    stat_tx_pdcp_bytes_discarded);

            len+=sprintf(&buffer[len],"[RLC] DCCH Mode %s, NB_TX_DATA   = %d (bytes %d)\tNB_TX_CONTROL %d (bytes %d)\tNB_TX_RETX %d (bytes %d)\tNB_TX_RETX_BY_STATUS = %d (bytes %d)\n",
                    (stat_rlc_mode==RLC_MODE_AM)? "AM": (stat_rlc_mode==RLC_MODE_UM)?"UM":"NONE",
                    stat_tx_data_pdu,
                    stat_tx_data_bytes,
                    stat_tx_control_pdu,
                    stat_tx_control_bytes,
                    stat_tx_retransmit_pdu,
                    stat_tx_retransmit_bytes,
                    stat_tx_retransmit_pdu_by_status,
                    stat_tx_retransmit_bytes_by_status);


            len+=sprintf(&buffer[len],"[RLC] DCCH Mode %s, NB_RX_DATA   = %d (bytes %d)\tNB_RX_CONTROL %d (bytes %d)\tNB_RX_DUPL %d (bytes %d)\tNB_RX_DROP = %d (bytes %d)\tNB_RX_OUT_OF_WINDOW = %d (bytes %d)\n",
                    (stat_rlc_mode==RLC_MODE_AM)? "AM": (stat_rlc_mode==RLC_MODE_UM)?"UM":"NONE",
                    stat_rx_data_pdu,
                    stat_rx_data_bytes,
                    stat_rx_control_pdu,
                    stat_rx_control_bytes,
                    stat_rx_data_pdus_duplicate,
                    stat_rx_data_bytes_duplicate,
                    stat_rx_data_pdu_dropped,
                    stat_rx_data_bytes_dropped,
                    stat_rx_data_pdu_out_of_window,
                    stat_rx_data_bytes_out_of_window);


            len+=sprintf(&buffer[len],"[RLC] DCCH Mode %s, RX_REODERING_TIMEOUT = %d\tRX_POLL_RET_TIMEOUT %d\tRX_PROHIBIT_TIME_OUT %d\n",
                    (stat_rlc_mode==RLC_MODE_AM)? "AM": (stat_rlc_mode==RLC_MODE_UM)?"UM":"NONE",
                    stat_timer_reordering_timed_out,
                    stat_timer_poll_retransmit_timed_out,
                    stat_timer_status_prohibit_timed_out);

            len+=sprintf(&buffer[len],"[RLC] DCCH Mode %s, NB_SDU_TO_RX = %d (bytes %d)\n",
                    (stat_rlc_mode==RLC_MODE_AM)? "AM": (stat_rlc_mode==RLC_MODE_UM)?"UM":"NONE",
                    stat_rx_pdcp_sdu,
                    stat_rx_pdcp_bytes);
            }

            rlc_status = rlc_stat_req(&ctxt,
                        SRB_FLAG_NO,
                        DTCH-2, // DRB_IDENTITY
                        &stat_rlc_mode,
                        &stat_tx_pdcp_sdu,
                        &stat_tx_pdcp_bytes,
                        &stat_tx_pdcp_sdu_discarded,
                        &stat_tx_pdcp_bytes_discarded,
                        &stat_tx_data_pdu,
                        &stat_tx_data_bytes,
                        &stat_tx_retransmit_pdu_by_status,
                        &stat_tx_retransmit_bytes_by_status,
                        &stat_tx_retransmit_pdu,
                        &stat_tx_retransmit_bytes,
                        &stat_tx_control_pdu,
                        &stat_tx_control_bytes,
                        &stat_rx_pdcp_sdu,
                        &stat_rx_pdcp_bytes,
                        &stat_rx_data_pdus_duplicate,
                        &stat_rx_data_bytes_duplicate,
                        &stat_rx_data_pdu,
                        &stat_rx_data_bytes,
                        &stat_rx_data_pdu_dropped,
                        &stat_rx_data_bytes_dropped,
                        &stat_rx_data_pdu_out_of_window,
                        &stat_rx_data_bytes_out_of_window,
                        &stat_rx_control_pdu,
                        &stat_rx_control_bytes,
                        &stat_timer_reordering_timed_out,
                        &stat_timer_poll_retransmit_timed_out,
                        &stat_timer_status_prohibit_timed_out);

            if (rlc_status == RLC_OP_STATUS_OK) {
            len+=sprintf(&buffer[len],"[RLC] DTCH Mode %s, NB_SDU_TO_TX = %d (bytes %d)\tNB_SDU_TO_TX_DISC %d (bytes %d)\n",
                    (stat_rlc_mode==RLC_MODE_AM)? "AM": (stat_rlc_mode==RLC_MODE_UM)?"UM":"NONE",
                    stat_tx_pdcp_sdu,
                    stat_tx_pdcp_bytes,
                    stat_tx_pdcp_sdu_discarded,
                    stat_tx_pdcp_bytes_discarded);

            len+=sprintf(&buffer[len],"[RLC] DTCH Mode %s, NB_TX_DATA   = %d (bytes %d)\tNB_TX_CONTROL %d (bytes %d)\tNB_TX_RETX %d (bytes %d)\tNB_TX_RETX_BY_STATUS = %d (bytes %d)\n",
                    (stat_rlc_mode==RLC_MODE_AM)? "AM": (stat_rlc_mode==RLC_MODE_UM)?"UM":"NONE",
                    stat_tx_data_pdu,
                    stat_tx_data_bytes,
                    stat_tx_control_pdu,
                    stat_tx_control_bytes,
                    stat_tx_retransmit_pdu,
                    stat_tx_retransmit_bytes,
                    stat_tx_retransmit_pdu_by_status,
                    stat_tx_retransmit_bytes_by_status);


            len+=sprintf(&buffer[len],"[RLC] DTCH Mode %s, NB_RX_DATA   = %d (bytes %d)\tNB_RX_CONTROL %d (bytes %d)\tNB_RX_DUPL %d (bytes %d)\tNB_RX_DROP = %d (bytes %d)\tNB_RX_OUT_OF_WINDOW = %d (bytes %d)\n",
                    (stat_rlc_mode==RLC_MODE_AM)? "AM": (stat_rlc_mode==RLC_MODE_UM)?"UM":"NONE",
                    stat_rx_data_pdu,
                    stat_rx_data_bytes,
                    stat_rx_control_pdu,
                    stat_rx_control_bytes,
                    stat_rx_data_pdus_duplicate,
                    stat_rx_data_bytes_duplicate,
                    stat_rx_data_pdu_dropped,
                    stat_rx_data_bytes_dropped,
                    stat_rx_data_pdu_out_of_window,
                    stat_rx_data_bytes_out_of_window);


            len+=sprintf(&buffer[len],"[RLC] DTCH Mode %s, RX_REODERING_TIMEOUT = %d\tRX_POLL_RET_TIMEOUT %d\tRX_PROHIBIT_TIME_OUT %d\n",
                    (stat_rlc_mode==RLC_MODE_AM)? "AM": (stat_rlc_mode==RLC_MODE_UM)?"UM":"NONE",
                    stat_timer_reordering_timed_out,
                    stat_timer_poll_retransmit_timed_out,
                    stat_timer_status_prohibit_timed_out);

            len+=sprintf(&buffer[len],"[RLC] DTCH Mode %s, NB_SDU_TO_RX = %d (bytes %d)\n",
                    (stat_rlc_mode==RLC_MODE_AM)? "AM": (stat_rlc_mode==RLC_MODE_UM)?"UM":"NONE",
                    stat_rx_pdcp_sdu,
                    stat_rx_pdcp_bytes);
                    */



//            }

         }
      }
    }
}



void
schedule_SRS(module_id_t module_idP, frame_t frameP, sub_frame_t subframeP)
{


  eNB_MAC_INST *eNB = RC.mac[module_idP];
  UE_list_t *UE_list = &eNB->UE_list;
  nfapi_ul_config_request_body_t *ul_req;
  int CC_id, UE_id;
  COMMON_channels_t *cc = RC.mac[module_idP]->common_channels;
  LTE_SoundingRS_UL_ConfigCommon_t *soundingRS_UL_ConfigCommon;
  struct LTE_SoundingRS_UL_ConfigDedicated *soundingRS_UL_ConfigDedicated;
  uint8_t TSFC;
  uint16_t deltaTSFC;		// bitmap
  uint8_t srs_SubframeConfig;
  
  // table for TSFC (Period) and deltaSFC (offset)
  const uint16_t deltaTSFCTabType1[15][2] = { {1, 1}, {1, 2}, {2, 2}, {1, 5}, {2, 5}, {4, 5}, {8, 5}, {3, 5}, {12, 5}, {1, 10}, {2, 10}, {4, 10}, {8, 10}, {351, 10}, {383, 10} };	// Table 5.5.3.3-2 3GPP 36.211 FDD
  const uint16_t deltaTSFCTabType2[14][2] = { {2, 5}, {6, 5}, {10, 5}, {18, 5}, {14, 5}, {22, 5}, {26, 5}, {30, 5}, {70, 10}, {74, 10}, {194, 10}, {326, 10}, {586, 10}, {210, 10} };	// Table 5.5.3.3-2 3GPP 36.211 TDD
  
  uint16_t srsPeriodicity, srsOffset;
  
  for (CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {
    // printf("Available PRBS: %d\n", eNB->eNB_stats[CC_id].available_prbs);
    soundingRS_UL_ConfigCommon = &cc[CC_id].radioResourceConfigCommon->soundingRS_UL_ConfigCommon;
    // check if SRS is enabled in this frame/subframe
    if (soundingRS_UL_ConfigCommon) {
      srs_SubframeConfig = soundingRS_UL_ConfigCommon->choice.setup.srs_SubframeConfig;
      if (cc[CC_id].tdd_Config == NULL) {	// FDD
        deltaTSFC = deltaTSFCTabType1[srs_SubframeConfig][0];
        TSFC = deltaTSFCTabType1[srs_SubframeConfig][1];
      } else {		// TDD
        deltaTSFC = deltaTSFCTabType2[srs_SubframeConfig][0];
        TSFC = deltaTSFCTabType2[srs_SubframeConfig][1];
      }
      // Sounding reference signal subframes are the subframes satisfying ns/2 mod TSFC (- deltaTSFC
      uint16_t tmp = (subframeP % TSFC);
      
      if ((1 << tmp) & deltaTSFC) {
        // This is an SRS subframe, loop over UEs
        for (UE_id = 0; UE_id < MAX_MOBILES_PER_ENB; UE_id++) {
          if (!RC.mac[module_idP]->UE_list.active[UE_id]) continue;
          ul_req = &RC.mac[module_idP]->UL_req[CC_id].ul_config_request_body;
          // drop the allocation if the UE hasn't send RRCConnectionSetupComplete yet
          if (mac_eNB_get_rrc_status(module_idP,UE_RNTI(module_idP, UE_id)) < RRC_CONNECTED) continue;
          
          AssertFatal(UE_list->UE_template[CC_id][UE_id].physicalConfigDedicated != NULL,
                "physicalConfigDedicated is null for UE %d\n",
                UE_id);
          
          if ((soundingRS_UL_ConfigDedicated = UE_list->UE_template[CC_id][UE_id].physicalConfigDedicated->soundingRS_UL_ConfigDedicated) != NULL) {
            if (soundingRS_UL_ConfigDedicated->present == LTE_SoundingRS_UL_ConfigDedicated_PR_setup) {
              get_srs_pos(&cc[CC_id],
              soundingRS_UL_ConfigDedicated->choice.
              setup.srs_ConfigIndex,
              &srsPeriodicity, &srsOffset);
              if (((10 * frameP + subframeP) % srsPeriodicity) == srsOffset) {
                // Program SRS
                ul_req->srs_present = 1;
                nfapi_ul_config_request_pdu_t * ul_config_pdu = &ul_req->ul_config_pdu_list[ul_req->number_of_pdus];
                memset((void *) ul_config_pdu, 0, sizeof(nfapi_ul_config_request_pdu_t));
                ul_config_pdu->pdu_type =  NFAPI_UL_CONFIG_SRS_PDU_TYPE;
                ul_config_pdu->pdu_size =  2 + (uint8_t) (2 + sizeof(nfapi_ul_config_srs_pdu));
                ul_config_pdu->srs_pdu.srs_pdu_rel8.tl.tag = NFAPI_UL_CONFIG_REQUEST_SRS_PDU_REL8_TAG;
                ul_config_pdu->srs_pdu.srs_pdu_rel8.size = (uint8_t)sizeof(nfapi_ul_config_srs_pdu);
                ul_config_pdu->srs_pdu.srs_pdu_rel8.rnti = UE_list->UE_template[CC_id][UE_id].rnti;
                ul_config_pdu->srs_pdu.srs_pdu_rel8.srs_bandwidth = soundingRS_UL_ConfigDedicated->choice.setup.srs_Bandwidth;
                ul_config_pdu->srs_pdu.srs_pdu_rel8.frequency_domain_position = soundingRS_UL_ConfigDedicated->choice.setup.freqDomainPosition;
                ul_config_pdu->srs_pdu.srs_pdu_rel8.srs_hopping_bandwidth = soundingRS_UL_ConfigDedicated->choice.setup.srs_HoppingBandwidth;;
                ul_config_pdu->srs_pdu.srs_pdu_rel8.transmission_comb = soundingRS_UL_ConfigDedicated->choice.setup.transmissionComb;
                ul_config_pdu->srs_pdu.srs_pdu_rel8.i_srs = soundingRS_UL_ConfigDedicated->choice.setup.srs_ConfigIndex;
                ul_config_pdu->srs_pdu.srs_pdu_rel8.sounding_reference_cyclic_shift = soundingRS_UL_ConfigDedicated->choice.setup.cyclicShift;		//              ul_config_pdu->srs_pdu.srs_pdu_rel10.antenna_port                   = ;//
                //              ul_config_pdu->srs_pdu.srs_pdu_rel13.number_of_combs                = ;//
                RC.mac[module_idP]->UL_req[CC_id].sfn_sf = (frameP << 4) + subframeP;
                RC.mac[module_idP]->UL_req[CC_id].header.message_id = NFAPI_UL_CONFIG_REQUEST;
                ul_req->number_of_pdus++;
              }	// if (((10*frameP+subframeP) % srsPeriodicity) == srsOffset)
            }	// if (soundingRS_UL_ConfigDedicated->present == SoundingRS_UL_ConfigDedicated_PR_setup)
          }		// if ((soundingRS_UL_ConfigDedicated = UE_list->UE_template[CC_id][UE_id].physicalConfigDedicated->soundingRS_UL_ConfigDedicated)!=NULL)
        }		// for (UE_id ...
      }			// if((1<<tmp) & deltaTSFC)
      
    }			// SRS config
  }
}

void
schedule_CSI(module_id_t module_idP, frame_t frameP, sub_frame_t subframeP)
{
  eNB_MAC_INST                   *eNB = RC.mac[module_idP];
  UE_list_t                      *UE_list = &eNB->UE_list;
  COMMON_channels_t              *cc;
  nfapi_ul_config_request_body_t *ul_req;
  int                            CC_id, UE_id;
  struct LTE_CQI_ReportPeriodic  *cqi_ReportPeriodic;
  uint16_t                       Npd, N_OFFSET_CQI;
  int                            H;

  for (CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {

    cc = &eNB->common_channels[CC_id];
    for (UE_id = 0; UE_id < MAX_MOBILES_PER_ENB; UE_id++) {
      if (!UE_list->active[UE_id]) continue;

      ul_req = &RC.mac[module_idP]->UL_req[CC_id].ul_config_request_body;

      // drop the allocation if the UE hasn't send RRCConnectionSetupComplete yet
      if (mac_eNB_get_rrc_status(module_idP, UE_RNTI(module_idP, UE_id)) < RRC_CONNECTED) continue;

      AssertFatal(UE_list->
		  UE_template[CC_id][UE_id].physicalConfigDedicated
		  != NULL,
		  "physicalConfigDedicated is null for UE %d\n",
		  UE_id);

      if (UE_list->UE_template[CC_id][UE_id].physicalConfigDedicated->cqi_ReportConfig) {
	if ((cqi_ReportPeriodic = UE_list->UE_template[CC_id][UE_id].physicalConfigDedicated->cqi_ReportConfig->cqi_ReportPeriodic) != NULL
	    && (cqi_ReportPeriodic->present != LTE_CQI_ReportPeriodic_PR_release)) {
	  //Rel8 Periodic CQI/PMI/RI reporting

	  get_csi_params(cc, cqi_ReportPeriodic, &Npd,
			 &N_OFFSET_CQI, &H);

	  if ((((frameP * 10) + subframeP) % Npd) == N_OFFSET_CQI) {	// CQI opportunity
	    UE_list->UE_sched_ctrl[UE_id].feedback_cnt[CC_id] = (((frameP * 10) + subframeP) / Npd) % H;
	    // Program CQI
	    nfapi_ul_config_request_pdu_t *ul_config_pdu = &ul_req->ul_config_pdu_list[ul_req->number_of_pdus];
	    memset((void *) ul_config_pdu, 0,
		   sizeof(nfapi_ul_config_request_pdu_t));
	    ul_config_pdu->pdu_type                                                          = NFAPI_UL_CONFIG_UCI_CQI_PDU_TYPE;
	    ul_config_pdu->pdu_size                                                          = 2 + (uint8_t) (2 + sizeof(nfapi_ul_config_uci_cqi_pdu));
	    ul_config_pdu->uci_cqi_pdu.ue_information.ue_information_rel8.tl.tag             = NFAPI_UL_CONFIG_REQUEST_UE_INFORMATION_REL8_TAG;
	    ul_config_pdu->uci_cqi_pdu.ue_information.ue_information_rel8.rnti               = UE_list->UE_template[CC_id][UE_id].rnti;
	    ul_config_pdu->uci_cqi_pdu.cqi_information.cqi_information_rel8.tl.tag           = NFAPI_UL_CONFIG_REQUEST_CQI_INFORMATION_REL8_TAG;
	    ul_config_pdu->uci_cqi_pdu.cqi_information.cqi_information_rel8.pucch_index      = cqi_ReportPeriodic->choice.setup.cqi_PUCCH_ResourceIndex;
	    ul_config_pdu->uci_cqi_pdu.cqi_information.cqi_information_rel8.dl_cqi_pmi_size  = get_rel8_dl_cqi_pmi_size(&UE_list->UE_sched_ctrl[UE_id], CC_id, cc,
															get_tmode(module_idP, CC_id, UE_id),
															cqi_ReportPeriodic);
	    ul_req->number_of_pdus++;
	    ul_req->tl.tag                                                                   = NFAPI_UL_CONFIG_REQUEST_BODY_TAG;

#if (LTE_RRC_VERSION >= MAKE_VERSION(10, 0, 0))
	    // PUT rel10-13 UCI options here
#endif
	  } else
	    if ((cqi_ReportPeriodic->choice.setup.ri_ConfigIndex)
		&& ((((frameP * 10) + subframeP) % ((H * Npd) << (*cqi_ReportPeriodic->choice.setup.ri_ConfigIndex / 161))) == N_OFFSET_CQI + (*cqi_ReportPeriodic->choice.setup.ri_ConfigIndex % 161))) {	// RI opportunity
	      // Program RI
	      nfapi_ul_config_request_pdu_t *ul_config_pdu = &ul_req->ul_config_pdu_list[ul_req->number_of_pdus];
	      memset((void *) ul_config_pdu, 0,
		     sizeof(nfapi_ul_config_request_pdu_t));
	      ul_config_pdu->pdu_type                                                          = NFAPI_UL_CONFIG_UCI_CQI_PDU_TYPE;
	      ul_config_pdu->pdu_size                                                          = 2 + (uint8_t) (2 + sizeof(nfapi_ul_config_uci_cqi_pdu));
	      ul_config_pdu->uci_cqi_pdu.ue_information.ue_information_rel8.tl.tag             = NFAPI_UL_CONFIG_REQUEST_UE_INFORMATION_REL8_TAG;
	      ul_config_pdu->uci_cqi_pdu.ue_information.ue_information_rel8.rnti               = UE_list->UE_template[CC_id][UE_id].rnti;
	      ul_config_pdu->uci_cqi_pdu.cqi_information.cqi_information_rel8.tl.tag           = NFAPI_UL_CONFIG_REQUEST_CQI_INFORMATION_REL8_TAG;
	      ul_config_pdu->uci_cqi_pdu.cqi_information.cqi_information_rel8.pucch_index      = cqi_ReportPeriodic->choice.setup.cqi_PUCCH_ResourceIndex;
	      ul_config_pdu->uci_cqi_pdu.cqi_information.cqi_information_rel8.dl_cqi_pmi_size  = (cc->p_eNB == 2) ? 1 : 2;
	      RC.mac[module_idP]->UL_req[CC_id].sfn_sf                                         = (frameP << 4) + subframeP;
	      ul_req->number_of_pdus++;
	      ul_req->tl.tag                                                                   = NFAPI_UL_CONFIG_REQUEST_BODY_TAG;
	    }
	}		// if ((cqi_ReportPeriodic = cqi_ReportConfig->cqi_ReportPeriodic)!=NULL) {
      }			// if (UE_list->UE_template[CC_id][UE_id].physicalConfigDedicated->cqi_ReportConfig)
    }			// for (UE_id=UE_list->head; UE_id>=0; UE_id=UE_list->next[UE_id]) {
  }				// for (CC_id=0; CC_id<MAX_NUM_CCs; CC_id++) {
}

void
schedule_SR(module_id_t module_idP, frame_t frameP, sub_frame_t subframeP)
{
  eNB_MAC_INST                   *eNB = RC.mac[module_idP];
  UE_list_t                      *UE_list = &eNB->UE_list;
  nfapi_ul_config_request_t      *ul_req;
  nfapi_ul_config_request_body_t *ul_req_body;
  int                            CC_id;
  int                            UE_id;
  LTE_SchedulingRequestConfig_t  *SRconfig;
  int                            skip_ue;
  int                            is_harq;
  nfapi_ul_config_sr_information sr;
  int                            i;

  for (CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {
    RC.mac[module_idP]->UL_req[CC_id].sfn_sf = (frameP << 4) + subframeP;

    for (UE_id = 0; UE_id < MAX_MOBILES_PER_ENB; UE_id++) {
      if (!RC.mac[module_idP]->UE_list.active[UE_id]) continue;

      ul_req        = &RC.mac[module_idP]->UL_req[CC_id];
      ul_req_body   = &ul_req->ul_config_request_body;

      // drop the allocation if the UE hasn't send RRCConnectionSetupComplete yet
      if (mac_eNB_get_rrc_status(module_idP, UE_RNTI(module_idP, UE_id)) < RRC_CONNECTED) continue;

      AssertFatal(UE_list->
		  UE_template[CC_id][UE_id].physicalConfigDedicated!= NULL,
		  "physicalConfigDedicated is null for UE %d\n",
		  UE_id);

      if ((SRconfig = UE_list->UE_template[CC_id][UE_id].physicalConfigDedicated->schedulingRequestConfig) != NULL) {
	if (SRconfig->present == LTE_SchedulingRequestConfig_PR_setup) {
	  if (SRconfig->choice.setup.sr_ConfigIndex <= 4) {	// 5 ms SR period
	    if ((subframeP % 5) != SRconfig->choice.setup.sr_ConfigIndex) continue;
	  } else if (SRconfig->choice.setup.sr_ConfigIndex <= 14) {	// 10 ms SR period
	    if (subframeP != (SRconfig->choice.setup.sr_ConfigIndex - 5)) continue;
	  } else if (SRconfig->choice.setup.sr_ConfigIndex <= 34) {	// 20 ms SR period
	    if ((10 * (frameP & 1) + subframeP) != (SRconfig->choice.setup.sr_ConfigIndex - 15)) continue;
	  } else if (SRconfig->choice.setup.sr_ConfigIndex <= 74) {	// 40 ms SR period
	    if ((10 * (frameP & 3) + subframeP) != (SRconfig->choice.setup.sr_ConfigIndex - 35)) continue;
	  } else if (SRconfig->choice.setup.sr_ConfigIndex <= 154) {	// 80 ms SR period
	    if ((10 * (frameP & 7) + subframeP) != (SRconfig->choice.setup.sr_ConfigIndex - 75)) continue;
	  }
	}		// SRconfig->present == SchedulingRequestConfig_PR_setup)
      }			// SRconfig = UE_list->UE_template[CC_id][UE_id].physicalConfigDedicated->schedulingRequestConfig)!=NULL)

      // if we get here there is some PUCCH1 reception to schedule for SR

      skip_ue = 0;
      is_harq = 0;
      // check that there is no existing UL grant for ULSCH which overrides the SR
      for (i = 0; i < ul_req_body->number_of_pdus; i++) {
	if (((ul_req_body->ul_config_pdu_list[i].pdu_type == NFAPI_UL_CONFIG_ULSCH_PDU_TYPE) || 
	     (ul_req_body->ul_config_pdu_list[i].pdu_type == NFAPI_UL_CONFIG_ULSCH_HARQ_PDU_TYPE) || 
	     (ul_req_body->ul_config_pdu_list[i].pdu_type == NFAPI_UL_CONFIG_ULSCH_CQI_RI_PDU_TYPE) || 
	     (ul_req_body->ul_config_pdu_list[i].pdu_type == NFAPI_UL_CONFIG_ULSCH_CQI_HARQ_RI_PDU_TYPE)) && 
	    (ul_req_body->ul_config_pdu_list[i].ulsch_pdu.ulsch_pdu_rel8.rnti == UE_list->UE_template[CC_id][UE_id].rnti)) {
	  skip_ue = 1;
	  break;
	}
	/* if there is already an HARQ pdu, convert to SR_HARQ */
	else if ((ul_req_body->ul_config_pdu_list[i].pdu_type == NFAPI_UL_CONFIG_UCI_HARQ_PDU_TYPE) && 
		 (ul_req_body->ul_config_pdu_list[i].uci_harq_pdu.ue_information.ue_information_rel8.rnti == UE_list->UE_template[CC_id][UE_id].rnti)) {
	  is_harq = 1;
	  break;
	}
      }

      // drop the allocation because ULSCH with handle it with BSR
      if (skip_ue == 1) continue;

      LOG_D(MAC,"Frame %d, Subframe %d : Scheduling SR for UE %d/%x is_harq:%d\n",frameP,subframeP,UE_id,UE_list->UE_template[CC_id][UE_id].rnti, is_harq);

      // check Rel10 or Rel8 SR
#if (LTE_RRC_VERSION >= MAKE_VERSION(10, 0, 0))
      if ((UE_list-> UE_template[CC_id][UE_id].physicalConfigDedicated->ext2)
	  && (UE_list->UE_template[CC_id][UE_id].physicalConfigDedicated->ext2->schedulingRequestConfig_v1020)
	  && (UE_list->UE_template[CC_id][UE_id].physicalConfigDedicated->ext2->schedulingRequestConfig_v1020)) {
	sr.sr_information_rel10.tl.tag                    = NFAPI_UL_CONFIG_REQUEST_SR_INFORMATION_REL10_TAG;
	sr.sr_information_rel10.number_of_pucch_resources = 1;
	sr.sr_information_rel10.pucch_index_p1            = *UE_list->UE_template[CC_id][UE_id].physicalConfigDedicated->ext2->schedulingRequestConfig_v1020->sr_PUCCH_ResourceIndexP1_r10;
	LOG_D(MAC,"REL10 PUCCH INDEX P1:%d\n", sr.sr_information_rel10.pucch_index_p1);
      } else
#endif
	{
	  sr.sr_information_rel8.tl.tag                   = NFAPI_UL_CONFIG_REQUEST_SR_INFORMATION_REL8_TAG;
	  sr.sr_information_rel8.pucch_index              = UE_list->UE_template[CC_id][UE_id].physicalConfigDedicated->schedulingRequestConfig->choice.setup.sr_PUCCH_ResourceIndex;
	  LOG_D(MAC,"REL8 PUCCH INDEX:%d\n", sr.sr_information_rel8.pucch_index);
	}

      /* if there is already an HARQ pdu, convert to SR_HARQ */
      if (is_harq) {
	nfapi_ul_config_harq_information h                                                                                 = ul_req_body->ul_config_pdu_list[i].uci_harq_pdu.harq_information;
	ul_req_body->ul_config_pdu_list[i].pdu_type                                                                        = NFAPI_UL_CONFIG_UCI_SR_HARQ_PDU_TYPE;
	ul_req_body->ul_config_pdu_list[i].uci_sr_harq_pdu.sr_information                                                  = sr;
	ul_req_body->ul_config_pdu_list[i].uci_sr_harq_pdu.harq_information                                                = h;
      } else {
	ul_req_body->ul_config_pdu_list[ul_req_body->number_of_pdus].pdu_type                                              = NFAPI_UL_CONFIG_UCI_SR_PDU_TYPE;
	ul_req_body->ul_config_pdu_list[ul_req_body->number_of_pdus].uci_sr_pdu.ue_information.ue_information_rel8.tl.tag  = NFAPI_UL_CONFIG_REQUEST_UE_INFORMATION_REL8_TAG;
	ul_req_body->ul_config_pdu_list[ul_req_body->number_of_pdus].uci_sr_pdu.ue_information.ue_information_rel8.rnti    = UE_list->UE_template[CC_id][UE_id].rnti;
	ul_req_body->ul_config_pdu_list[ul_req_body->number_of_pdus].uci_sr_pdu.ue_information.ue_information_rel11.tl.tag = 0;
	ul_req_body->ul_config_pdu_list[ul_req_body->number_of_pdus].uci_sr_pdu.ue_information.ue_information_rel13.tl.tag = 0;
	ul_req_body->ul_config_pdu_list[ul_req_body->number_of_pdus].uci_sr_pdu.sr_information                             = sr;
	ul_req_body->number_of_pdus++;
      }			/* if (is_harq) */
      ul_req_body->tl.tag                                                                                                  = NFAPI_UL_CONFIG_REQUEST_BODY_TAG;
    }			// for (UE_id=UE_list->head; UE_id>=0; UE_id=UE_list->next[UE_id])
  }				// for (CC_id=0; CC_id<MAX_NUM_CCs; CC_id++)
}

extern uint8_t nfapi_mode;

void
check_ul_failure(module_id_t module_idP, int CC_id, int UE_id,
		 frame_t frameP, sub_frame_t subframeP)
{
  UE_list_t                 *UE_list = &RC.mac[module_idP]->UE_list;
  nfapi_dl_config_request_t  *DL_req = &RC.mac[module_idP]->DL_req[0];
  uint16_t                      rnti = UE_RNTI(module_idP, UE_id);
  COMMON_channels_t              *cc = RC.mac[module_idP]->common_channels;

  // check uplink failure
  if ((UE_list->UE_sched_ctrl[UE_id].ul_failure_timer > 0) &&
      (UE_list->UE_sched_ctrl[UE_id].ul_out_of_sync == 0)) {
    if (UE_list->UE_sched_ctrl[UE_id].ul_failure_timer == 1)
      LOG_I(MAC, "UE %d rnti %x: UL Failure timer %d \n", UE_id, rnti,
            UE_list->UE_sched_ctrl[UE_id].ul_failure_timer);
    if (UE_list->UE_sched_ctrl[UE_id].ra_pdcch_order_sent == 0) {
      UE_list->UE_sched_ctrl[UE_id].ra_pdcch_order_sent = 1;

      // add a format 1A dci for this UE to request an RA procedure (only one UE per subframe)
      nfapi_dl_config_request_pdu_t *dl_config_pdu                    = &DL_req[CC_id].dl_config_request_body.dl_config_pdu_list[DL_req[CC_id].dl_config_request_body.number_pdu];
      memset((void *) dl_config_pdu, 0,sizeof(nfapi_dl_config_request_pdu_t));
      dl_config_pdu->pdu_type                                         = NFAPI_DL_CONFIG_DCI_DL_PDU_TYPE;
      dl_config_pdu->pdu_size                                         = (uint8_t) (2 + sizeof(nfapi_dl_config_dci_dl_pdu));
      dl_config_pdu->dci_dl_pdu.dci_dl_pdu_rel8.tl.tag                = NFAPI_DL_CONFIG_REQUEST_DCI_DL_PDU_REL8_TAG;
      dl_config_pdu->dci_dl_pdu.dci_dl_pdu_rel8.dci_format            = NFAPI_DL_DCI_FORMAT_1A;
      dl_config_pdu->dci_dl_pdu.dci_dl_pdu_rel8.aggregation_level     = get_aggregation(get_bw_index(module_idP, CC_id),
											UE_list->UE_sched_ctrl[UE_id].
											dl_cqi[CC_id], format1A);
      dl_config_pdu->dci_dl_pdu.dci_dl_pdu_rel8.rnti                  = rnti;
      dl_config_pdu->dci_dl_pdu.dci_dl_pdu_rel8.rnti_type             = 1;	// CRNTI : see Table 4-10 from SCF082 - nFAPI specifications
      dl_config_pdu->dci_dl_pdu.dci_dl_pdu_rel8.transmission_power    = 6000;	// equal to RS power

      AssertFatal((cc[CC_id].mib->message.dl_Bandwidth >= 0) && (cc[CC_id].mib->message.dl_Bandwidth < 6),
		  "illegal dl_Bandwidth %d\n",
		  (int) cc[CC_id].mib->message.dl_Bandwidth);
      dl_config_pdu->dci_dl_pdu.dci_dl_pdu_rel8.resource_block_coding = pdcch_order_table[cc[CC_id].mib->message.dl_Bandwidth];
      DL_req[CC_id].dl_config_request_body.number_dci++;
      DL_req[CC_id].dl_config_request_body.number_pdu++;
      DL_req[CC_id].dl_config_request_body.tl.tag                      = NFAPI_DL_CONFIG_REQUEST_BODY_TAG;
      LOG_D(MAC,
	    "UE %d rnti %x: sending PDCCH order for RAPROC (failure timer %d), resource_block_coding %d \n",
	    UE_id, rnti,
	    UE_list->UE_sched_ctrl[UE_id].ul_failure_timer,
	    dl_config_pdu->dci_dl_pdu.
	    dci_dl_pdu_rel8.resource_block_coding);
    } else {		// ra_pdcch_sent==1
      LOG_D(MAC,
	    "UE %d rnti %x: sent PDCCH order for RAPROC waiting (failure timer %d) \n",
	    UE_id, rnti,
	    UE_list->UE_sched_ctrl[UE_id].ul_failure_timer);
      if ((UE_list->UE_sched_ctrl[UE_id].ul_failure_timer % 80) == 0) UE_list->UE_sched_ctrl[UE_id].ra_pdcch_order_sent = 0;	// resend every 8 frames
    }

    UE_list->UE_sched_ctrl[UE_id].ul_failure_timer++;
    // check threshold
    if (UE_list->UE_sched_ctrl[UE_id].ul_failure_timer > 4000) {
      // note: probably ul_failure_timer should be less than UE radio link failure time(see T310/N310/N311)
      // inform RRC of failure and clear timer
      LOG_I(MAC,
	    "UE %d rnti %x: UL Failure after repeated PDCCH orders: Triggering RRC \n",
	    UE_id, rnti);
      mac_eNB_rrc_ul_failure(module_idP, CC_id, frameP, subframeP,rnti);
      UE_list->UE_sched_ctrl[UE_id].ul_failure_timer = 0;
      UE_list->UE_sched_ctrl[UE_id].ul_out_of_sync   = 1;

      //Inform the controller about the UE deactivation. Should be moved to RRC agent in the future
      if (rrc_agent_registered[module_idP]) {
        LOG_W(MAC, "notify flexran Agent of UE state change\n");
        agent_rrc_xface[module_idP]->flexran_agent_notify_ue_state_change(module_idP,
            rnti, PROTOCOL__FLEX_UE_STATE_CHANGE_TYPE__FLUESC_DEACTIVATED);
      }
    }
  }				// ul_failure_timer>0
}

void
clear_nfapi_information(eNB_MAC_INST * eNB, int CC_idP,
			frame_t frameP, sub_frame_t subframeP)
{
  nfapi_dl_config_request_t      *DL_req = &eNB->DL_req[0];
  nfapi_ul_config_request_t      *UL_req = &eNB->UL_req[0];
  nfapi_hi_dci0_request_t   *HI_DCI0_req = &eNB->HI_DCI0_req[CC_idP][subframeP];
  nfapi_tx_request_t             *TX_req = &eNB->TX_req[0];

  eNB->pdu_index[CC_idP] = 0;

  if (nfapi_mode==0 || nfapi_mode == 1) { // monolithic or PNF

    DL_req[CC_idP].dl_config_request_body.number_pdcch_ofdm_symbols           = 1;
    DL_req[CC_idP].dl_config_request_body.number_dci                          = 0;
    DL_req[CC_idP].dl_config_request_body.number_pdu                          = 0;
    DL_req[CC_idP].dl_config_request_body.number_pdsch_rnti                   = 0;
    DL_req[CC_idP].dl_config_request_body.transmission_power_pcfich           = 6000;

    HI_DCI0_req->hi_dci0_request_body.sfnsf                                   = subframeP + (frameP<<4);
    HI_DCI0_req->hi_dci0_request_body.number_of_dci                           = 0;


    UL_req[CC_idP].ul_config_request_body.number_of_pdus                      = 0;
    UL_req[CC_idP].ul_config_request_body.rach_prach_frequency_resources      = 0; // ignored, handled by PHY for now
    UL_req[CC_idP].ul_config_request_body.srs_present                         = 0; // ignored, handled by PHY for now

    TX_req[CC_idP].tx_request_body.number_of_pdus                 = 0;

  }
}

void
copy_ulreq(module_id_t module_idP, frame_t frameP, sub_frame_t subframeP)
{
  int CC_id;
  eNB_MAC_INST *mac = RC.mac[module_idP];

  for (CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {

    nfapi_ul_config_request_t *ul_req_tmp             = &mac->UL_req_tmp[CC_id][subframeP];
    nfapi_ul_config_request_t *ul_req                 = &mac->UL_req[CC_id];
    nfapi_ul_config_request_pdu_t *ul_req_pdu         = ul_req->ul_config_request_body.ul_config_pdu_list;

    *ul_req = *ul_req_tmp;

    // Restore the pointer
    ul_req->ul_config_request_body.ul_config_pdu_list = ul_req_pdu;
    ul_req->sfn_sf                                    = (frameP<<4) + subframeP;
    ul_req_tmp->ul_config_request_body.number_of_pdus = 0;

    if (ul_req->ul_config_request_body.number_of_pdus>0)
      {
        LOG_D(PHY, "%s() active NOW (frameP:%d subframeP:%d) pdus:%d\n", __FUNCTION__, frameP, subframeP, ul_req->ul_config_request_body.number_of_pdus);
      }

    memcpy((void*)ul_req->ul_config_request_body.ul_config_pdu_list,
	   (void*)ul_req_tmp->ul_config_request_body.ul_config_pdu_list,
	   ul_req->ul_config_request_body.number_of_pdus*sizeof(nfapi_ul_config_request_pdu_t));
  }
}

void
eNB_dlsch_ulsch_scheduler(module_id_t module_idP, frame_t frameP,
			  sub_frame_t subframeP)
{

  // print the status of the eNB first
  dump_eNB_statistics(module_idP);


  int               mbsfn_status[MAX_NUM_CCs];
  protocol_ctxt_t   ctxt;

  int               CC_id, i = -1;
  UE_list_t         *UE_list = &RC.mac[module_idP]->UE_list;
  rnti_t            rnti;

  COMMON_channels_t *cc      = RC.mac[module_idP]->common_channels;

  start_meas(&RC.mac[module_idP]->eNB_scheduler);
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME
    (VCD_SIGNAL_DUMPER_FUNCTIONS_ENB_DLSCH_ULSCH_SCHEDULER,
     VCD_FUNCTION_IN);

  RC.mac[module_idP]->frame    = frameP;
  RC.mac[module_idP]->subframe = subframeP;

  for (CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {
    mbsfn_status[CC_id] = 0;

    // clear vrb_maps
    memset(cc[CC_id].vrb_map, 0, 100);
    memset(cc[CC_id].vrb_map_UL, 0, 100);


    #if (LTE_RRC_VERSION >= MAKE_VERSION(10, 0, 0))
        cc[CC_id].mcch_active        = 0;
    #endif

    clear_nfapi_information(RC.mac[module_idP], CC_id, frameP, subframeP);
  }

  // refresh UE list based on UEs dropped by PHY in previous subframe
  for (i = 0; i < MAX_MOBILES_PER_ENB; i++) {
    if (UE_list->active[i]) {
      rnti = UE_RNTI(module_idP, i);
      CC_id = UE_PCCID(module_idP, i);
      
      if (((frameP&127) == 0) && (subframeP == 0)) {
        LOG_I(MAC,
          "UE  rnti %x : %s, PHR %d dB DL CQI %d PUSCH SNR %d PUCCH SNR %d\n",
          rnti,
          UE_list->UE_sched_ctrl[i].ul_out_of_sync ==
          0 ? "in synch" : "out of sync",
          UE_list->UE_template[CC_id][i].phr_info,
          UE_list->UE_sched_ctrl[i].dl_cqi[CC_id],
          (5*UE_list->UE_sched_ctrl[i].pusch_snr[CC_id] - 640) / 10,
          (5*UE_list->UE_sched_ctrl[i].pucch1_snr[CC_id] - 640) / 10);
        }
      
      RC.eNB[module_idP][CC_id]->pusch_stats_bsr[i][(frameP * 10) +
						    subframeP] = -63;
      if (i == UE_list->head)
	VCD_SIGNAL_DUMPER_DUMP_VARIABLE_BY_NAME
	  (VCD_SIGNAL_DUMPER_VARIABLES_UE0_BSR,
	   RC.eNB[module_idP][CC_id]->
	   pusch_stats_bsr[i][(frameP * 10) + subframeP]);
      // increment this, it is cleared when we receive an sdu
      RC.mac[module_idP]->UE_list.UE_sched_ctrl[i].ul_inactivity_timer++;
      
      RC.mac[module_idP]->UE_list.UE_sched_ctrl[i].cqi_req_timer++;
      LOG_D(MAC, "UE %d/%x : ul_inactivity %d, cqi_req %d\n", i, rnti,
	    RC.mac[module_idP]->UE_list.UE_sched_ctrl[i].
	    ul_inactivity_timer,
	    RC.mac[module_idP]->UE_list.UE_sched_ctrl[i].cqi_req_timer);
      check_ul_failure(module_idP, CC_id, i, frameP, subframeP);
      
      if (RC.mac[module_idP]->UE_list.UE_sched_ctrl[i].ue_reestablishment_reject_timer > 0) {
	RC.mac[module_idP]->UE_list.UE_sched_ctrl[i].ue_reestablishment_reject_timer++;
	if(RC.mac[module_idP]->UE_list.UE_sched_ctrl[i].ue_reestablishment_reject_timer >=
	   RC.mac[module_idP]->UE_list.UE_sched_ctrl[i].ue_reestablishment_reject_timer_thres) {
	  RC.mac[module_idP]->UE_list.UE_sched_ctrl[i].ue_reestablishment_reject_timer = 0;
          //clear reestablish_rnti_map
          if(RC.mac[module_idP]->UE_list.UE_sched_ctrl[i].ue_reestablishment_reject_timer_thres >20){
	    for (int ue_id_l = 0; ue_id_l < MAX_MOBILES_PER_ENB; ue_id_l++) {
	      if (reestablish_rnti_map[ue_id_l][0] == rnti) {
	        // clear currentC-RNTI from map
	        reestablish_rnti_map[ue_id_l][0] = 0;
	        reestablish_rnti_map[ue_id_l][1] = 0;
	        break;
	      }
	    }

            PROTOCOL_CTXT_SET_BY_MODULE_ID(&ctxt, module_idP, ENB_FLAG_YES, rnti, 0, 0,module_idP);
            rrc_rlc_remove_ue(&ctxt);
            pdcp_remove_UE(&ctxt);
          }
	  // Note: This should not be done in the MAC!
	  for (int ii=0; ii<MAX_MOBILES_PER_ENB; ii++) {
	    LTE_eNB_ULSCH_t *ulsch = RC.eNB[module_idP][CC_id]->ulsch[ii];
	    if((ulsch != NULL) && (ulsch->rnti == rnti)){
              void clean_eNb_ulsch(LTE_eNB_ULSCH_t *ulsch);
	      LOG_I(MAC, "clean_eNb_ulsch UE %x \n", rnti);
	      clean_eNb_ulsch(ulsch);
	    }
	  }
	  for (int ii=0; ii<MAX_MOBILES_PER_ENB; ii++) {
	    LTE_eNB_DLSCH_t *dlsch = RC.eNB[module_idP][CC_id]->dlsch[ii][0];
	    if((dlsch != NULL) && (dlsch->rnti == rnti)){
              void clean_eNb_dlsch(LTE_eNB_DLSCH_t *dlsch);
	      LOG_I(MAC, "clean_eNb_dlsch UE %x \n", rnti);
	      clean_eNb_dlsch(dlsch);
	    }
	  }
	  
	  for(int j = 0; j < 10; j++){
	    nfapi_ul_config_request_body_t *ul_req_tmp = NULL;
	    ul_req_tmp = &RC.mac[module_idP]->UL_req_tmp[CC_id][j].ul_config_request_body;
	    if(ul_req_tmp){
	      int pdu_number = ul_req_tmp->number_of_pdus;
	      for(int pdu_index = pdu_number-1; pdu_index >= 0; pdu_index--){
          if(ul_req_tmp->ul_config_pdu_list[pdu_index].ulsch_pdu.ulsch_pdu_rel8.rnti == rnti){
            LOG_I(MAC, "remove UE %x from ul_config_pdu_list %d/%d\n", rnti, pdu_index, pdu_number);
            if(pdu_index < pdu_number -1){
              memcpy(&ul_req_tmp->ul_config_pdu_list[pdu_index], &ul_req_tmp->ul_config_pdu_list[pdu_index+1], (pdu_number-1-pdu_index) * sizeof(nfapi_ul_config_request_pdu_t));
            }
            ul_req_tmp->number_of_pdus--;
          }
	      }
	    }
	  }
	rrc_mac_remove_ue(module_idP,rnti);
	}
      }
    }
  }

#if (!defined(PRE_SCD_THREAD))
  PROTOCOL_CTXT_SET_BY_MODULE_ID(&ctxt, module_idP, ENB_FLAG_YES,
				 NOT_A_RNTI, frameP, subframeP,
				 module_idP);
  pdcp_run(&ctxt);

  rrc_rx_tx(&ctxt, CC_id);
#endif

#if (LTE_RRC_VERSION >= MAKE_VERSION(10, 0, 0))

  for (CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {
    if (cc[CC_id].MBMS_flag > 0) {
      start_meas(&RC.mac[module_idP]->schedule_mch);
      mbsfn_status[CC_id] = schedule_MBMS(module_idP, CC_id, frameP, subframeP);
      stop_meas(&RC.mac[module_idP]->schedule_mch);
    }
  }

#endif

  static int debug_flag=0;
  void (*schedule_ulsch_p)(module_id_t module_idP, frame_t frameP, sub_frame_t subframe)=NULL;
  void (*schedule_ue_spec_p)(module_id_t module_idP, frame_t frameP, sub_frame_t subframe, int *mbsfn_flag)=NULL;
  if(RC.mac[module_idP]->scheduler_mode == SCHED_MODE_DEFAULT){
    schedule_ulsch_p = schedule_ulsch;
    schedule_ue_spec_p = schedule_dlsch;
  }else if(RC.mac[module_idP]->scheduler_mode == SCHED_MODE_FAIR_RR){
    memset(dlsch_ue_select, 0, sizeof(dlsch_ue_select));
    schedule_ulsch_p = schedule_ulsch_fairRR;
    schedule_ue_spec_p = schedule_ue_spec_fairRR;
  }
  if(debug_flag==0){
    LOG_E(MAC,"SCHED_MODE=%d\n",RC.mac[module_idP]->scheduler_mode);
    debug_flag=1;
  }

  // This schedules MIB

  if ((subframeP == 0) && (frameP & 3) == 0)
      schedule_mib(module_idP, frameP, subframeP);
  if (get_softmodem_params()->phy_test == 0){
    // This schedules SI for legacy LTE and eMTC starting in subframeP
    schedule_SI(module_idP, frameP, subframeP);
    // This schedules Paging in subframeP
    schedule_PCH(module_idP,frameP,subframeP);
    // This schedules Random-Access for legacy LTE and eMTC starting in subframeP
    schedule_RA(module_idP, frameP, subframeP);
    // copy previously scheduled UL resources (ULSCH + HARQ)
    copy_ulreq(module_idP, frameP, subframeP);
    // This schedules SRS in subframeP
    schedule_SRS(module_idP, frameP, subframeP);
    // This schedules ULSCH in subframeP (dci0)
    if (schedule_ulsch_p != NULL) {
       schedule_ulsch_p(module_idP, frameP, subframeP);
    } else {
       LOG_E(MAC," %s %d: schedule_ulsch_p is NULL, function not called\n",__FILE__,__LINE__); 
    }
    // This schedules UCI_SR in subframeP
    schedule_SR(module_idP, frameP, subframeP);
    // This schedules UCI_CSI in subframeP
    schedule_CSI(module_idP, frameP, subframeP);
    // This schedules DLSCH in subframeP
    if (schedule_ue_spec_p != NULL) {
       schedule_ue_spec_p(module_idP, frameP, subframeP, mbsfn_status);
    } else {
       LOG_E(MAC," %s %d: schedule_ue_spec_p is NULL, function not called\n",__FILE__,__LINE__); 
    }

  }
  else{
    schedule_ulsch_phy_test(module_idP,frameP,subframeP);
    schedule_ue_spec_phy_test(module_idP,frameP,subframeP,mbsfn_status);
  }

  if (RC.flexran[module_idP]->enabled)
    flexran_agent_send_update_stats(module_idP);
  
  // Allocate CCEs for good after scheduling is done
  for (CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {
    if(cc[CC_id].tdd_Config == NULL || !(is_UL_sf(&cc[CC_id],subframeP)))
      allocate_CCEs(module_idP, CC_id, frameP, subframeP, 2);
  }

  if (mac_agent_registered[module_idP] && subframeP == 9) {
    flexran_agent_slice_update(module_idP);
  }

  stop_meas(&RC.mac[module_idP]->eNB_scheduler);

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME
      (VCD_SIGNAL_DUMPER_FUNCTIONS_ENB_DLSCH_ULSCH_SCHEDULER,
      VCD_FUNCTION_OUT);
}
