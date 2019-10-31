# OpenairInterface eNB Scheduler
### The repo is adapted from OpenairInterface5g, and the main focus is on base station scheduling algorithm
OpenAirInterface is under OpenAirInterface Software Alliance license.

├── http://www.openairinterface.org/?page_id=101

├── http://www.openairinterface.org/?page_id=698

### Understanding of eNB scheduler

#### `openair2/LAYER2/MAC/eNB_scheduler.c`

- Main function that triggers the scheduling procedure: `eNB_dlsch_ulsch_scheduler` at 572. Called by *Physical Interface* `IF_Module.c` function `UL_indication`.

- Scheduler mode: 
  1) `eNB->scheduler_mode == SCHED_MODE_DEFAULT` 
  2) `eNB->scheduler_mode == SCHED_MODE_FAIR_RR`
 
 - Scheduling Master Information Block(MIB) `:971`


