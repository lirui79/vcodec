/*------------------------------------------------------------------------------
--       Copyright (c) 2015, VeriSilicon Inc. All rights reserved             --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
--         Copyright (c) 2007-2010, Hantro OY. All rights reserved.           --
--                                                                            --
-- This software is confidential and proprietary and may be used only as      --
--   expressly authorized by VeriSilicon in a written licensing agreement.    --
--                                                                            --
--         This entire notice must be reproduced on all copies                --
--                       and may not be removed.                              --
--                                                                            --
--------------------------------------------------------------------------------
-- Redistribution and use in source and binary forms, with or without         --
-- modification, are permitted provided that the following conditions are met:--
--   * Redistributions of source code must retain the above copyright notice, --
--       this list of conditions and the following disclaimer.                --
--   * Redistributions in binary form must reproduce the above copyright      --
--       notice, this list of conditions and the following disclaimer in the  --
--       documentation and/or other materials provided with the distribution. --
--   * Neither the names of Google nor the names of its contributors may be   --
--       used to endorse or promote products derived from this software       --
--       without specific prior written permission.                           --
--------------------------------------------------------------------------------
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"--
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE  --
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE --
-- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE  --
-- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR        --
-- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF       --
-- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS   --
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN    --
-- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)    --
-- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE --
-- POSSIBILITY OF SUCH DAMAGE.                                                --
--------------------------------------------------------------------------------
------------------------------------------------------------------------------*/

#include "basetype.h"
#include "vp6_pp_pipeline.h"
#include "vp6hwd_container.h"
#include "vp6hwd_debug.h"
#include "dec_log.h"
/*------------------------------------------------------------------------------
    Function name   : vp6RegisterPP
    Description     :
    Return type     : i32
    Argument        : const void * dec_inst
    Argument        : const void  *pp_inst
    Argument        : (*PPRun)(const void *)
    Argument        : void (*PPEndCallback)(const void *)
------------------------------------------------------------------------------*/
i32 vp6RegisterPP(const void *dec_inst, const void *pp_inst,
                  void (*PPDecStart) (const void *, const DecPpInterface *),
                  void (*PPDecWaitEnd) (const void *),
                  void (*PPConfigQuery) (const void *, DecPpQuery *)) {
  VP6DecContainer_t  *dec_cont;

  dec_cont = (VP6DecContainer_t *) dec_inst;

  if(dec_inst == NULL || dec_cont->pp.pp_instance != NULL ||
      pp_inst == NULL || PPDecStart == NULL || PPDecWaitEnd == NULL
      || PPConfigQuery == NULL) {
    APITRACEERR("vp6RegisterPP: Invalid parameter\n");
    return -1;
  }

  if(dec_cont->asic_running) {
    APITRACEERR("vp6RegisterPP: Illegal action, asic_running\n");
    return -2;
  }

  dec_cont->pp.pp_instance = pp_inst;
  dec_cont->pp.PPConfigQuery = PPConfigQuery;
  dec_cont->pp.PPDecStart = PPDecStart;
  dec_cont->pp.PPDecWaitEnd = PPDecWaitEnd;

  dec_cont->pp.dec_pp_if.pp_status = DECPP_IDLE;

  APITRACEDEBUG("vp6RegisterPP: Connected to PP instance 0x%08x\n", (size_t)pp_inst);

  return 0;
}

/*------------------------------------------------------------------------------
    Function name   : vp6UnregisterPP
    Description     :
    Return type     : i32
    Argument        : const void * dec_inst
    Argument        : const void  *pp_inst
------------------------------------------------------------------------------*/
i32 vp6UnregisterPP(const void *dec_inst, const void *pp_inst) {
  VP6DecContainer_t *dec_cont;

  if (dec_inst == NULL)
  	return -1;

  dec_cont = (VP6DecContainer_t *) dec_inst;

  ASSERT(pp_inst == dec_cont->pp.pp_instance);

  if(pp_inst != dec_cont->pp.pp_instance) {
    APITRACEERR("vp6UnregisterPP: Invalid parameter\n");
    return -1;
  }

  if(dec_cont->asic_running) {
    APITRACEERR("vp6UnregisterPP: Illegal action, asic_running\n");
    return -2;
  }

  dec_cont->pp.pp_instance = NULL;
  dec_cont->pp.PPConfigQuery = NULL;
  dec_cont->pp.PPDecStart = NULL;
  dec_cont->pp.PPDecWaitEnd = NULL;

  APITRACEDEBUG("vp6UnregisterPP: Disconnected from PP instance 0x%08x\n",
                (size_t)pp_inst);

  return 0;
}

/*------------------------------------------------------------------------------
    Function name   : vp6PreparePpRun
    Description     :
    Return type     : void
    Argument        :
------------------------------------------------------------------------------*/
void vp6PreparePpRun(VP6DecContainer_t * dec_cont) {
  DecPpInterface *dec_pp_if = &dec_cont->pp.dec_pp_if;

  if(dec_cont->pp.pp_instance != NULL) { /* we have PP connected */
    dec_cont->pp.pp_info.tiled_mode =
      dec_cont->tiled_reference_enable;
    dec_cont->pp.PPConfigQuery(dec_cont->pp.pp_instance,
                               &dec_cont->pp.pp_info);

    APITRACEDEBUG
    ("vp6PreparePpRun: output picture => PP could run!\n");

    dec_pp_if->use_pipeline = dec_cont->pp.pp_info.pipeline_accepted & 1;

    if(dec_pp_if->use_pipeline) {
      APITRACEDEBUG
      ("vp6PreparePpRun: pipeline=ON => PP will be running\n");
      dec_pp_if->pp_status = DECPP_RUNNING;
    }
    /* parallel processing if previous output pic exists */
    else if (dec_cont->out_count) {
      APITRACEDEBUG
      ("vp6PreparePpRun: pipeline=OFF => PP has to run after DEC\n");
      dec_pp_if->pp_status = DECPP_RUNNING;
    }
  }
}
