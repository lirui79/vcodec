# VCodec



## Getting started

To make it easy for you to get started with GitLab, here's a list of recommended next steps.

Already a pro? Just edit this README.md and make it your own. Want to make it easy? [Use the template at the bottom](#editing-this-readme)!

## Add your files

- [ ] [Create](https://docs.gitlab.com/ee/user/project/repository/web_editor.html#create-a-file) or [upload](https://docs.gitlab.com/ee/user/project/repository/web_editor.html#upload-a-file) files
- [ ] [Add files using the command line](https://docs.gitlab.com/topics/git/add_files/#add-files-to-a-git-repository) or push an existing Git repository with the following command:

```
cd existing_repo
git remote add origin http://172.16.0.99:8080/stone_li/vcodec.git
git branch -M main
git push -uf origin main
```

## Integrate with your tools

- [ ] [Set up project integrations](http://172.16.0.99:8080/stone_li/vcodec/-/settings/integrations)

## Collaborate with your team

- [ ] [Invite team members and collaborators](https://docs.gitlab.com/ee/user/project/members/)
- [ ] [Create a new merge request](https://docs.gitlab.com/ee/user/project/merge_requests/creating_merge_requests.html)
- [ ] [Automatically close issues from merge requests](https://docs.gitlab.com/ee/user/project/issues/managing_issues.html#closing-issues-automatically)
- [ ] [Enable merge request approvals](https://docs.gitlab.com/ee/user/project/merge_requests/approvals/)
- [ ] [Set auto-merge](https://docs.gitlab.com/user/project/merge_requests/auto_merge/)

## Test and Deploy

Use the built-in continuous integration in GitLab.

- [ ] [Get started with GitLab CI/CD](https://docs.gitlab.com/ee/ci/quick_start/)
- [ ] [Analyze your code for known vulnerabilities with Static Application Security Testing (SAST)](https://docs.gitlab.com/ee/user/application_security/sast/)
- [ ] [Deploy to Kubernetes, Amazon EC2, or Amazon ECS using Auto Deploy](https://docs.gitlab.com/ee/topics/autodevops/requirements.html)
- [ ] [Use pull-based deployments for improved Kubernetes management](https://docs.gitlab.com/ee/user/clusters/agent/)
- [ ] [Set up protected environments](https://docs.gitlab.com/ee/ci/environments/protected_environments.html)

***

# Editing this README

When you're ready to make this README your own, just edit this file and use the handy template below (or feel free to structure it however you want - this is just a starting point!). Thanks to [makeareadme.com](https://www.makeareadme.com/) for this template.

## Suggestions for a good README

Every project is different, so consider which of these sections apply to yours. The sections used in the template are suggestions for most open source projects. Also keep in mind that while a README can be too long and detailed, too long is better than too short. If you think your README is too long, consider utilizing another form of documentation rather than cutting out information.

## Name
Choose a self-explaining name for your project.

## Description
Let people know what your project can do specifically. Provide context and add a link to any reference visitors might be unfamiliar with. A list of Features or a Background subsection can also be added here. If there are alternatives to your project, this is a good place to list differentiating factors.

## Badges
On some READMEs, you may see small images that convey metadata, such as whether or not all the tests are passing for the project. You can use Shields to add some to your README. Many services also have instructions for adding a badge.

## Visuals
Depending on what you are making, it can be a good idea to include screenshots or even a video (you'll frequently see GIFs rather than actual videos). Tools like ttygif can help, but check out Asciinema for a more sophisticated method.

## Installation
Within a particular ecosystem, there may be a common way of installing things, such as using Yarn, NuGet, or Homebrew. However, consider the possibility that whoever is reading your README is a novice and would like more guidance. Listing specific steps helps remove ambiguity and gets people to using your project as quickly as possible. If it only runs in a specific context like a particular programming language version or operating system or has dependencies that have to be installed manually, also add a Requirements subsection.

## Usage
Use examples liberally, and show the expected output if you can. It's helpful to have inline the smallest example of usage that you can demonstrate, while providing links to more sophisticated examples if they are too long to reasonably include in the README.

## Support
Tell people where they can go to for help. It can be any combination of an issue tracker, a chat room, an email address, etc.

## Roadmap
If you have ideas for releases in the future, it is a good idea to list them in the README.

## Contributing
State if you are open to contributions and what your requirements are for accepting them.

For people who want to make changes to your project, it's helpful to have some documentation on how to get started. Perhaps there is a script that they should run or some environment variables that they need to set. Make these steps explicit. These instructions could also be useful to your future self.

You can also document commands to lint the code or run tests. These steps help to ensure high code quality and reduce the likelihood that the changes inadvertently break something. Having instructions for running tests is especially helpful if it requires external setup, such as starting a Selenium server for testing in a browser.

## Authors and acknowledgment
Show your appreciation to those who have contributed to the project.

## License
For open source projects, say how it is licensed.

## Project status
If you have run out of energy or time for your project, put a note at the top of the README saying that development has slowed down or stopped completely. Someone may choose to fork your project or volunteer to step in as a maintainer or owner, allowing your project to keep going. You can also make an explicit request for maintainers.


VC9000D    driver\vcodec ->  VC9000D 中    software\linux\subsys_driver     replace  compile  编译

VC9000E    driver\vcodec ->  VC9000E 中    software\linux_reference\kernel_module\linux    replace  compile

1.Compile 编译
  1）切换 至 VC9000D/software/linux 目录中，执行
    >  mv subsys_driver  subsys_driver_bak
    >  ln -s ../../../driver/vcodec  subsys_driver

  2）切换 至 VC9000E/software/linux_reference/kernel_module 目录中，执行
    >  mv linux  linux_bak
    >  ln -s ../../../driver/vcodec  linux

  3) 切换至目录  driver/memalloc 目录中，执行
    >  make clean;make
    >  sudo insmod memalloc.ko alloc_size=256 alloc_base=0xaf000000
       alloc_size 为分配内存的大小，单位为M；alloc_base为分配内存的起始地址，需要根据硬件平台和系统配置来设置，通常使用内存管理单元（MMU）未使用区域的基地址。
       例如 ubuntu 26.04 desktop、Ubuntu 26.04 server、ubuntu 22.04 desktop 系统中，可调整 Crash kernel 内存大小，内存调整至768M，设置最后256M用于测试， 可用 cat /proc/iomem
           >   cat /proc/iomem
            00000000-00000fff : Reserved
            00001000-0009e7ff : System RAM
            0009e800-0009ffff : Reserved
            000a0000-000bffff : PCI Bus 0000:00
            000c0000-000c7fff : Video ROM
            000ca000-000cafff : Adapter ROM
            000cb000-000ccfff : Adapter ROM
            000d0000-000dbfff : PCI Bus 0000:00
            000dc000-000fffff : Reserved
            000f0000-000fffff : System ROM
            00100000-bfecffff : System RAM
            51400000-529fffff : Kernel code
            52a00000-537aafff : Kernel rodata
            53800000-53c54ebf : Kernel data
            54151000-545fffff : Kernel bss
            8f000000-beffffff : Crash kernel
            bfed0000-bfefefff : ACPI Tables
            bfeff000-bfefffff : ACPI Non-volatile Storage
            bff00000-bfffffff : System RAM
            c0000000-febfffff : PCI Bus 0000:00
            c0000000-c0003fff : 0000:00:10.0
            e5b00000-e5bfffff : PCI Bus 0000:22
            e5c00000-e5cfffff : PCI Bus 0000:1a
            e5d00000-e5dfffff : PCI Bus 0000:12
            e5e00000-e5efffff : PCI Bus 0000:0a
            e5f00000-e5ffffff : PCI Bus 0000:21
            e6000000-e60fffff : PCI Bus 0000:19
            e6100000-e61fffff : PCI Bus 0000:11
            e6200000-e62fffff : PCI Bus 0000:09
            e6300000-e63fffff : PCI Bus 0000:20
            e6400000-e64fffff : PCI Bus 0000:18
            e6500000-e65fffff : PCI Bus 0000:10
            e6600000-e66fffff : PCI Bus 0000:08
            e6700000-e67fffff : PCI Bus 0000:1f

    >  sudo insmod memalloc.ko alloc_size=256 alloc_base=0xaf000000

   4) 切换至目录  driver/vcodec 目录中，执行
    >  make clean;make
    >  sudo insmod vcodec.ko
    >  sudo insmod vcodec.ko vsi_kloglvl=2

   5）可查看内核日志
    >  sudo dmesg

   6) 切换至目录  VC9000D/ 目录中，执行
    >  cat readme.txt
        VC9000D_CtrlSW_V2.4.85

        cd rootdir/VC9000D/software/linux
        mv subsys_driver  subsys_driver_bak
        ln -s ../../../driver/vcodec  subsys_driver

        cd rootdir

        make

        example
            make clean g2dec ENV=x86_linux_pci
            make clean g2dec ENV=x86_linux USE_MODEL_SIMULATION=n USE_VCMD=y
    >  cd rootdir/VC9000D
    >  make clean g2dec ENV=x86_linux USE_MODEL_SIMULATION=n USE_VCMD=y

   7) 切换至目录  VC9000E/ 目录中，执行
    >  cat readme.txt
        VC9000E_CtrlSW_20240923

        cd  rootdir/VC9000E/software/linux_reference/kernel_module
        mv linux  linux_bak
        ln -s ../../../driver/vcodec  linux

        compile
            cd  rootdir/VC9000E/software
            run  make      select
            example compile app
                make clean;make hevc ENV=pci TRACE=n
    >   cd  rootdir/VC9000E/software
    >   make clean;make hevc ENV=pci TRACE=n

2. Run/Test 运行测试
   1）确认测试文件已上传
   2）切换至目录  VC9000D/out/x86_linux/debug 目录中，执行
    >  ./g2dec --dec-dev=/dev/hantrodec --mem-dev=/dev/memalloc --logoutlevel=3 --logtracemap=CFG --input-format=h264 -Ob1.yuv /home/stone/workspace/akiyo_352x288_300_IBBBP.h264
    >  ./g2dec --dec-dev=/dev/hantrodec --mem-dev=/dev/memalloc --logoutlevel=3 --logtracemap=CFG --input-format=bs -Ob1.yuv /home/stone/workspace/sample_640x360.hevc

   3) 切换至目录  VC9000E/software/bin/pci/ 目录中，执行
    >    ./hevc_testenc --encDevice=/dev/hantroenc --memDevice=/dev/memalloc -a0 -b299 --rdoLevel=1 --refRingBufEnable=0 -i/home/stone/workspace/YUV/akiyo_352x288_300.yuv --inputFormat=0 --gopSize=1 --enableRdoQuant=0 --lumWidthSrc=352 --lumHeightSrc=288 --width=352 --height=288 --codecFormat=hevc --inputAlignmentExp=0 --aqInfoAlignmentExp=6 --bitDepthLuma=8 --bitDepthChroma=8 --refAlignmentExp=0 --refChromaAlignmentExp=6 -o hevc_case_id_118_cmodel_cmds.hevc
