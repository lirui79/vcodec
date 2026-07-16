VC9000D_CtrlSW_V2.4.85

cd rootdir/VC9000D/software/linux
  mv subsys_driver  subsys_driver_bak
  ln -s ../../../driver/vcodec  subsys_driver


cd rootdir/VC9000D

compile
    make      select

example
    make clean g2dec ENV=x86_linux_pci
    make clean g2dec ENV=x86_linux USE_MODEL_SIMULATION=n USE_VCMD=y

cd rootdir/VC9000D/out/x86_linux/debug

    ./g2dec --dec-dev=/dev/hantrodec --mem-dev=/dev/memalloc --logoutlevel=3 --logtracemap=CFG --input-format=h264 -Ob1.yuv /home/stone/workspace/akiyo_352x288_300_IBBBP.h264
    ./g2dec --dec-dev=/dev/hantrodec --mem-dev=/dev/memalloc --logoutlevel=3 --logtracemap=CFG --input-format=bs -Ob1.yuv /home/stone/workspace/sample_640x360.hevc