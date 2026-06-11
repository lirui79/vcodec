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
--  Description :  VCX Encoder System Model Interface for VCMD
--
------------------------------------------------------------------------------*/
#ifndef ENC_VCMD_H
#define ENC_VCMD_H

#include "base_type.h"
#include "enccommon.h"
#include "vcmdstruct.h"
#include "enc_core.h"

/* rename for name conflict with decoder */
#define AsicHwVcmdCoreInit EncVcmdCoreInit
#define AsicHwVcmdCoreRelease EncVcmdCoreRelease
#define AsicHwVcmdCoreGetBase EncVcmdCoreGetBase
#define AsicHwVcmdCoreWriteRegister EncVcmdCoreWriteRegister
#define AsicHwVcmdCoreReadRegister EncVcmdCoreReadRegister
#define AsicHwSubsysGetRegister EncSubsysGetRegister
#define AsicHwSubsysSetRegister EncSubsysSetRegister
#define AsicHwSubsysCoreRun EncSubsysCoreRun
#define AsicHwSubsysCoreWait EncSubsysCoreWait
/**
 * \page vcmd Video Command Engine
 *
 * VCMD is an engine to batch register operation of one vidoe core and the
 * related peripherals such as DEC400 and CACHE.
 *
 * After initialized, a thread "AsicHwVcmdCoreRun()" will run at background.
 * The intial status of the VCMD will be "IDLE". Then the background thread
 * will check the status of its registers and do corresponding operations.
 * When a command buffer is triggered, the engine will fetch commands from
 * the buffer and execute it one by one according to its state machine.
 */

/**
 * \defgroup api_vcmd Encoder VCMD API for c-model
 *
 * @{
 */
/** The callbacks of VCMD */
struct VcmdCallbacks {
  int (*vcmd_nor_isr_callback)(
      int irq, void *dev_id); /**< callback function for vcmd normal ISR*/
  int (*vcmd_abn_isr_callback)(
      int irq, void *dev_id); /**< callback function for vcmd abnormal ISR*/
  u32 (*vcmd_read_buf_callback)(
      u32 *addr, void *dev_id); /**< callback function for vcmd buffer read*/
  void (*vcmd_write_buf_callback)(
      u32 *addr, u32 value,
      void *dev_id); /**< callback function for vcmd buffer write*/
  u32 (*vcmd_read_reg_callback)(
      void *core, u32 reg_idx, u32 core_type,
      void *dev_id); /**< callback function for subsystem register read*/
  void (*vcmd_write_reg_callback)(
      void *core, u32 reg_idx, u32 value, u32 core_type,
      void *dev_id); /**< callback function for subsystem register write*/
  void (*vcmd_jmp_callback)(
      void *core, u32 core_type,
      void *dev_id); /**< callback function for one cmdbuf is processed end*/
  void (*vcmd_stall_callback)(
      void *core, u32 core_type,
      void *dev_id); /**< callback function for stall state*/
};

/** subsystem configure used by one VCMD */
struct vcmd_config {
  unsigned long vcmd_base_addr;
  u32 vcmd_iosize;
  int vcmd_irq;
  u32 sub_module_type; /**< input vce=0,IM=1,vcd=2,jpege=3, jpegd=4*/
  u16 submodule_main_addr; /**<  in byte*/
  u16 submodule_dec400_addr; /**< if submodule addr == 0xffff, this submodule does not exist. in byte*/
  u16 submodule_L2Cache_addr;  /**<  in byte*/
  u16 submodule_MMU_addr[2];   /**< in byte*/
  u16 submodule_axife_addr[2]; /**< in byte*/
  u32 priority; //the priority of vcmd
  u16 submodule_ufbc_addr; //if submodule addr == 0xffff, this submodule does not exist.// in byte
};

/** the HW configure of VCMD core */
struct HwVcmdCoreConfig {
  u32 vcmd_hw_version_id;
  struct vcmd_config *vcmd_core_config_ptr;
  int core_id;
  void *dev_id;
  u32 baseMem[ASIC_SWREG_AMOUNT];
  struct VcmdCallbacks vcmdcallback_ptr;
};

/**  the Sub module index */
enum SubModuleIndex {
  MODULE_MAIN_CORE = 0, /**< main core include vce/vcd/cutree/jpege/jpegd */
  MODULE_VCMD = 0x10,   /**< vcmd  */
  MODULE_DEC400 = 0x20, /**< dec400*/
  MODULE_CACHE = 0x30,  /**< cache */
  MODULE_MMU = 0x40,    /**< mmu   */
  MODULE_MMU2 = 0x50,   /**< mmu write */
  MODULE_AXIFE = 0x60,  /**< axife */
  MODULE_AXIFE2 = 0x70, /**< axife */
  MODULE_UFBC = 0x80, /**< UFBC */
  MODULE_OTHER = 0x90,  /**< other module */
  MODULE_MAX
};

/** Initialize one VCMD core according to the configure
 *
 * @param [in] vcmd_core_config configure of the VCMD attached subsystem;
 * @return the instance of the VCMD core;
 */
const void *AsicHwVcmdCoreInit(struct HwVcmdCoreConfig *vcmd_core_config);

/** Release the VCMD resource
 *
 * @param [in] instance the instance created when initializing;
 */
void AsicHwVcmdCoreRelease(const void *instance);

/** Get the base address of VCMD Registers for current core
 *
 * @param [in] instance the instance created when initializing;
 * @return the base address of the VCMD registers
 */
u32 *AsicHwVcmdCoreGetBase(const void *instance);

/** Update one reigster of current VCMD core
 *
 * @param [in] instance the instance created when initializing;
 * @param [in] offset the bytes offset of the vcmd register;
 * @param [in] value the value to write as an unsigned 32 bits interger;
 */
void AsicHwVcmdCoreWriteRegister(const void *instance, u32 offset, u32 value);

/** Get the value of VCMD Register of current VCMD core
 *
 * @param [in] instance the instance created when initializing;
 * @param [in] offset the bytes offset of the vcmd register;
 * @return the value of VCMD register;
 */
u32 AsicHwVcmdCoreReadRegister(const void *instance, u32 offset);

/** Function for get subsystem register value
 *
 *  @param [in] instance the instance created when initializing;
 *  @param [in] offset the address offset of core register;
 *  @return the value of core register;
 */
u32 AsicHwSubsysGetRegister(void *instance, u32 offset);

/** Function for set subsystem register value
 *
 *  @param [in] instance the instance created when initializing;
 *  @param [in] offset the address offset of core register;
 *  @param [in] value the value to write as an unsigned 32 bits interger;
 */
void AsicHwSubsysSetRegister(void *instance, u32 offset, u32 value);

/** Function for the core will run on subsystem
 *
 *  @param [in] instance the instance created when initializing;
 */
void AsicHwSubsysCoreRun(void *instance);

/** Function for wait core running thread to end
*
*   @param [in] instance the instance created when initializing;
*/
void AsicHwSubsysCoreWait(void *instance);

/**
 * @}
 */

#endif /* ENC_VCMD_H */
