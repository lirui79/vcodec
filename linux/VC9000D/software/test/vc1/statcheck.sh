#-------------------------------------------------------------------------------
#-                                                                            --
#-       This software is confidential and proprietary and may be used        --
#-        only as expressly authorized by a licensing agreement from          --
#-                                                                            --
#-                            VeriSilicon Inc.                                --
#-                                                                            --
#-                   (C) COPYRIGHT 2015 VeriSilicon Inc                       --
#-                            ALL RIGHTS RESERVED                             --
#-                                                                            --
#-                 The entire notice above must be reproduced                 --
#-                  on all copies and should not be removed.                  --
#-                                                                            --
#-------------------------------------------------------------------------------

#!/bin/bash
#create tag
cvs stat .          | grep atus

cvs stat ../scripts/          | grep atus
cvs stat ../common/          | grep atus

cvs stat ../../linux/dwl/          | grep atus
cvs stat ../../linux/memalloc/          | grep atus
cvs stat ../../linux/ldriver/kernel_26x/          | grep atus

cvs stat ../../linux/vc1/          | grep atus

cvs stat ../../source/config/      | grep atus

cvs stat ../../source/vc1/          | grep atus

cvs stat ../../source/inc/vc1decapi.h ../../source/inc/basetype.h ../../source/inc/dwl.h          | grep atus


cvs stat -v Makefile

