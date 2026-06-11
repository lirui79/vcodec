/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Verisilicon.                                    --
--                                                                            --
--                   (C) COPYRIGHT 2019 VERISILICON                           --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Description :  VCX Encoder System Model Interface
--
------------------------------------------------------------------------------*/

/**
 * \mainpage Overview
 *
 * This document describes the Programming Interface (API) of the System Model
 * of Hantro VCCORE Encoder. It is used developed Software before HW is available.
 *
 * In VCCORE Encoder Software, the EWL layer designed to use these API which
 * will provide the example to connect with our standard API.
 *
 * the hierachy of the system organization is from core to multi-slice-nodes.
 * The system can have several slice-nodes. Each slice-node have same configure
 * for its sub-systems. One sub-system will include one core and its peripherals.
 * The core can be video encoder, jpeg encoder, or cu tree engine. The peripherals
 * can be VCMD, DEC400, MMU, or Cache.
 */

#ifndef ENC_CORE_H
#define ENC_CORE_H

/**
 * \defgroup api_core Encoder Core API for c-modele
 *
 * @{
 */

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "base_type.h"
#include "enccommon.h"
#include "encswhwregisters.h"
#include "osal.h"
#include <stdio.h>

#define MAX_HWSLICE_NUM 4
#define MAX_HWCORE_NUM 4

/** register numbers of UFBC module */
#define UFBC_SWREG_AMOUT 10

#define HasExtHw 0x1
#define NoExtHw 0x0

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

typedef struct {
  struct picture *hevc;
  struct JpegEncContainer_t *jpeg;
  struct cuTreeAsicCtl *cutree;
} WorkInst;

/**
 * Hardware configure for One Subsystem
 */
typedef struct {
  /** hw version */
  u32 asic_id;
  /** build id, identifier after V9000. */
  u32 build_id;
  u32 dec400_customer_id;
  /*sub-system modules*/
  u32 has_vcmd;   /**< if VCMD is used in the subsystem */
  u32 has_mmu;    /**< if MMU is used in the subsystem */
  u32 has_dec400; /**< if DEC400 is used in the subsystem */
  u32 has_cache;  /**< if CACHE is used in the subsystem */
  u32 has_axife;  /**< if AXIFE is used in the subsystem */
  /* features */
  u32 fuse1;   /**< swreg80 value */
  u32 fuse2;   /**< swreg214 value */
  u32 fuse3;   /**< swreg226 value */
  u32 fuse4;   /**< swreg287 value */
  u32 fuseAXI; /**< swreg319 value */
  u32 fuse5;   /**< swreg430 value */

  /* callbacks */
  u32 (*cb_get_input_rows)(void); /**< callback when low-latency is enable */

  /**
   * callback for writing and checking multi-core sync words
   * @para syncword_ba [in] the bus addresses for sync word write and read.
   *           syncword_ba[0] is bus address to write recon_word,
   *           syncword_ba[1..ref_cnt] are bus addresses to read reference
   *           syncwords.
   * @para recon_word [in] the word to write into syncword_ba[0]
   * @para ref_cnt [in] how many reference sync words will be read
   * @para ref_words [in] the array to save the required sync words value.
   *           the callback shall wait until the reference syncword is larger
   *           than these values.
   * @return 0 succuess and wait it ready;
   * @return -1 cannot wait the right sync words for long time. treat as error.
   */
  i32 (*cb_multicore_sync)(void *ctx, ptr_t *syncword_ba, u32 recon_word,
                           u32 ref_cnt, u32 *ref_words);
  void *cb_ctx;
} encHwCfg;

enum {
  CORE_STATE_IDLE = 0,
  CORE_STATE_RUN,
  CORE_STATE_ROW_0,
  CORE_STATE_ROW_MAX = CORE_STATE_ROW_0+8192/16,
  CORE_STATE_MAX
};

/**
 * Interface for One core. Internal used only.
 */
typedef struct {
  i32 slice_id; /**< the gpu-slice-node index */
  i32 core_id;  /**< the core index */
  encHwCfg cfg; /**< the core configure */

  /** encoder core emulation register table */
  u32 asicRegs[ASIC_SWREG_AMOUNT];

  /** ufbc module registers */
  u32 ufbc_regs[UFBC_SWREG_AMOUT];

  /** the lock will be used to claim the access of above resource.
   * it can be used to access the core exclusively.
   */
  pthread_mutex_t lock;
  /** flag to indicate if the core is initialized */
  i32 is_initialized;
  /** indicate which state for simulation */
  u32 state;
} SysCore;

/**
 * Interface for all cores. Internal used only.
 */
typedef struct {
  u32 slice_num;       /**< gpu-slice-node count */
  u32 cores_per_slice; /**< core or subsystem  count */
  encHwCfg Cfg[MAX_HWCORE_NUM];
} SysCoreInfo;

/**
 * \brief memory descriptor for the buffer specified in one address
 *     register.
 * the structure of the memory descriptor will benefits
 * keeping the stableness of API and the expansibility of memory
 * descriptor. We also can declare the fields more clearly inside the
 * structure.
 */
typedef struct {
  /** \brief the width of a 2D buffer in byte or size of the
     *     memory for 1D buffer. fill with 0 if the memory is not used.*/
  int width;
  /** \brief the height for 2D buffer, 1 for 1D buffer. */
  int height;
  /** \brief the stride for 2D buffer in byte, should be same as
     *  width for 1D memory.*/
  int stride;
  /** \brief indicate the buffer is for input (=1) or output (=2)
     * or both (=3) from the VPU perspective */
  u32 flag;
  u32 reg_msb_idx;
} SysBufferInfo;

/*------------------------------------------------------------------------------
    4.  Function prototypes
------------------------------------------------------------------------------*/

/** Get SLICE-Node number. Assume each slice node has same configure for
 * the subsystems.
 * @return slice-node number
 */
u32 CoreEncGetSliceNum();
/** Get Core/Subsystem number of one slice-node. All slice-node should have same
 * number of cores.
 * @return core or subsystem number in on slice-node.
 */
u32 CoreEncGetCoreNum(u32 slice_id);

/** Initialize one SysCore.
 * @param in slice_id slice-node index;
 * @param in core_id core or subsystem index;
 * @return core or subsystem number in on slice-node.
 */
SysCore *CoreEncSetup(u32 slice_id, u32 core_id);

/** Try to reserve the core.
 * @return 0 the core/subsystem is reserved for use succesfully. others, fail to
 *     reserve the core.
 */
i32 CoreEncTryReserveHw(SysCore *core);
/** Release the core reserved before.
 * @return 0 the core/subsystem is reserved before and release successfully.
 *         others, fail to reserve the core.
 */
i32 CoreEncReleaseHw(SysCore *core);
/** Enable the core to encode one frame.
 * @param [in] core the core to run
 * @param [in] offset the register offset of enable core.
 * @param [in] val the value to write the the register to enable the core;
 * @param [inout] a pointer to the handle of the instance. The initial of the
 *       handle should be NULL. and the handle will be initialized and return
 *       to software as context of this instance. If the instance is not
 *       released by CoreEncRelease(), it can be used later. Such design
 *       will reduce the initialized step to speed up the simulation.
 * @return 0 the encode task done. need to check the register value to detect
 *       the status.
 */
i32 CoreEncRun(SysCore *core, u32 offset, u32 val, WorkInst *inst);

/** Simulate some internal states. should be called after CoreEncRun().
 */
i32 CoreEncWaitHwRdy(SysCore *core);

/** release the internal resource used to run one instance.
* @param [in] core the core used to run before.
* @param [in] inst the handle to run the instance before.
* @param [in] clientType the type of the instance.
*           todo: should use thefields in the instance directly.
*/
i32 CoreEncRelease(SysCore *core, WorkInst *inst, u32 clientType);

/** release one reserved core */
void CoreEncParseAsicConfig(u32 *regs, const EWLHwConfig_t **cfg);

/** Functions for get one register (32bits) */
u32 CoreEncGetRegister(SysCore *core, u32 base);
/** Functions for set one register (32bits) */
void CoreEncSetRegister(SysCore *core, u32 base, u32 value);

/** Functions for get one field in a register with unsigned value */
u32 CoreEncGetRegisterValue(SysCore *core, regName name);
/** Functions for get one field in a register with signed value */
i32 CoreEncGetRegisterValueSigned(SysCore *core, regName name);

/** Functions for set one field in a register with unsigned value */
void CoreEncSetRegisterValue(SysCore *core, regName name, u32 value);
/** Functions for set one field in a register with signed value */
void CoreEncSetRegisterValueSigned(SysCore *core, regName name, i32 value);

//void CoreEncGetAsicConfig(i32 core_id, u32 *regs, EWLHwConfig_t *cfg);

/**
 * \brief Get the memory size parameters for the buffer mentioned in
 * the address register. When the meory is not used according to the
 * settings, it the size will be filled with 0.
 *
 * \param [in] reg_index the index of register which should be the
 *      LSB register for one memory region.
 * \param [in] stage in which stage to check the memory.for different
 *      memory size maybe inferred at different stage. 0 as before run
 *      and 1 as run done.
 * \param [out] info the buffer information, should be filled by
 *      the API.
 * \return  0 valid size is filled;
 * \return -1 invalid address register.
 */
i32 CoreEncGetMemoryDescription(SysCore *core, u32 reg_index, u32 stage,
                                SysBufferInfo *info);

#define CoreEncSetAddrRegisterValue(inst, REGBASE, addr)                   \
  do {                                                                     \
    if (sizeof(ptr_t) == 8) {                                              \
      CoreEncSetRegisterValue((inst), REGBASE, (u32)(addr));               \
      CoreEncSetRegisterValue((inst), REGBASE##_MSB, (u32)((addr) >> 32)); \
    } else {                                                               \
      CoreEncSetRegisterValue((inst), REGBASE, (u32)(addr));               \
    }                                                                      \
  } while (0)

#define CoreEncGetAddrRegisterValue(inst, REGBASE)                           \
  ((sizeof(ptr_t) == 8)                                                      \
       ? ((((ptr_t)CoreEncGetRegisterValue((inst), REGBASE)) |               \
           (((ptr_t)CoreEncGetRegisterValue((inst), REGBASE##_MSB)) << 32))) \
       : ((ptr_t)CoreEncGetRegisterValue((inst), REGBASE)))

/**
 * Set the configure for the system. Or the default setup will be used.
 * Currently, the system model don't support change configure dynamically.
 * This API must be called before other API. and each core/subsystem must
 * be intialized by CoreEncSetHwCfg() for each core.
 */
i32 CoreEncCreateHwEnv(u32 slice_num, u32 core_num);

/**
 * Set the HW configure for one core/sub-system. It must be used after
 * CoreEncCreateHwEnv() is called to configure each core.
 * Once CoreEncSetup() is called, the configure is fixed and cannot
 * be changed.
 */
i32 CoreEncSetHwCfg(u32 slice_id, u32 core_id, encHwCfg *cfg);

/**
 * Get the HW information of the system
 */
SysCoreInfo CoreEncGetHwInfo(u32 slice_num, u32 core_num);

/**
 * Release the reource used in system model
 */
void CoreEncShutdown();


/** Functions for get one register (32bits) from ufbc */
u32 CoreEncGetUfbcRegister(SysCore *core, u32 base);
/** Functions for set one register (32bits) to ufbc */
void CoreEncSetUfbcRegister(SysCore *core, u32 base, u32 value);


/**@}*/

#endif /* ENC_CORE_H */
