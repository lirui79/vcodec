/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            VeriSilicon Inc.                                --
--                                                                            --
--                   (C) COPYRIGHT 2015 VeriSilicon Inc                       --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Abstract : Command line parameter parsing
--
------------------------------------------------------------------------------*/

#ifndef ENC_GET_OPTION
#define ENC_GET_OPTION

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "base_type.h"
#define LOGBUFFER 1024 * 64
#define MAX_ALINE 1024
#define OFFSETOF(cml) (size_t)&((commandLine_s*)0)->cml
#define MAX_NUM 30

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/
typedef enum {
  NULL_TYPE = 0,
  I32_NUM_TYPE,
  U32_NUM_TYPE,
  CHAR_ARRAY_TYPE,
  STR_TYPE,
} cmdl_type;

typedef struct {
  char *longOpt;
  char shortOpt;
  i32 enableArg;
  cmdl_type optType;
  i32 multi_num;
  i32 offset[MAX_NUM];
} option_s;

typedef struct {
  i32 optCnt;
  char *optArg;
  char shortOpt;
  char *longOpt;
  i32 enableArg;
} argument_s;

/*------------------------------------------------------------------------------
    4. Function prototypes
------------------------------------------------------------------------------*/
i32 EncGetOption(i32 argc, char **argv, option_s *, argument_s *);
i8 CmlPrint(option_s *option, commandLine_s *cmdl, char *cmdl_all_log,
            int cmdl_all_log_size);
void LogStr(char *cmdl_all_log, char *cmdl_log_buffer,
            int cmdl_all_log_size, const char *fmt, ...);
void Default_Parameter(commandLine_s *cml);

#endif
