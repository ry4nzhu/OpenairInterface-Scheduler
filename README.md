# OpenAirInterface is under OpenAirInterface Software Alliance license.

## This repo is forked from OpenAirInterface GitLab Repository. The original project repo is: https://gitlab.eurecom.fr/oai/openairinterface5g

├── http://www.openairinterface.org/?page_id=101

├── http://www.openairinterface.org/?page_id=698

* It is distributed under OAI Public License V1.0. 
The license information is distributed under LICENSE file in the same directory.
Please see NOTICE.txt for third party software that is included in the sources.

The OpenAirInterface (OAI) software is composed of the following parts: 

```
openairinterface5g
├── ci-scripts: Meta-scripts used by the OSA CI process. Contains also configuration files used day-to-day by CI.
├── cmake_targets: Build utilities to compile (simulation, emulation and real-time platforms), and generated build files
├── common : Some common OAI utilities, other tools can be found at openair2/UTILS
├── doc : Contains an up-to-date feature set list
├── LICENSE
├── maketags : Script to generate emacs tags
├── nfapi : Contains the NFAPI code. A local Readme file provides more details.
├── openair1 : 3GPP LTE Rel-10/12 PHY layer + PHY RF simulation. A local Readme file provides more details.
├── openair2 : 3GPP LTE Rel-10 RLC/MAC/PDCP/RRC/X2AP implementation. 
    ├── COMMON
    ├── DOCS
    ├── ENB_APP
    ├── LAYER2/RLC/ with the following subdirectories: UM_v9.3.0, TM_v9.3.0, and AM_v9.3.0. 
    ├── LAYER2/PDCP/PDCP_v10.1.0.
    ├── NETWORK_DRIVER
    ├── PHY_INTERFACE
    ├── RRC/LITE
    ├── UTIL
    ├── X2AP
├── openair3: 3GPP LTE Rel10 for S1AP, NAS GTPV1-U for both ENB and UE.
    ├── COMMON
    ├── DOCS
    ├── GTPV1-U
    ├── NAS
    ├── S1AP
    ├── SCTP
    ├── SECU
    ├── UDP
    ├── UTILS
└── targets: Top-level wrappers for unitary simulation for PHY channels, system-level emulation (eNB-UE with and without S1), and realtime eNB and UE and RRH GW.
```

RELEASE NOTES:

v0.1 -> Last stable commit on develop branch before enhancement-10-harmony

v0.2 -> Merge of enhancement-10-harmony to include NGFI RRH + New Interface for RF/BBU

v0.3 -> Last stable commit on develop branch before the merge of feature-131-new-license. This is the last commit with GPL License

v0.4 -> Merge of feature-131-new-license. It closes issue#131 and changes the license to OAI Public License V1.0

v0.5 -> Merge of enhancement-10-harmony-lts. It includes fixes for Ubuntu 16.04 support

v0.5.1 -> Merge of bugfix-137-uplink-fixes. It includes stablity fixes for eNB

v0.5.2 -> Last version with old code for oaisim (abstraction mode works)

v0.6 -> RRH functionality, UE greatly improved, better TDD support,
        a lot of bugs fixed. WARNING: oaisim in PHY abstraction mode does not
        work, you need to use v0.5.2 for that.

v0.6.1 -> Mostly bugfixes. This is the last version without NFAPI.

v1.0.0 -> January 2019. This version first implements the architectural split described in doc/oai_lte_enb_func_split_arch.png picture.
            Only FAPI, nFAPI and IF4.5 interfaces are implemented.
            Repository tree structure prepares future integrations of features such as LTE-M, nbIOT or 5G-NR.
            Preliminary X2 support has been implemented.
            S1-flex has been introduced.
            New tools: config library, telnet server, ...
            A lot of bugfixes and a proper automated Continuous Integration process validates contributions.

## Scheduling Algorithm

### Number of PRBs

* `openair2/LAYER2/MAC/eNB_scheduler_primitives.c:113:int to_prb(int dl_Bandwidth)` 

* `./openair2/LAYER2/MAC/eNB_scheduler_dlsch.c:533:    eNB->eNB_stats[CC_id].available_prbs = total_nb_available_rb[CC_id];`

### Understanding of eNB scheduler

#### `openair2/LAYER2/MAC/eNB_scheduler.c`

* Scheduler mode:
  1) `eNB->scheduler_mode == SCHED_MODE_DEFAULT`, if set, function `schedule_ulsch` and `schedule_dlsch` are used for scheduling.
  2) `eNB->scheduler_mode == SCHED_MODE_FAIR_RR`

* Main function that triggers the scheduling procedure: `eNB_dlsch_ulsch_scheduler` at 572. Called by *Physical Interface* `IF_Module.c` function `UL_indication`.

* Scheduling Master Information Block(MIB) `:971`

#### Default Scheduler

* UE specific DLSCH scheduling. Retrieves next ue to be scheduled from round-robin scheduler and gets the appropriate harq_pid for the subframe from PHY. If the process is active and requires a retransmission, it schedules the retransmission with the same PRB count and MCS as the first transmission. Otherwise it consults RLC for DCCH/DTCH SDUs (status with maximum number of available PRBS), builds the MAC header (timing advance sent by default) and copies.

##### Data Structures

* `UE_list_t` at `LAYER2/MAC/mac.h:1190`: UE list used by eNB to order UEs/CC for scheduling

* downlink scheduling function `schedule_ue_spec` at `eNB_scheduler_dlsch.c: 457`: This is possiblily the place that could be modified.

* `openair2/LAYER2/MAC/eNB_scheduler_dlsch.c: 612`

* `openair2/LAYER2/MAC/eNB_scheduler_dlsch.c: 771` : Resource block

* `openair2/LAYER2/MAC/eNB_scheduler_dlsch.c: 951 TBS = get_TBS_DL(eNB_UE_stats->dlsch_mcs1,nb_available_rb);`  

* `uint32_t get_TBS_DL(uint8_t mcs, uint16_t nb_rb) at 110` in `openair1/PHY/lte_transport/lte_mcs.c`

* Transfer block table at `openair1/PHY/LTE_TRANSPORT/dlsch_tbs_full.h : 27 TBStable[TBStable_rowCnt][110]`

* uplink scheduling function `schedule_ulsch_rnti`