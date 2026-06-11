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
mode="666"
module=${1:-"hantrodec"}
device="/tmp/dev/$module"
offset=${2:-0x0}
#set dma_used as 1 when using dma and set the host base
dma_used=${3:-0}
host_base=${4:-0x40005000}
vcmd_isr_polling=${5:-1}
vsi_kloglvl=${6:-5}
arb_urgent=${7:-0}
arb_weight=${8:-0x1d}
arb_timewindow=${9:-0x1d}
arb_bw_overflow=${10:-0}
timeout_time=${11:-0}
use_vcmd=${12:-1}
echo "$module"

if [ ! -e /tmp/dev ]
then
    mkdir -p /tmp/dev/
fi

#insert module
rm_module=`lsmod |grep -w $module`
if [ ! -z "$rm_module" ]
then
   sudo rmmod $module || exit 1
fi
sudo insmod $module.ko dec_dev_n="$module" ddr_offset=$offset dma_used=$dma_used  \
                  host_base=$host_base vcmd_isr_polling=$vcmd_isr_polling  \
                  vsi_kloglvl=$vsi_kloglvl arbiter_weight=$arb_weight \
                  arbiter_urgent=$arb_urgent arbiter_timewindow=$arb_timewindow \
                  arbiter_bw_overflow=$arb_bw_overflow sw_timeout_time=$timeout_time \
                  use_vcmd=$use_vcmd|| exit 1

echo "module $module inserted"

#remove old nod
rm -f $device

#read the major asigned at loading time
major=`cat /proc/devices | grep -w $module | cut -c1-3`

echo "$module major = $major"

#create dev node
sudo mknod $device c $major 0

echo "node $device created"

#give all 'rw' access
sudo chmod $mode $device

echo "set node access to $mode"

#the end
echo
