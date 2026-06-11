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
#!/bin/sh

# Load memalloc

module="memalloc"
device="/tmp/dev/memalloc"
mode="666"
module=${1:-"memalloc"}
ddr_offset=${2:-0x0}
device="/tmp/dev/$module"

# Set PCIE_EN=n when using dma
# alloc_base=addr_transl+0x80_0000(+0x90_0000, when using vcmd)
# addr_transl= 0x4000_5000(platform: 155, 165, 201, 195) or 0x1000_0000(platform: 222, 226, 85, 130, 102)
# alloc_size= 892 or 752(only in platform 165) and when using vcmd need substract 9
alloc_size=${3:-883}
alloc_base=${4:-0x41105000}
addr_transl=${5:-0x40005000}
echo

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

sudo insmod $module.ko mem_dev_n="$module" ddr_offset=$ddr_offset vcmd_size=0x900000 ddr_size=768 \
       mem_alloc_table_size=0x1000000 alloc_size=$alloc_size alloc_base=$alloc_base \
       addr_transl=$addr_transl || exit 1

echo "$*"
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
