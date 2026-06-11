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

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "JpegTestBench.h"
#include "EncGetOption.h"
#include <stdarg.h>
#include <stddef.h>
#include "base_type.h"
#include "vsi_string.h"
#include <stdio.h>
#include "enc_log.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/
static i32 LongOption(i32 argc, char **argv, option_s *option,
                      argument_s *argument, char **optArg);
static i32 ShortOption(i32 argc, char **argv, option_s *option,
                       argument_s *argument, char **optArg);
static i32 Argument(i32 argc, char **argv, option_s *option,
                    argument_s *argument, char **optArg, u32 lenght);
static i32 GetNext(i32 argc, char **argv, argument_s *argument, char **optArg);

/*------------------------------------------------------------------------------

	EncGetOption

	Parse command line options. This function should be called with argc
	and argv values that are parameters of main(). The function will parse
	the next parameter from the command line. The function returns the
	next option character and stores the current plase to structure
	argument_s. Structure option_s contain valid options and matched option
	and argument are argument_s structure.
	For example:

	option_s option[] = {
		{"help",           'H', 0},	// No armument
		{"input",          'i', 1},	// Argument is compulsory
		{"output",         'o', 2},	// Argument is optional
		{NULL,              0,  0}	// Format of last line

	Comman line format can be
	--input filename
	--input=filename
	--inputfilename
	-i filename
	-i=filename
	-ifilename

	Input	argc	Argument count as passed to main().
		argv	Argument values as passed to main().
		option	Valid options and argument requirements.
		argument Option and argument return structure.

	Return	1	Unknow option.
		0	Option and argument are OK.
		-1	No more options.
		-2	Option match but argument is missing.

------------------------------------------------------------------------------*/
i32 EncGetOption(i32 argc, char **argv, option_s *option,
                 argument_s *argument) {
  char *optArg = NULL;
  i32 ret;

  argument->optArg = "?";
  argument->shortOpt = '?';
  argument->longOpt = "?";
  argument->enableArg = 0;

  if (GetNext(argc, argv, argument, &optArg) != 0) {
    return -1; /* End of options */
  }

  /* Long option */
  if ((ret = LongOption(argc, argv, option, argument, &optArg)) != 1) {
    return ret;
  }

  /* Short option */
  if ((ret = ShortOption(argc, argv, option, argument, &optArg)) != 1) {
    return ret;
  }

  /* This is unknow option but option anyway so optArg must return */
  argument->optArg = optArg;

  return 1;
}

/*------------------------------------------------------------------------------

	LongOption

------------------------------------------------------------------------------*/
i32 LongOption(i32 argc, char **argv, option_s *option, argument_s *argument,
               char **optArg) {
  i32 ret, i = 0;
  u32 lenght;

  if (strncmp("--", *optArg, 2) != 0) {
    return 1;
  }

  while (option[i].longOpt != NULL) {
    lenght = strlen(option[i].longOpt);
    if (strncmp(option[i].longOpt, *optArg + 2, lenght) == 0) {
      goto match;
    }
    i++;
  }
  return 1;

match:
  lenght += 2; /* Because option start -- */
  ret = Argument(argc, argv, &option[i], argument, optArg, lenght);
  if (ret != 0) {
    if (ret == 1)
      return 2;
    return -2;
  }

  return 0;
}

/*------------------------------------------------------------------------------

	ShortOption

------------------------------------------------------------------------------*/
i32 ShortOption(i32 argc, char **argv, option_s *option, argument_s *argument,
                char **optArg) {
  i32 ret, i = 0;
  char shortOpt;

  if (strncmp("-", *optArg, 1) != 0) {
    return 1;
  }

  //strncpy(&shortOpt, *optArg+1, 1);
  shortOpt = *(*optArg + 1);
  while (option[i].longOpt != NULL) {
    if (option[i].shortOpt == shortOpt) {
      goto match;
    }
    i++;
  }
  return 1;

match:
  ret = Argument(argc, argv, &option[i], argument, optArg, 2);
  if (ret != 0) {
    if (ret == 1)
      return 2;
    return -2;
  }

  return 0;
}

/*------------------------------------------------------------------------------

	Argument

------------------------------------------------------------------------------*/
i32 Argument(i32 argc, char **argv, option_s *option, argument_s *argument,
             char **optArg, u32 lenght) {
  char *arg;

  argument->shortOpt = option->shortOpt;
  argument->longOpt = option->longOpt;
  arg = *optArg + lenght;

  /* Argument and option are together */
  if (strlen(arg) != 0) {
    /* There should be no argument */
    if (option->enableArg == 0) {
      return -1;
    }

    /* Remove = */
    if (strncmp("=", arg, 1) == 0) {
      arg++;
      if (strcmp(arg, "\0") == 0) {
        if (GetNext(argc, argv, argument, optArg))
          return -1;
        if (strncmp("-", *optArg, 1) == 0) {
          argument->optCnt--;
          (*optArg)--;
          return 1;
        }
      }
    }
    argument->enableArg = 1;
    argument->optArg = arg;
    return 0;
  }

  /* Argument and option are separately */
  if (GetNext(argc, argv, argument, optArg) != 0) {
    /* There is no more parameters */
    if (option->enableArg == 1) {
      return -1;
    }
    return 0;
  }
  /*--option   =   value*/
  if (strcmp("=", *optArg) == 0) {
    if (GetNext(argc, argv, argument, optArg)) return -1;
  }
  /*--option   =value*/
  if (strncmp("=", *optArg, 1) == 0) {
    (*optArg)++;
  }

  /* Parameter is missing if next start with "-" but next time this
	 * option is OK so we must fix argument->optCnt */
  if (strncmp("-", *optArg, 1) == 0) {
    argument->optCnt--;
    if (option->enableArg == 1) {
      return -1;
    }
    return 0;
  }

  /* There should be no argument */
  if (option->enableArg == 0) {
    return -1;
  }

  argument->enableArg = 1;
  argument->optArg = *optArg;

  return 0;
}

/*------------------------------------------------------------------------------

	GetNext

------------------------------------------------------------------------------*/
i32 GetNext(i32 argc, char **argv, argument_s *argument, char **optArg) {
  /* End of options */
  if ((argument->optCnt >= argc) || (argument->optCnt < 0)) {
    return -1;
  }
  *optArg = argv[argument->optCnt];
  argument->optCnt++;

  return 0;
}

/*------------------------------------------------------------------------------
  print cml default value
------------------------------------------------------------------------------*/
i8 CmlPrint(option_s *option, commandLine_s *cmdl, char *cmdl_all_log,
             int cmdl_all_log_size) {
  u8 str_index;
  i16 i = 0;
  char cmdl_log_buffer[MAX_ALINE] = {0};
  if (!option || !cmdl || !cmdl_all_log) return NOK;
  while (option[i].longOpt){
    if (option[i].enableArg == 1){
      if (option[i].optType == I32_NUM_TYPE){
        LogStr(cmdl_all_log, cmdl_log_buffer, cmdl_all_log_size,"--%s=", option[i].longOpt);
        for (int j = 0; j < option[i].multi_num; j++){
          if (j == option[i].multi_num -1){
            LogStr(cmdl_all_log, cmdl_log_buffer, cmdl_all_log_size,"%d ",
              *(i32 *)(((i8 *)cmdl) + option[i].offset[j]));
            break;
          }
            LogStr(cmdl_all_log, cmdl_log_buffer, cmdl_all_log_size,"%d:",
              *(i32 *)(((i8 *)cmdl) + option[i].offset[j]));
        }
      }
      else if (option[i].optType == U32_NUM_TYPE){
        LogStr(cmdl_all_log, cmdl_log_buffer, cmdl_all_log_size,"--%s=", option[i].longOpt);
        for (int j = 0; j < option[i].multi_num; j++){
          if (j == option[i].multi_num -1){
            LogStr(cmdl_all_log, cmdl_log_buffer, cmdl_all_log_size,"%d ",
              *(u32 *)(((i8 *)cmdl) + option[i].offset[j]));
            break;
          }
            LogStr(cmdl_all_log, cmdl_log_buffer, cmdl_all_log_size,"%d:",
              *(u32 *)(((i8 *)cmdl) + option[i].offset[j]));
        }
      }
      else if (option[i].optType == CHAR_ARRAY_TYPE){
        LogStr(cmdl_all_log, cmdl_log_buffer, cmdl_all_log_size,"--%s=%s ",
          option[i].longOpt, (char *)(((i8 *)cmdl) + option[i].offset[0]));
      }
      else if (option[i].optType == STR_TYPE){
        LogStr(cmdl_all_log, cmdl_log_buffer, cmdl_all_log_size,"--%s=%s ",
          option[i].longOpt, *(char **)(((i8 *)cmdl) + option[i].offset[0]));
      }
    }
    i++;
  }
  LogStr(cmdl_all_log, cmdl_log_buffer, cmdl_all_log_size, "\n");
  return OK;
}

void LogStr(char *cml_all_log, char *cml_log_buffer, int cml_all_log_size,
            const char *fmt, ...) {
  va_list arg;
  va_start(arg, fmt);
  vsnprintf(cml_log_buffer, MAX_ALINE, fmt, arg);
  strncat(cml_all_log, cml_log_buffer,
          cml_all_log_size - strlen(cml_all_log) - 1);
  va_end(arg);
}

/*------------------------------------------------------------------------------
  default_parameter
------------------------------------------------------------------------------*/
void Default_Parameter(commandLine_s *cml) {
  int i;
  memset(cml, 0, sizeof(commandLine_s));
  strcpy(cml->input, "input.yuv");
  strcpy(cml->inputThumb, "thumbnail.jpg");
  strcpy(cml->com, "com.txt");
  strcpy(cml->output, "stream.jpg");
  strcpy(cml->qTablePath, "");
  cml->useVcmd = -1;
  cml->firstPic = 0;
  cml->lastPic = 0;
  cml->lumWidthSrc = DEFAULT;
  cml->lumHeightSrc = DEFAULT;
  cml->width = DEFAULT;
  cml->height = DEFAULT;
  cml->horOffsetSrc = 0;
  cml->verOffsetSrc = 0;
  cml->qLevel = 1;
  cml->quality = -1;
  cml->restartInterval = 0;
  cml->thumbnail = 0;
  cml->widthThumb = 32;
  cml->heightThumb = 32;
  cml->frameType = 0;
  cml->colorConversion = 0;
  cml->rotation = 0;
  cml->partialCoding = 0;
  cml->codingMode = 0;
  cml->markerType = 0;
  cml->unitsType = 0;
  cml->xdensity = 1;
  cml->ydensity = 1;
  cml->writeOut = 1;
  cml->comLength = 0;
  cml->inputLineBufMode = 0;
  cml->inputLineBufDepth = 1;
  cml->amountPerLoopBack = 0;
  cml->segmentUnitHeight = 16;
  cml->hashtype = 0;
  cml->mirror = 0;
  cml->formatCustomizedType = -1;
  cml->constChromaEn = 0;
  cml->constCb = 0x80;
  cml->constCr = 0x80;
  cml->predictMode = 0;
  cml->ptransValue = 0;
  cml->bitPerSecond = 0;
  cml->mjpeg = 0;
  cml->frameRateNum = 30;
  cml->frameRateDenom = 1;
  cml->rcMode = 1;
  cml->picQpDeltaMin = -2;
  cml->picQpDeltaMax = 3;
  cml->qpmin = 0;
  cml->qpmax = 51;
  cml->fixedQP = -1;
  cml->exp_of_input_alignment = 4;
  cml->streamBufChain = 0;
  cml->streamMultiSegmentMode = 0;
  cml->streamMultiSegmentSize = 1024;
  cml->streamMultiSegmentAmount = 4;
  strcpy(cml->dec400CompTableinput, "dec400CompTableinput.bin");
  cml->AXIAlignment = 0;
  cml->irqTypeMask = 0x1f4;
  cml->secure_mode = 0;
  cml->inputSliceInfoEn = 0;
  cml->encDevice= "/tmp/dev/vsi_vcx";
  cml->memDevice = "/tmp/dev/memalloc";
#ifdef USE_LIBVA
  cml->vaDriverName = "hantro";
#endif

  cml->scanType = 0; //0:raster scan,1:superTileX scan

  /* ROI Map */
  cml->roimapFile = NULL;
  cml->nonRoiLevel = 5;
  cml->nonRoiFilter = NULL;

  /*Overlay*/
  cml->overlayEnables = 0;

  for (i = 0; i < MAX_OVERLAY_NUM; i++) {
    strcpy(cml->olInput[i], "olInput.yuv");
    strcpy(cml->osdDec400CompTableInput[i], "osdDec400CompTableinput.bin");
    cml->olFormat[i] = 0;
    cml->olAlpha[i] = 0;
    cml->olWidth[i] = 0;
    cml->olHeight[i] = 0;
    cml->olXoffset[i] = 0;
    cml->olYoffset[i] = 0;
    cml->olYStride[i] = 0;
    cml->olUVStride[i] = 0;
    cml->olSuperTile[i] = 0;
    cml->olScaleWidth[i] = 0;
    cml->olScaleHeight[i] = 0;
  }

  /* OSD_MAP */
  cml->osdMapEnable = 0;
  strcpy(cml->osdMapInput, "osdMapInput.yuv");
  cml->osdMapStride = DEFAULT;
  cml->osdMapBlockSize = 8;
  for (i = 0; i < MAX_OSDMAP_COLOR_NUM; i++) {
    cml->osdMapAlpha[i] = 0;
    cml->osdMapY[i] = 0;
    cml->osdMapU[i] = 0;
    cml->osdMapV[i] = 0;
  }

  cml->sramPowerdownDisable = 0;
  cml->sramPowerdownMode = 0;
  cml->sramPowerdownTimerDiv32 = 96;//8'h60
  /* logmsg env default settting */
  VCEncLogSetting env_log = {LOG_STDOUT, VCENC_LOG_WARN, 0x003F, 0x0001}; //enable API/REGS/EWL/MEM/RC/CML enable RECON
  VCEncLogGetEnvSetting(&env_log);
  cml->logOutDir = env_log.out_dir;
  cml->logOutLevel = env_log.out_level;
  cml->logTraceMap = env_log.k_trace_map;

  cml->ufbcMode = 0;
  cml->ufbcYuvTrans = 0;
  cml->ufbcBlockType = 0;
  cml->ufbcBlockSplit = 0;
  cml->priority = 0;
  cml->core_mask = 0;
  cml->lowlatGatingDisable = 1;
  cml->mosSizeIndex = 1;
}
