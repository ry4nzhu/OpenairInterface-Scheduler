#ifndef __OPENAIR2_STATS_H__
#define __OPENAIR2_STATS_H__

#include <inttypes.h>

#include "LAYER2/RLC/rlc.h"
#include "LAYER2/MAC/mac.h"
#include "LAYER2/MAC/mac_extern.h"
#include "LAYER2/PDCP_v10.1.0/pdcp.h"
#include "common/utils/LOG/log.h"
#include "common/ran_context.h"


void dump_eNB_statistics(module_id_t module_idP);

#endif