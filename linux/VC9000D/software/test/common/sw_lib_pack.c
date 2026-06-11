/*-------------------------------------------------------------------------------
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
--------------------------------------------------------------------------------*/
#if defined(_WINDOWS) || defined(_WIN32) || defined(_MSC_VER)
/* In fact, the following code is invalid, We already added these libs to the
 * vcmd_sw.lib in the project properties. But in practice, we found that Visual
 * studio 2015 can not combine the libs to a new lib without adding a resources
 * file, so we added such a redundant file. (The project can generate target
 * lib with this file is null, but if it's deleted, can't generate target.)*/
#pragma comment(lib, "common.lib")
#pragma comment(lib, "dwl.lib")
#pragma comment(lib, "hevc.lib")
#pragma comment(lib, "jpeg.lib")
#pragma comment(lib, "vp9.lib")
#pragma comment(lib, "h264high.lib")
#pragma comment(lib, "av1.lib")
#pragma comment(lib, "mpeg2.lib")
#endif