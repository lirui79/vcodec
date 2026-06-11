/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Verisilicon Inc.                                --
--                                                                            --
--                   (C) COPYRIGHT 2015 VeriSilicon Inc                       --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--         The entire notice above must be reproduced on all copies.          --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Description : Flexible Reference List Organize
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Table of context

    1. Include headers
    2. External compiler flags
    3. Module defines
    4. Local function prototypes
    5. Functions

------------------------------------------------------------------------------*/
#include <math.h>
#include "osal.h"

#include "vsi_string.h"
#include "base_type.h"
#include "error.h"
#include "hevcencapi.h"
#include "HevcTestBench.h"
#include "enccommon.h"
#include "test_bench_utils.h"
#include "test_bench_pic_config.h"
#ifdef INTERNAL_TEST
#include "sw_test_id.h"
#endif
#ifdef TEST_DATA
#include "enctrace.h"
#endif

#define MAX_LINE_LENGTH_BLOCK 512 * 8
#define MAX_LINE_LENGTH 1024

/**
\page flexrefs Flexible Reference Configuration

The video encoder supports specifying a reference description file for HEVC (H.265) and H.264
(AVC) for flexible reference configuration.

The reference description file contains a reference list in a required format. The test bench
checks the reference list and reports errors if any. If the reference list is valid, the test
bench encodes the frames specified in the list, and exits after the encoding is completed.

If a reference description file is specified, look-ahead encoding and gradual decoder refresh
(GDR) are not supported.


\section flexRefs_s1 Reference Description File Format

Each line in the reference description file must contain the following fields in order:
- <b>FrameN</b>: the frame index.
- <b>Type</b>: the slice type, which can be P, B, or IDR.
- <b>POC</b>: the display order of the frame.
  The display order starts from 0 at each IDR frame.
- <b>Qd</b>: the offset to be added to the QP parameter to obtain the final QP value used for
the frame.
  The offset is either specified by the application or calculated by the rate control algorithm.
- <b>Qft</b>: the weight used during rate distortion optimization.
- <b>TID</b>: the temporal layer ID.
- <b>LTR</b>: the LTR index if the current picture as LTR frame;
- <b>num_ref_pics</b>: the number of reference frames kept for the current frame.

  The reference frames can be used by either the current frame or a subsequent frame.
- <b>ref_pics</b>: a list of num_ref_pics entries in format [^]<poc_diff> or [^]L<poc> to indicate
  the reference frames of the current frame.
  - The optional prefix ^ indicates that the corresponding frame is used for reference by
    the current frame.
    - A P-frame can use only one other frame as its reference.
    - A B-frame can use two other frames as its reference.
  - <poc_diff> indicates the POC offset of the reference frame relative to the current
    frame and that the frame is used for short-term reference.
  - [^]L<poc> indicates the POC of the reference frame and that the frame is used for
    long-term reference.

  For each frame, a maximum of four short-term reference frames and two long-term reference frames
  can be kept. The short-term reference frames must be ordered in ascending order of POC offsets,
  and listed before long-term reference frames.
- <b>used_by_cur</b>: a list of num_ref_pics binaries, each indicating whether the corresponding
  frame in the ref_pics list is used as a reference for encoding of the current frame.


\section flexRefs_s2 Reference Description File Configurations

To specify a reference description file, you need to:
- Specify the <tt>\-\-flexRefs</tt> option.
- Specify <tt>\-\-RPSInSliceHeader=1</tt>.


\section flexRefs_s3 Example of Configuring a Reference Description File

This section provides an example of encoding eight frames with a referene description file.

<b>Command</b>

<pre>
./hevc_testenc -i foreman_cif.yuv -w352 -h288
    --RPSInSliceHeader=1 --flexRefs refs.lst -o flex.hevc
</pre>

<b>Reference Description File</b>

The contents of the <tt>vrefs.lst</tt> file in the command are shown as follows:

<pre>
#  Frm Type POC Qd Qft TID LTR  N ref_pics    used_by_cur
	0	IDR  0	 0	0.5  0	 0	0
	4	P	 4	 0	0.5  0	 0	1 ^-4		  1
	2	B	 2	 0	0.5  0	 0	2 ^-2 ^2	  1 1
	1	B	 1	 0	0.5  0	 0	3 ^-1 ^1 3	  1 1
	3	B	 3	 0	0.5  0	 0	2 ^-1 ^1	  1 1
	8	P	 8	 0	0.5  0	 1	1 ^-4		  1
	6	B	 6	 0	0.5  0	 0	2 ^-2 ^L8	  1 1
	5	B	 5	 0	0.5  0	 0	3 ^-1 ^1 L8   1 1
	7	B	 7	 0	0.5  0	 0	2 ^-1 ^L8	  1 1
</pre>
*/

/*
 * Check if the flexRefs file is valid;
 *
 * \return 0 sucess;
 * \return -1 fail;
 */
static int CheckFlexRefsFile(FILE *fp) {
  const int MAX_REFS_PER_BFRAME = 2;
  const int MIN_REFS_PER_BFRAME = 2;
  const int MAX_REFS_PER_PFRAME = 1;
  const int MIN_REFS_PER_PFRAME = 1;
  u32 ref_list[VCENC_MAX_REF_FRAMES + 1];
  VCEncGopPicConfig picCfg;
  i32 i, j, frame, cur_poc, ref_poc, used_cnt;
  ltr_struct tmp;

  fseek(fp, 0L, SEEK_SET);

  // initialize as invalid
  for (i = 0; i < VCENC_MAX_REF_FRAMES + 1; i++) {
    ref_list[i] = -1;
  }

  do {
    frame = GetNextReferenceControl(fp, &picCfg, &tmp);
    if (frame == -2) break;
    if (frame < 0) return -1;

    cur_poc = picCfg.poc;

    used_cnt = 0;
    for (i = 0; i < picCfg.numRefPics; i++) {
      if (IS_LONG_TERM_REF_DELTAPOC(picCfg.refPics[i].ref_pic)) {
        ref_poc = LONG_TERM_REF_DELTAPOC2ID(picCfg.refPics[i].ref_pic);
      } else {
        ref_poc = cur_poc + picCfg.refPics[i].ref_pic;
      }
      for (j = 0; j < VCENC_MAX_REF_FRAMES + 1; j++) {
        if (ref_list[j] == ref_poc) break;
      }
      if (j == VCENC_MAX_REF_FRAMES + 1) {
        printf("[flexRefs] Fail to find reference %d for Frame %d\n", ref_poc,
               frame);
        return -1;
      }
      if (picCfg.refPics[i].used_by_cur) used_cnt++;
    }

    if (picCfg.codingType == VCENC_PREDICTED_FRAME &&
        (used_cnt > MAX_REFS_PER_PFRAME || used_cnt < MAX_REFS_PER_PFRAME)) {
      printf("[flexRefs] Two many USED reference for P Frame %d\n", frame);
      return -1;
    }
    if (picCfg.codingType == VCENC_BIDIR_PREDICTED_FRAME &&
        (used_cnt > MAX_REFS_PER_BFRAME || used_cnt < MAX_REFS_PER_BFRAME)) {
      printf("[flexRefs] Two many USED reference for B Frame %d\n", frame);
      return -1;
    }

    // setup new reference list
    for (i = 0; i < picCfg.numRefPics; i++) {
      if (IS_LONG_TERM_REF_DELTAPOC(picCfg.refPics[i].ref_pic)) {
        ref_poc = LONG_TERM_REF_DELTAPOC2ID(picCfg.refPics[i].ref_pic);
      } else {
        ref_poc = cur_poc + picCfg.refPics[i].ref_pic;
      }
      ref_list[i] = ref_poc;
    }
    // add current frame
    ref_list[i] = cur_poc;
    for (i += 1; i < VCENC_MAX_REF_FRAMES + 1; i++) {
      ref_list[i] = -1;
    }
  } while (1);

  fseek(fp, 0L, SEEK_SET);

  return 0;
}

/**
 * when flexible reference enable, open the description file.
 *
 * \param IN cml->flexRefsFile if not MULL, specify the description file
 * \prarm OUT tb->flex_fp null if cml->flexRefsFile is NULL, or return the file handle
 * \return 0 sucess open the file or skip the operation if not enable
 *         -1 fail to open the file.
 */
i32 OpenFlexRefsFile(struct test_bench *tb, commandLine_s *cml) {
  i32 i;

  if (cml->flexRefs == NULL) {
    return 0;
  }

  if (cml->gdrDuration != 0) {
    printf("Error: Fail to enable flex rps when GDR is enable.\n");
    return -1;
  }
  if (cml->lookaheadDepth != 0) {
    printf("Error: Fail to enable flex rps when lookahead is enable.\n");
    return -1;
  }
  if (cml->RpsInSliceHeader == 0) {
    printf(
        "Error: Fail to enable flex rps when rps in slice header is "
        "disable.\n");
    return -1;
  }

  tb->flexRefsFile = fopen(cml->flexRefs, "r");
  if (tb->flexRefsFile == NULL) {
    printf("Error: Fail to open file %s\n", cml->flexRefs);
    return -1;
  }

  if (CheckFlexRefsFile(tb->flexRefsFile) != 0) {
    printf("Error: Fail to Reference list check for file %s\n", cml->flexRefs);
    return -1;
  }

  return 0;
}

int FillInputOptionsForFlexRps(VCEncIn *in, VCEncGopPicConfig *rps) {
  int i;

  /*in->bIsPeriodUsingLTR = 0;
  in->u8IdxEncodedAsLTR = 0;
  in->bIsPeriodUpdateLTR = 0;
  for(i=0; i<VCENC_MAX_LT_REF_FRAMES; i++) {
    in->long_term_ref_pic[i] = INVALITED_POC;
    in->bLTR_need_update[i] = 0;
    in->long_term_ref_pic[i] = -1;
  }*/

  in->i8SpecialRpsIdx = -1;
  in->i8SpecialRpsIdx_next = -1;

  in->gopSize = 1;
  in->gopPicIdx = 0;
  in->gopConfig.pGopPicCfg = rps;
  in->gopConfig.size = 1;
  in->gopConfig.ltrcnt = 2;  //max 2

  in->poc = rps->poc;

  return 0;
}

/*
 * Parse the reference control information for next encoding frame.
 *
 * \return >=0 the frame number of next encoding picture, start from 0.
 * \return -1 error format.
 * \return -2 end of file read.
 */
int GetNextReferenceControl(FILE *fp, VCEncGopPicConfig *rps,
                            ltr_struct *ltr_t) {
#define CheckTokenError(token)                                         \
  if (!line) {                                                         \
    printf("Error: in flexRefs file line %d, no valid token for %s\n", \
           line_idx, token);                                           \
    return -1;                                                         \
  }

#define CheckParseError(token)                                           \
  if (ret == 0) {                                                        \
    printf("Error: in flexRefs file line %d, cannot get %s\n", line_idx, \
           token);                                                       \
    return -1;                                                           \
  }

#define ShowValueError(token, line)                                         \
  do {                                                                      \
    printf("Error: in flexRefs file line %d, invalid value for %s\n", line, \
           token);                                                          \
    return -1;                                                              \
  } while (0)

  static int line_idx = 0;
  char line_buffer[MAX_LINE_LENGTH];

  char *line = NULL;
  int frame, poc, num_ref_pics, ref_is_ltr, ref_pic;
  char type;
  int i, ret;
  int ltr_idx = 0;

  while (1) {
    if (feof(fp)) return -2;
    line_idx++;
    line_buffer[0] = '\0';
    // Read one line
    line = fgets((char *)line_buffer, MAX_LINE_LENGTH, fp);
    if (!line) return -2;
    if (line[0] != '#') break;
  }

  //handle line end
  char *s = strpbrk(line, "\n");
  if (s) *s = '\0';

  //format: POC Type QPoffset QPfactor TemporalId LTR num_ref_pics ref_pics

  //frame index
  line = nextToken(line);
  CheckTokenError("frame");
  ret = sscanf(line, "%d", &frame);
  CheckParseError("frame");

  //frame type
  line = nextToken(line);
  CheckTokenError("type");
  ret = sscanf(line, "%c", &type);
  CheckParseError("type");
  if (type == 'P' || type == 'p')
    rps->codingType = VCENC_PREDICTED_FRAME;
  else if (type == 'B' || type == 'b')
    rps->codingType = VCENC_BIDIR_PREDICTED_FRAME;
  else if (type == 'I' || type == 'i')
    rps->codingType = VCENC_INTRA_FRAME;
  else {
    ShowValueError("type", line_idx);
  }
  rps->nonReference = 0;

  //poc
  line = nextToken(line);
  CheckTokenError("poc");
  ret = sscanf(line, "%d", &poc);
  CheckParseError("poc");
  rps->poc = poc;

  //qp offset
  line = nextToken(line);
  CheckTokenError("QPoffset");
  ret = sscanf(line, "%d", &(rps->QpOffset));
  CheckParseError("QPoffset");
  if (rps->QpOffset < 0 || rps->QpOffset > 51) {
    ShowValueError("QPoffset", line_idx);
  }

  //qp factor
  line = nextToken(line);
  CheckTokenError("QpFactor");
  ret = sscanf(line, "%lf", &(rps->QpFactor));
  CheckParseError("QpFactor");
  rps->QpFactor = sqrt(rps->QpFactor);

  //temporalId factor
  line = nextToken(line);
  CheckTokenError("temporalId");
  ret = sscanf(line, "%d", &(rps->temporalId));
  CheckParseError("QpFactor");

  //mark as ltr
  line = nextToken(line);
  CheckTokenError("ltr");
  ret = sscanf(line, "%d", &ltr_t->encoded_as_ltr);
  CheckParseError("ltr");
  if (ltr_t->encoded_as_ltr > 2)  //current only test 2 long term
  {
    ShowValueError("ltr", line_idx);
  }

  //num_ref_pics
  line = nextToken(line);
  CheckTokenError("num_ref_pics");
  ret = sscanf(line, "%d", &num_ref_pics);
  CheckParseError("num_ref_pics");
  if (num_ref_pics < 0 || num_ref_pics > VCENC_MAX_REF_FRAMES) {
    ShowValueError("num_ref_pic", line_idx);
  }
  rps->numRefPics = num_ref_pics;

  //ref_pics
  for (i = 0; i < num_ref_pics; i++) {
    line = nextToken(line);
    CheckTokenError("ref_pic");
    // used_by_cur
    if (line[0] == '^') {
      rps->refPics[i].used_by_cur = 1;
      line += 1;
    } else {
      rps->refPics[i].used_by_cur = 0;
    }
    // long term
    if (line[0] == 'L') {
      ref_is_ltr = 1;
      line += 1;
    } else {
      ref_is_ltr = 0;
    }

    // poc diff or poc
    ret = sscanf(line, "%d", &ref_pic);
    CheckParseError("ref_pic");
    if (ref_is_ltr) {
      rps->refPics[i].ref_pic = LONG_TERM_REF_ID2DELTAPOC(ref_pic);
      ltr_t->bLTR_used_by_cur[ltr_idx++] = rps->refPics[i].used_by_cur;
    } else {
      rps->refPics[i].ref_pic = ref_pic;
    }
  }

  if (i < num_ref_pics) {
    ShowValueError("num_ref_pics/ref_pic mismatch", line_idx);
  }

  return frame;
}
