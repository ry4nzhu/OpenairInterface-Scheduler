# OpenairInterface eNB Scheduler

## The repo is adapted from OpenAirInterface5g gitLab, and the main focus of this repo is on base station scheduling algorithm

OpenAirInterface is under OpenAirInterface Software Alliance license.

├── <http://www.openairinterface.org/?page_id=101>

├── <http://www.openairinterface.org/?page_id=698>

### Understanding of eNB scheduler

#### `openair2/LAYER2/MAC/eNB_scheduler.c`

- Scheduler mode:
  1) `eNB->scheduler_mode == SCHED_MODE_DEFAULT`, if set, function `schedule_ulsch` and `schedule_dlsch` are used for scheduling.
  2) `eNB->scheduler_mode == SCHED_MODE_FAIR_RR`

- Main function that triggers the scheduling procedure: `eNB_dlsch_ulsch_scheduler` at 572. Called by *Physical Interface* `IF_Module.c` function `UL_indication`.

- Scheduling Master Information Block(MIB) `:971`

#### Default Scheduler

- UE specific DLSCH scheduling. Retrieves next ue to be scheduled from round-robin scheduler and gets the appropriate harq_pid for the subframe from PHY. If the process is active and requires a retransmission, it schedules the retransmission with the same PRB count and MCS as the first transmission. Otherwise it consults RLC for DCCH/DTCH SDUs (status with maximum number of available PRBS), builds the MAC header (timing advance sent by default) and copies. 

##### Data Structures

- `UE_list_t` at `LAYER2/MAC/mac.h:1190`: UE list used by eNB to order UEs/CC for scheduling

- downlink scheduling function `schedule_ue_spec` at `eNB_scheduler_dlsch.c: 457`: This is possiblily the place that could be modified.

- `openair2/LAYER2/MAC/eNB_scheduler_dlsch.c: 612`

- `openair2/LAYER2/MAC/eNB_scheduler_dlsch.c: 771` : resource block

- `openair2/LAYER2/MAC/eNB_scheduler_dlsch.c: 951 TBS = get_TBS_DL(eNB_UE_stats->dlsch_mcs1,nb_available_rb);`  

- uplink scheduling function `schedule_ulsch_rnti`

- `uint32_t get_TBS_DL(uint8_t mcs, uint16_t nb_rb) at 110` in `openair1/PHY/lte_transport/lte_mcs.c`

- Transfer block table at `openair1/PHY/LTE_TRANSPORT/dlsch_tbs_full.h : 27 TBStable[TBStable_rowCnt][110]` 
