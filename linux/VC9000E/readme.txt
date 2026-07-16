VC9000E_CtrlSW_20240923

cd  rootdir/VC9000E/software/linux_reference/kernel_module
   mv linux  linux_bak
   ln -s ../../../driver/vcodec  linux

cd  rootdir/VC9000E/software

compile
   make      select

example
     example compile app
        make clean;make hevc ENV=pci TRACE=n

cd  rootdir/VC9000E/software/bin/pci/
  ./hevc_testenc --encDevice=/dev/hantroenc --memDevice=/dev/memalloc -a0 -b299 --rdoLevel=1 --refRingBufEnable=0 -i/home/stone/workspace/YUV/akiyo_352x288_300.yuv --inputFormat=0 --gopSize=1 --enableRdoQuant=0 --lumWidthSrc=352 --lumHeightSrc=288 --width=352 --height=288 --codecFormat=hevc --inputAlignmentExp=0 --aqInfoAlignmentExp=6 --bitDepthLuma=8 --bitDepthChroma=8 --refAlignmentExp=0 --refChromaAlignmentExp=6 -o hevc_case_id_118_cmodel_cmds.hevc
