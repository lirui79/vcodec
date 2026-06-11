/*
 * SPDX_license-Identifier: GPL-2.0  OR BSD-3-Clause
 * Copyright (c) 2015, Verisilicon Inc. - All Rights Reserved
 *
 ********************************************************************************
 *
 * GPL-2.0
 *
 ********************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
 * Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 ********************************************************************************
 *
 * Alternatively, This software may be distributed under the terms of
 * BSD-3-Clause, in which case the following provisions apply instead of the ones
 * mentioned above :
 *
 ********************************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ********************************************************************************
 */

#include "basetype.h"
#include <string.h>
#include <stdio.h>
#include "arbiter_get_option.h"

i32 GetNext(i32 argc, char **argv, argument_s *argument, char **optArg) {
  /* End of options */
  if ((argument->optCnt >= argc) || (argument->optCnt < 0)) {
    return -1;
  }
  *optArg = argv[argument->optCnt];
  argument->optCnt++;

  return 0;
}

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

i32 LongOption(i32 argc, char **argv, option_s *option, argument_s *argument,
               char **optArg) {
  i32 i = 0;
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
  if (Argument(argc, argv, &option[i], argument, optArg, lenght) != 0) {
    return -2;
  }

  return 0;
}

i32 ShortOption(i32 argc, char **argv, option_s *option, argument_s *argument,
                char **optArg) {
  i32 i = 0;
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
  if (Argument(argc, argv, &option[i], argument, optArg, 2) != 0) {
    return -2;
  }

  return 0;
}

i32 ParseOption(i32 argc, char **argv, option_s *option,
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