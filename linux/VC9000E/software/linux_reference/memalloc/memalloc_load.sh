#!/bin/bash
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
#--   Abstract     : Load memalloc                                            --
#--                                                                           --
#-------------------------------------------------------------------------------

mode="666"
module=${1:-"memalloc"}
ddr_offset=${2:-0x0}
alloc_size=${3:-750}
alloc_base=${4:-0x41105000}
addr_transl=${5:-0x40005000}
device="/tmp/dev/$module"

echo

if [ ! -e /tmp/dev ]
then
    mkdir -p /tmp/dev/
fi

#insert module
rm_module=`lsmod |grep -w $module`
if [ ! -z "$rm_module" ]
then
   rmmod $module || exit 1
fi

insmod $module.ko mem_dev=$module ddr_offset=$ddr_offset alloc_size=$alloc_size alloc_base=$alloc_base \
       addr_transl=$addr_transl vcmd_size=0x900000 ddr_size=768 || exit 1


echo "module $module inserted"

#remove old nod
rm -f $device

#read the major asigned at loading time
major=`cat /proc/devices | grep -w $module | cut -c1-3`

echo "$module major = $major"

#create dev node
mknod $device c $major 0

echo "node $device created"

#give all 'rw' access
chmod $mode $device

echo "set node access to $mode"

#the end
echo
