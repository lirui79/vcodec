/*------------------------------------------------------------------------------
--                                                                                                                               --
--       This software is confidential and proprietary and may be used                                   --
--        only as expressly authorized by a licensing agreement from                                     --
--                                                                                                                               --
--                            Verisilicon.                                                                                    --
--                                                                                                                               --
--                   (C) COPYRIGHT 2014 VERISILICON                                                            --
--                            ALL RIGHTS RESERVED                                                                    --
--                                                                                                                               --
--                 The entire notice above must be reproduced                                                  --
--                  on all copies and should not be removed.                                                     --
--                                                                                                                                --
--------------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include "sw_skip_frame.h"
#include "sw_put_bits.h"
#include "test_data_define.h"
#include "sw_cabac_defines.h"
#include "enccommon.h"
#include "sw_slice.h"
#include "instance.h"
#include "tools.h"


/*------------------------------------------------------------------------------
                            h264 cabac stuff
 ------------------------------------------------------------------------------*/
 static const i32 sw_skip_h264ContextInit[3][25][2] = {
    /* cabac_init_idc == 0 */
    {
     /* 0 -> 10 */
     {20, -15}, {2, 54}, {3, 74}, {20, -15},
     {2, 54}, {3, 74}, {-28, 127}, {-23, 104},
     {-6, 53}, {-1, 54}, {7, 51},

     /* 11 -> 23 */
     {23, 33}, {23, 2}, {21, 0}, {1, 9},
     {0, 49}, {-37, 118}, {5, 57}, {-13, 78},
     {-11, 65}, {1, 62}, {12, 49}, {-4, 73},
     {17, 50},

     /* 24 -> 39 */
     {18, 64},
    },
};
static const u8 sw_skip_range_table_lps[64][4] =
{
    {128, 176, 208, 240},
    {128, 167, 197, 227},
    {128, 158, 187, 216},
    {123, 150, 178, 205},
    {116, 142, 169, 195},
    {111, 135, 160, 185},
    {105, 128, 152, 175},
    {100, 122, 144, 166},
    { 95, 116, 137, 158},
    { 90, 110, 130, 150},
    { 85, 104, 123, 142},
    { 81,  99, 117, 135},
    { 77,  94, 111, 128},
    { 73,  89, 105, 122},
    { 69,  85, 100, 116},
    { 66,  80,  95, 110},
    { 62,  76,  90, 104},
    { 59,  72,  86,  99},
    { 56,  69,  81,  94},
    { 53,  65,  77,  89},
    { 51,  62,  73,  85},
    { 48,  59,  69,  80},
    { 46,  56,  66,  76},
    { 43,  53,  63,  72},
    { 41,  50,  59,  69},
    { 39,  48,  56,  65},
    { 37,  45,  54,  62},
    { 35,  43,  51,  59},
    { 33,  41,  48,  56},
    { 32,  39,  46,  53},
    { 30,  37,  43,  50},
    { 29,  35,  41,  48},
    { 27,  33,  39,  45},
    { 26,  31,  37,  43},
    { 24,  30,  35,  41},
    { 23,  28,  33,  39},
    { 22,  27,  32,  37},
    { 21,  26,  30,  35},
    { 20,  24,  29,  33},
    { 19,  23,  27,  31},
    { 18,  22,  26,  30},
    { 17,  21,  25,  28},
    { 16,  20,  23,  27},
    { 15,  19,  22,  25},
    { 14,  18,  21,  24},
    { 14,  17,  20,  23},
    { 13,  16,  19,  22},
    { 12,  15,  18,  21},
    { 12,  14,  17,  20},
    { 11,  14,  16,  19},
    { 11,  13,  15,  18},
    { 10,  12,  15,  17},
    { 10,  12,  14,  16},
    {  9,  11,  13,  15},
    {  9,  11,  12,  14},
    {  8,  10,  12,  14},
    {  8,   9,  11,  13},
    {  7,   9,  11,  12},
    {  7,   9,  10,  12},
    {  7,   8,  10,  11},
    {  6,   8,   9,  11},
    {  6,   7,   9,  10},
    {  6,   7,   8,   9},
    {  2,   2,   2,   2}
};

static const u8 sw_skip_trans_idx_mps[64] =
{
    1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
    31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
    51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
    61, 62, 62, 63
};

static const u8 sw_skip_trans_idx_lps[64] =
{
    0,  0,  1,  2,  2,  4,  4,  5,  6,  7,
    8,  9,  9, 11, 11, 12, 13, 13, 15, 15,
    16, 16, 18, 18, 19, 19, 21, 21, 22, 22,
    23, 24, 24, 25, 26, 26, 27, 27, 28, 29,
    29, 30, 30, 30, 31, 32, 32, 33, 33, 33,
    34, 34, 35, 35, 35, 36, 36, 36, 37, 37,
    37, 38, 38, 63
};
static u32 sw_skip_cabac_init_h264(struct cabac *c,u32 cabac_init_idc, i32 qp);
static u32 sw_skip_cabac_init_h264(struct cabac *c,        u32 cabac_init_idc, i32 qp)
{
    ASSERT(cabac_init_idc == 0);//fix 0

    u8 *ctx = c->ctx;
    i32 i;
    const i32(*ctxIn)[25][2];
    c->cod_low = 0;
    c->cod_range = 510;
    c->first_bit = 1;
    c->bits_outstanding = 0;

    ctxIn = &sw_skip_h264ContextInit[cabac_init_idc];

    for(i = 0; i < 25; i++)
    {
        i32 m = (i32) (*ctxIn)[i][0];
        i32 n = (i32) (*ctxIn)[i][1];

        i32 preCtxState = CLIP3(1, 126, ((m * (i32) qp) >> 4) + n);

        if(preCtxState <= 63)
        {
            ctx[i] =
                (u8) ((63 - preCtxState) << 1);
        }
        else
        {
            ctx[i] =
                (u8) (((preCtxState - 64) << 1) | 1);
        }
    }
    return 0;
}

/*------------------------------------------------------------------------------
                           hevc cabac stuff
------------------------------------------------------------------------------*/
/* Table 9-31 Values of variable initValue for split_cu_flag ctxIdx */
static const u8 sw_skip_table_split_cu_flag[3][3] =
{
    {139, 141, 157},
    {107, 139, 126},
    {107, 139, 126},
};

/* Table 9-9 Values of variable initValue for skip_flag ctxIdx */
static const u8 sw_skip_table_skip_flag[3][3] =
{
    {154, 154, 154},
    {197, 185, 201},
    {197, 185, 201},
};

/* Table 9-16 Values of variable initValue for merge_idx ctxIdx */
static const u8 sw_skip_table_merge_idx[3][1] =
{
    {154},
    {122},
    {137},
};
/*------------------------------------------------------------------------------
sw_skip_ctx_init NOTE contex compression like HM 8.1
p_state_idx = ctx >> 1;
val_mps   = ctx & 0x1;
------------------------------------------------------------------------------*/
static u8 *sw_skip_ctx_init(u8 *ctx, u8 const *init_values, i32 size, i32 qp);
static void sw_skip_cabac_init_hevc(struct cabac *c, enum slice_type slice_type,
                             i32 cabac_init_flag, i32 qp);
static void sw_skip_cabac_put_bit(struct cabac *c, i32 bit);
static void sw_skip_cabac_renorm(struct cabac *c);
static void sw_skip_cabac_flush(struct cabac *c);
static void sw_skip_cabac_terminate(struct cabac *c, i32 bin);
static void sw_skip_cabac(struct cabac *c, i32 ctx_idx, i32 bin);
static void sw_skip_slice_alignment_one_bits_h264(struct buffer *b);
static u8 *sw_skip_ctx_init(u8 *ctx, u8 const *init_values, i32 size, i32 qp)
{
    i32 tmp, m, n, i;
    i32 pre_ctx_state;

    for (i = 0; i < size; i++)
    {
        tmp = init_values[i];
        m = (tmp >> 4) * 5 - 45;
        n = ((tmp & 15) << 3) - 16;
        tmp = ((m * qp) >> 4) + n;
        pre_ctx_state = CLIP3(1, 126, tmp);

        if (pre_ctx_state <= 63)
        {
            *ctx = ((63 - pre_ctx_state) << 1) | 0;
        }
        else
        {
            *ctx = ((pre_ctx_state - 64) << 1) | 1;
        }
        ctx++;
    }
    return ctx;
}

static void sw_skip_cabac_init_hevc(struct cabac *c, enum slice_type slice_type,
                             i32 cabac_init_flag, i32 qp)
{
    u8 *ctx = c->ctx;
    i32 i;

    c->cod_low = 0;
    c->cod_range = 510;
    c->first_bit = 1;
    c->bits_outstanding = 0;

    if (slice_type == I_SLICE)
    {
        i = 0;
    }
    else if (slice_type == P_SLICE)
    {
        i = cabac_init_flag ? 2 : 1;
    }
    else
    {
        i = cabac_init_flag ? 1 : 2;
    }

    /* Table 9-7    :2*/
    ctx = c->ctx + 2;
    c->split_cu_flag = ctx - c->ctx;
    ctx = sw_skip_ctx_init(ctx, sw_skip_table_split_cu_flag[i], sizeof(sw_skip_table_split_cu_flag[i]), qp);

    /* Table 9-9    :5*/
    c->skip_flag = ctx - c->ctx;
    ctx = sw_skip_ctx_init(ctx, sw_skip_table_skip_flag[i], sizeof(sw_skip_table_skip_flag[i]), qp);

    /* Table 9-16   :21*/
    ctx = c->ctx + 21;
    c->merge_idx = ctx - c->ctx;
    ctx = sw_skip_ctx_init(ctx, sw_skip_table_merge_idx[i], sizeof(sw_skip_table_merge_idx[i]), qp);

#ifdef TEST_DATA
    c->terminate_flag = 0;
#endif
    //ctx size increased for H.264
    ASSERT(sizeof(c->ctx) >= (ctx - c->ctx)*sizeof(u8));
}

static void sw_skip_cabac_put_bit(struct cabac *c, i32 bit)
{
    if (c->first_bit)
    {
        c->first_bit = 0;
    }
    else
    {
        put_bit(&c->b, bit, 1);
#ifdef TEST_DATA
        c->test_bits <<= 1;
        c->test_bits |= bit;
        c->test_bits_num++;
#endif
    }

    while (c->bits_outstanding > 0)
    {
        put_bit(&c->b, !bit, 1);
#ifdef TEST_DATA
        c->test_bits <<= 1;
        c->test_bits |= !bit;
        c->test_bits_num++;
#endif
        c->bits_outstanding--;
    }
}
static void sw_skip_cabac_renorm(struct cabac *c)
{
    while (c->cod_range < 256)
    {
        if (c->cod_low < 256)
        {
            sw_skip_cabac_put_bit(c, 0);
        }
        else if (c->cod_low >= 512)
        {
            c->cod_low -= 512;
            sw_skip_cabac_put_bit(c, 1);
        }
        else
        {
            c->cod_low -= 256;
            c->bits_outstanding++;
        }
        c->cod_range <<= 1;
        c->cod_low <<= 1;
    }

}
static void sw_skip_cabac_flush(struct cabac *c)
{
    COMMENT(&c->b, "cabac_flush");
    c->cod_range = 2;
    sw_skip_cabac_renorm(c);
    sw_skip_cabac_put_bit(c, (c->cod_low >> 9) & 0x1);
    put_bit(&c->b, (c->cod_low >> 8) & 0x1, 1);
#ifdef TEST_DATA
    c->test_bits <<= 1;
    c->test_bits |= (c->cod_low >> 8) & 0x1;
    c->test_bits_num++;
#endif

}

static void sw_skip_cabac_terminate(struct cabac *c, i32 bin)
{

#ifdef TEST_DATA
    c->terminate_flag = 1;
    c->test_bits = 0;
    c->test_bits_num = 0;
#endif

    c->cod_range -= 2;
    if (bin != 0)
    {
        c->cod_low += c->cod_range;
        sw_skip_cabac_flush(c);
    }
    else
    {
        sw_skip_cabac_renorm(c);
    }

}

static void sw_skip_cabac(struct cabac *c, i32 ctx_idx, i32 bin)
{
    i32 ctx = c->ctx[ctx_idx];
    i32 p_state_idx = ctx >> 1;
    i32 val_mps = ctx & 0x1;
    i32 q_cod_range_idx = (c->cod_range >> 6) & 0x3;
    i32 cod_range_lps = sw_skip_range_table_lps[p_state_idx][q_cod_range_idx];
#ifdef TEST_DATA
    c->terminate_flag = 0;
    c->test_bits = 0;
    c->test_bits_num = 0;
    c->b.input_cabac_BIN_number += 1;
#endif

    c->cod_range -= cod_range_lps;
    if (bin != val_mps)
    {
        c->cod_low += c->cod_range;
        c->cod_range = cod_range_lps;
        if (p_state_idx == 0)
        {
            val_mps = !val_mps;
        }
        c->ctx[ctx_idx] = (sw_skip_trans_idx_lps[p_state_idx] << 1) | val_mps;
    }
    else
    {
        c->ctx[ctx_idx] = (sw_skip_trans_idx_mps[p_state_idx] << 1) | val_mps;
    }
    sw_skip_cabac_renorm(c);
}
/*------------------------------------------------------------------------------
                            h264 slice header
 ------------------------------------------------------------------------------*/
static void sw_skip_slice_alignment_one_bits_h264(struct buffer *b)
{
    if (buffer_full(b)) return;

    while (b->bit_cnt % 8)
    {
        COMMENT(b, "cabac_alignment_one_bit");
        put_bit(b, 1, 1);
    }

    while (b->bit_cnt)
    {
        /* Flush next byte to stream */
        if ((b->bit_cnt >= 24) && ((b->cache & 0xFFFFFC00) == 0))
        {
            *b->stream++ = 0;
            *b->stream++ = 0;
            *b->stream++ = 0x03;

            (*b->cnt) += 3;
            b->cache <<= 16;
            b->bit_cnt -= 16;
        }
        else
        {
            *b->stream++ = b->cache >> 24;
            (*b->cnt)++;
            b->cache <<= 8;
            b->bit_cnt -= 8;
        }
    }
}

enum sw_skip_slice_type_264
{
    P_SLICE_H264 = 0,
    B_SLICE_H264 = 1,
    I_SLICE_H264 = 2,
    P_SLICES_H264 = 5,
    B_SLICES_H264 = 6,
    I_SLICES_H264 = 7
};
static void sw_skip_slice_h264(VCEncInst inst, struct sw_picture *pic, struct slice *slice, int byteStream);
static void sw_skip_ref_pic_lists_modification (VCEncInst inst, struct sw_picture *pic, struct slice *s);
static void sw_skip_slice_hevc(VCEncInst inst, struct sw_picture *pic, struct slice *slice, int byteStream);
static void sw_skip_slice_h264(VCEncInst inst, struct sw_picture *pic, struct slice *slice, int byteStream)
{
    struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
    struct slice *s = slice;
    s->cabac.b = vcenc_instance->stream;
    struct buffer *b = &s->cabac.b;
    i32 first_slice_in_pic_flag;
    i32 dependent_slice_flag = 0;
    i32 tmp;
    enum sw_skip_slice_type_264 h264_type=0;
    i32 poc_bits = pic->sps->log2MaxpicOrderCntLsbMinus4 + 4;
    i32 frame_num_bits = pic->sps->log2MaxFrameNumMinus4 + 4;

    if( vcenc_instance->asic.regs.frameCodingType == 2 )//B
        h264_type = B_SLICE_H264;
    else if( vcenc_instance->asic.regs.frameCodingType == 1 )//I
        h264_type = I_SLICE_H264;
    else if( vcenc_instance->asic.regs.frameCodingType == 0 )//P
        h264_type = P_SLICE_H264;

    //if(s->prefixnal_svc_ext) //not support
    //prefix_svc_ext(p, c);

    u32 IdrPicFlag = 0;//(s->nal_unit.type == ASIC_H264_IDR);

    /* Byte stream header 0x00000001 */
    if (byteStream)
    {
        put_bits_startcode(b);
    }

    put_bit(b, 0, 1);
    COMMENT(b, "forbidden_zero_bit");

    put_bit(b, vcenc_instance->asic.regs.nalRefIdc, 2);   // s->nalRefIdc
    COMMENT(b, "nal_ref_idc");

    //put_bit(b, (i32) s->nal_unit.type, 5);H264_NONIDR
    put_bit(b, (i32) H264_NONIDR, 5);
    COMMENT(b, "nal_unit_type");

    s->slice_address = 0;//c->num;
    put_bit_ue(b, s->slice_address);
    COMMENT(b, "first_mb_in_slice");

    put_bit_ue(b, h264_type);/*(i32)s->type_264 + 5*/
    COMMENT(b, "slice_type");

    put_bit_ue(b, pic->pps->ps.id); //p->pps->ps.id
    COMMENT(b, "pic_parameter_set_id");

    put_bit_32(b, vcenc_instance->asic.regs.frameNum, frame_num_bits);
    COMMENT(b, "frame_num");

    /*if (IdrPicFlag) {
      put_bit_ue(b, s->idrPicId);
      COMMENT(b, "idr_pic_id");
      TRACING(write_slice_header, "idr_pic_id", s->idrPicId);
    }*/

    if(pic->sps->picOrderCntType == 0)
    {
        put_bit_32(b, vcenc_instance->asic.regs.poc, poc_bits);
        COMMENT(b, "pic_order_cnt_lsb");
    }

    /* if( pic_order_cnt_type = = 1 && etc... not implemented */
    /* if( redundant_pic_cnt_present_flag ) etc... not implemented */

    if( h264_type == B_SLICE_H264 ) //s->type_264
    {
        put_bit_32(b, 1, 1);
        COMMENT(b, "direct_spatial_mv_pred_flag");
    }
    if (h264_type != I_SLICE_H264)//( s->type_264 != I_SLICE_H264 )
    {
        #if 0
        if (slice->numRefUsed > 1)
        {
            H264AsicPutBits(stream, 1, 1);
            COMMENT("num_ref_idx_active_override_flag");
            H264AsicExpGolombUnsigned(stream, 1);
            COMMENT("num_ref_idx_active_minus1");
        }
        else
        #endif
        {
            put_bit(b, 0, 1);
            COMMENT(b,"num_ref_idx_active_override_flag");
            #if 0
            if( num_ref_idx_active_override_flag )
            {
                num_ref_idx_l0_active_minus1
                if( slice_type = = B )
                    num_ref_idx_l1_active_minus1
            }
            #endif
        }
    }

    // ref_pic_list_reordering
    if (h264_type != I_SLICE_H264)  //(s->type_264 != I_SLICE_H264) {
    {
        #if 0
        /* When MVC slice is inter-view predicted from the base view
         * the ref pic list needs to be modified except for anchor. */
        if ((slice->nalUnitType == MVC) && (slice->mvcRefMod))
        {
            /* ref_pic_list_mvc_modification */
            H264AsicPutBits(stream, 1, 1);
            COMMENT("ref_pic_list_modification_flag_l0");
            H264AsicExpGolombUnsigned(stream, 5);
            COMMENT("modification_of_pic_nums_idc");
            H264AsicExpGolombUnsigned(stream, 0);
            COMMENT("abs_diff_view_idx_minus1");
            H264AsicExpGolombUnsigned(stream, 3);
            COMMENT("modification_of_pic_nums_idc");
        }
        /* long-term ref used as primary ref pic */
        else if (slice->nalUnitType != MVC && slice->ltRef)
        {
            H264AsicPutBits(stream, 1, 1);
            COMMENT("ref_pic_list_modification_flag_l0");
            H264AsicExpGolombUnsigned(stream, 2);
            COMMENT("modification_of_pic_nums_idc");
            H264AsicExpGolombUnsigned(stream, 0);
            COMMENT("");
            H264AsicExpGolombUnsigned(stream, 3);
            COMMENT("modification_of_pic_nums_idc");
        }
        else
        #endif

        u32 ref_pic_list_reordering_flag_lx=1;
        u32 reordering_of_pic_nums_idc=0;
        u32 abs_diff_pic_num_minus1=0;
        i32 ref_frame_num=0;
        struct h264_mb_col * colctbs;
        /* L0: ref_pic_list_reordering( ) */
        {
            put_bit(b, ref_pic_list_reordering_flag_lx, 1);
            COMMENT(b,"ref_pic_list_reordering_flag_l0");

            if( ref_pic_list_reordering_flag_lx )
            {
                //remapping
                reordering_of_pic_nums_idc = (pic->rps->ref_pic_s0[0].long_term_flag ? 2 : 0);
                put_bit_ue(b, reordering_of_pic_nums_idc);
                COMMENT(b,"reordering_of_pic_nums_idc");

                //slice->ref_frame_num_l0[0] = slice->frameNum - swctrl->l0_delta_framenum[0];
                ref_frame_num = vcenc_instance->asic.regs.frameNum - vcenc_instance->asic.regs.l0_delta_framenum[0]; //s->ref_frame_num_l0[0];
                abs_diff_pic_num_minus1 = ((vcenc_instance->asic.regs.frameNum - ref_frame_num-1)&0xfff);

                //printf("weli: cur_frame=%d cur_poc=%d L0 delta_poc=%d dalta_framenum=%d ref_frame_num=%d\n", s->frameNum, s->poc, p->rps->ref_pic_s0[0].delta_poc, abs_diff_pic_num_minus1+1, ref_frame_num);
                if( reordering_of_pic_nums_idc == 0 || reordering_of_pic_nums_idc == 1 )
                {
                    put_bit_ue(b, abs_diff_pic_num_minus1);
                    COMMENT(b,"abs_diff_pic_num_minus1");
                }
                else if( reordering_of_pic_nums_idc == 2 )
                {
                    //long_term_pic_num
                    //put_bit_ue(b, s->ref_frame_ltridx_l0[0]);//no replace stuff
                    //COMMENT(b,"long_term_pic_num");
                }

                //end reordering
                reordering_of_pic_nums_idc = 3;
                put_bit_ue(b, reordering_of_pic_nums_idc);
                COMMENT(b,"reordering_of_pic_nums_idc");
            }

        }
        /* L1: ref_pic_list_reordering( ) */
        if(h264_type == B_SLICE_H264)//(s->type_264 == B_SLICE_H264)
        {
            put_bit(b, ref_pic_list_reordering_flag_lx, 1);
            COMMENT(b,"ref_pic_list_reordering_flag_l1");
            //TRACING(write_slice_header,"ref_pic_list_reordering_flag_l1", ref_pic_list_reordering_flag_lx);

            if( ref_pic_list_reordering_flag_lx )
            {
                reordering_of_pic_nums_idc = (pic->rps->ref_pic_s1[0].long_term_flag ? 2 : 0);
                put_bit_ue(b, reordering_of_pic_nums_idc);
                COMMENT(b,"reordering_of_pic_nums_idc");
                //slice->ref_frame_num_l1[0] = slice->frameNum - swctrl->l1_delta_framenum[0];
                ref_frame_num = vcenc_instance->asic.regs.frameNum - vcenc_instance->asic.regs.l1_delta_framenum[0]; //s->ref_frame_num_l1[0];
                abs_diff_pic_num_minus1 = ((vcenc_instance->asic.regs.frameNum - ref_frame_num-1)&0xfff);

                //printf("weli: cur_frame=%d cur_poc=%d L1 delta_poc=%d dalta_framenum=%d ref_frame_num=%d\n", s->frameNum, s->poc, p->rps->ref_pic_s1[0].delta_poc, abs_diff_pic_num_minus1+1, ref_frame_num);
                if( reordering_of_pic_nums_idc == 0 || reordering_of_pic_nums_idc == 1 )
                {
                    put_bit_ue(b, abs_diff_pic_num_minus1);
                    COMMENT(b,"abs_diff_pic_num_minus1");
                }
                else if( reordering_of_pic_nums_idc == 2 )
                {
                    //long_term_pic_num
                    //put_bit_ue(b, s->ref_frame_ltridx_l1[0]);//no replace stuff
                    //COMMENT(b,"long_term_pic_num");
                }
                //end reordering
                reordering_of_pic_nums_idc = 3;
                put_bit_ue(b, reordering_of_pic_nums_idc);
                COMMENT(b,"reordering_of_pic_nums_idc");
            }
        }

    }

    /* if( ( weighted_pred_flag && ( not implemented */
    if (vcenc_instance->asic.regs.nalRefIdc != 0)
    {
        if (IdrPicFlag)//always 0
        {
            put_bit(b, 0, 1);
            COMMENT(b,"no_output_of_prior_pics_flag");
            //put_bit(b, s->markCurrentLongTerm, 1);
            //COMMENT(b,"long_term_reference_flag");
        }
        else
        {
            /* H.264 MMO  libva not support this feature*/
            i32 h264_mmo_nops = 0, i = 0;
            for(i = 0; i < 2; i++)
            if(vcenc_instance->asic.regs.l0_used_by_next_pic[i] == 0)
            {
                h264_mmo_nops++;
            }
            for(i = 0; i < 2; i++)
            if(vcenc_instance->asic.regs.l1_used_by_next_pic[i] == 0) {
              h264_mmo_nops++;
            }
            u32 adaptive_ref_pic_marking_mode_flag = (vcenc_instance->asic.regs.max_long_term_frame_idx_plus1
                         || h264_mmo_nops > 0 || vcenc_instance->asic.regs.markCurrentLongTerm);
            put_bit(b, adaptive_ref_pic_marking_mode_flag, 1);
            COMMENT(b,"adaptive_ref_pic_marking_mode_flag");
            if(vcenc_instance->asic.regs.max_long_term_frame_idx_plus1)
            {
                put_bit_ue(b, 4);
                COMMENT(b,"mmcop = 4");
                put_bit_ue(b, vcenc_instance->asic.regs.max_long_term_frame_idx_plus1);
                COMMENT(b,"max_long_term_frame_idx_plus1");
            }

            for(int i = 0; i < h264_mmo_nops; i++)
            {
                /* mmcop = 1 */
                if(vcenc_instance->h264_mmo_ltIdx[i] < 0)
                {
                    if(!vcenc_instance->h264_mmo_long_term_flag[i])
                    {
                        put_bit_ue(b, 1);
                        COMMENT(b,"mmcop = 1");
                        /* diff */
                        put_bit_ue(b, (vcenc_instance->asic.regs.frameNum - vcenc_instance->h264_mmo_unref[i] - 1) & 0xfff);
                        COMMENT(b,"difference_of_pic_nums_minus1");
                    }
                    else
                    {
                        put_bit_ue(b, 2);
                        COMMENT(b,"mmcop = 2");
                        put_bit_ue(b, vcenc_instance->h264_mmo_unref[i]);
                        COMMENT(b,"long_term_pic_num");
                    }
                }
                else
                {
                    put_bit_ue(b, 3);
                    COMMENT(b,"mmcop = 3");
                    /* diff */
                    put_bit_ue(b, (vcenc_instance->asic.regs.frameNum - vcenc_instance->h264_mmo_unref[i] - 1) & 0xfff);
                    COMMENT(b,"difference_of_pic_nums_minus1");
                    put_bit_ue(b, vcenc_instance->h264_mmo_ltIdx[i]);
                    COMMENT(b,"long_term_frame_idx");
                }
            }
            if (vcenc_instance->asic.regs.markCurrentLongTerm)
            {
                /* mmcop = 6 (current marked as long term) */
                put_bit_ue(b, 6);
                COMMENT(b,"mmcop = 6");
                /* idx */
                put_bit_ue(b, vcenc_instance->asic.regs.currentLongTermIdx);//s->curLongTermIdx
                COMMENT(b,"idx");
            }
            if (adaptive_ref_pic_marking_mode_flag)
            {
                /* mmcop = 0 (end) */
                put_bit_ue(b, 0);
                COMMENT(b,"mmcop = 0");
                //TRACING(write_slice_header,"mmcop", 0);
            }
        }

    }
    //( s->entropyCodingMode == ENCHW_YES && s->type_264 != I_SLICE_H264) {
    if(vcenc_instance->asic.regs.entropy_coding_mode_flag == ENCHW_YES && vcenc_instance->asic.regs.frameCodingType != 1)
    {
        put_bit_ue(b, 0);//s->cabac_init_idc); always 0
        COMMENT(b,"cabac_init_idc");
    }

    put_bit_se(b, vcenc_instance->asic.regs.qp - vcenc_instance->asic.regs.picInitQp); //p->pps->init_qp
    COMMENT(b,"slice_qp_delta");

    /* if( slice_type = = SP || slice_type == SI etc... not implemented */

    if(s->deblocking_filter_override_flag == ENCHW_YES)
    {
        put_bit_ue(b, s->deblocking_filter_disabled_flag);
        COMMENT(b,"disable_deblocking_filter_idc");
        if (!s->deblocking_filter_disabled_flag)
        {
            put_bit_se(b, s->tc_offset / 2);
            COMMENT(b,"slice_alpha_c0_offset_div2");
            put_bit_se(b, s->beta_offset / 2);
            COMMENT(b,"slice_beta_offset_div2");
        }
    }

    /* if( num_slice_groups_minus1 > 0 && etc.. not implemented */

    //cabac
    //if( s->entropyCodingMode == ENCHW_YES)
    if(vcenc_instance->asic.regs.entropy_coding_mode_flag == ENCHW_YES)
        sw_skip_slice_alignment_one_bits_h264(b);
}

/*------------------------------------------------------------------------------
                            hevc slice header
 ------------------------------------------------------------------------------*/
static void sw_skip_ref_pic_lists_modification (VCEncInst inst, struct sw_picture *pic, struct slice *s)
{
    //before/after sub-sets in RPS are different from reference list0/list1.
    //list0 = before + after; list1 = after + before
    //Here ref_pic_s0 is list0, ref_pic_s1 is list1, each ref pic in a list should be with a flag used_by_curr_pic=1 no matter it is actived or not
    struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
    s->cabac.b = vcenc_instance->stream;
    struct buffer *b = &s->cabac.b;
    int i;
    int ref_cnt=0;
    ASSERT (ref_cnt <= 2);
    ASSERT (s->type == P_SLICE);

    for(i=0; i<pic->rps->num_negative_pics; i++) {
        ref_cnt += pic->rps->ref_pic_s0[i].used_by_curr_pic;
    }

    for(i=0; i<pic->rps->num_positive_pics; i++) {
        ref_cnt += pic->rps->ref_pic_s1[i].used_by_curr_pic;
    }

    if (ref_cnt > 1)
    {
        COMMENT(b, "ref_pic_list_modification_flag_l0");
        put_bit(b, s->ref_pic_list_modification_flag_l0, 1);
        if (s->ref_pic_list_modification_flag_l0)
        {
            for(i = 0; i <= s->active_l0_cnt-1; i++)
            {
                COMMENT(b, "list_entry_l0");
                put_bit(b, s->list_entry_l0[i], 1);
            }
        }
#if 0 /* only insert P skip */
        if (s->type == B_SLICE)
        {
            COMMENT(b, "ref_pic_list_modification_flag_l1");
            put_bit(b, s->ref_pic_list_modification_flag_l1, 1);
            if (s->ref_pic_list_modification_flag_l1)
            {
                for(i = 0; i <= s->active_l1_cnt-1; i++)
                {
                    COMMENT(b, "list_entry_l1");
                    put_bit(b, s->list_entry_l1[i], 1);
                }
            }
        }
#endif
    }
}

static void sw_skip_slice_hevc(VCEncInst inst, struct sw_picture *pic, struct slice *slice, int byteStream)
{
    struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
    struct slice *s = slice;
    s->cabac.b = vcenc_instance->stream;
    struct buffer *b = &s->cabac.b;
    i32 first_slice_in_pic_flag;
    i32 dependent_slice_flag = 0;
    i32 tmp;
    i32 log2_max_pic_order_cnt_lsb = 16;

    i32 inter_ref_pic_set_prediction_flag = 0;

    i32 i;
    i32 j;

    /* Byte stream header 0x00000001 */
    if (byteStream)
    {
        put_bits_startcode(b);
    }
    //WRITE_SLICE_HEADER_INPUT(s,p,c);

    /* Nal unit header */
    nal_unit(b, (struct nal_unit *)&s->nal_unit);

    first_slice_in_pic_flag = !s->num; //=1

    COMMENT(b, "first_slice_segment_in_pic_flag");
    put_bit(b, first_slice_in_pic_flag, 1);

    /* RapPicFlag */
    if ((s->nal_unit.type >= BLA_W_LP) &&
            (s->nal_unit.type <= RSV_IRAP_VCL23))
    {
        COMMENT(b, "no_output_of_prior_pics_flag");
        put_bit(b,  pic->pps->no_output_of_prior_pics_flag, 1);
    }

    COMMENT(b, "slice_pic_parameter_set_id");
    put_bit_ue(b, vcenc_instance->asic.regs.pps_id);

    s->slice_address = 0;//c->num;

    if (!first_slice_in_pic_flag)
    {
        if (pic->pps->dependent_slice_enabled_flag)
        {
            COMMENT(b, "dependent_slice_segment_flag");
            put_bit(b, s->dependent_slice_flag, 1);
            dependent_slice_flag = s->dependent_slice_flag;
        }

        COMMENT(b, "slice_segment_address")
        if (log2i(pic->pps->ctb_per_picture, &tmp)) tmp++;
        /*    ASSERT(tmp == (i32)ceil(log2((double)p->pps->ctb_per_picture)));*/
        put_bit_32((struct buffer *)b, 0/*c->num*/, tmp);
    }

    if (!dependent_slice_flag)
    {
        COMMENT(b, "slice_type");
        put_bit_ue(b, s->type);

        if (pic->pps->output_flag_present_flag)
        {
            COMMENT(b, "pic_output_flag");
            put_bit(b, s->pic_output_flag, 1);
        }
        /* TODO: if (separate_colour_plane_flag == 1)... */
        /*  ASSERT(p->sps->chroma_format_idc == 1);*/

        /* !IdrPicFlag */
        if (s->nal_unit.type != IDR_W_RADL &&
                s->nal_unit.type != IDR_N_LP)
        {

            COMMENT(b, "slice_pic_order_cnt_lsb");
            tmp = vcenc_instance->asic.regs.poc % (1 << log2_max_pic_order_cnt_lsb);
            //CHECK_INT(tmp, "slice_pic_order_cnt_lsb", 0, (1 << p->sps->log2_max_pic_order_cnt_lsb), 1);
            put_bit_32(b, tmp , log2_max_pic_order_cnt_lsb);

            COMMENT(b, "short_term_ref_pic_set_sps_flag ");
            put_bit(b, vcenc_instance->asic.regs.short_term_ref_pic_set_sps_flag, 1);

            if (!vcenc_instance->asic.regs.short_term_ref_pic_set_sps_flag)
            {
                if (inter_ref_pic_set_prediction_flag)
                {
                    //TODO
                    // short-term RPS of the current picture is derived based on
                    // the short_term_ref_pic_set( ) syntax structure that is directly included in
                    // the slice headers of the current picture.
                }
                else
                {
                    COMMENT(b, "inter_ref_pic_set_prediction_flag");
                    put_bit(b, inter_ref_pic_set_prediction_flag, 1);

                    COMMENT(b, "num_negative_pics");
                    put_bit_ue(b, vcenc_instance->sHwRps.u3NegPicNum);

                    COMMENT(b, "num_positive_pics");
                    put_bit_ue(b, vcenc_instance->sHwRps.u3PosPicNum);

                    for (tmp = 0; tmp < vcenc_instance->sHwRps.u3NegPicNum; tmp++)
                    {
                        COMMENT(b, "delta_poc_s0_minus1");
                        put_bit_ue(b, vcenc_instance->sHwRps.u20DeltaPocS0[tmp]);

                        COMMENT(b, "used_by_curr_pic_s0_flag");
                        put_bit(b, vcenc_instance->sHwRps.u1DeltaPocS0Used[tmp], 1);
                    }

                    for (tmp = 0; tmp < vcenc_instance->sHwRps.u3PosPicNum; tmp++)
                    {
                        COMMENT(b, "delta_poc_s1_minus1");
                        put_bit_ue(b, vcenc_instance->sHwRps.u20DeltaPocS1[tmp]);

                        COMMENT(b, "used_by_curr_pic_s1_flag");
                        put_bit(b, vcenc_instance->sHwRps.u1DeltaPocS1Used[tmp], 1);
                    }
                }
            }
            else if (pic->sps->num_short_term_ref_pic_sets > 1)
            {

                Int numBits = 0;
                //ASSERT(0); //only support p->sps->num_short_term_ref_pic_sets ==1
                while ((1 << numBits) < pic->sps->num_short_term_ref_pic_sets)
                {
                    numBits++;
                }

                COMMENT(b, "short_term_ref_pic_set_idx ");
                //CHECK_INT(p->rps->ps.id, "short_term_ref_pic_set_idx", 0, 2, 1);
                //TRACING(write_slice_header,"short_term_ref_pic_set_idx", p->rps->ps.id);
                put_bit(b, pic->rps->ps.id, numBits);
            }

            if (pic->sps->long_term_ref_pics_present_flag)
            {
                COMMENT(b, "num_long_term_pics");
                put_bit_ue(b, s->num_long_term_pics);
                for(i = 0; i < 2; i++)
                {
                    if(pic->rps->ref_pic_s0[i].long_term_flag)
                    {
                        COMMENT(b, "poc_lsb_lt");
                        tmp = (vcenc_instance->asic.regs.poc + pic->rps->ref_pic_s0[i].delta_poc) % (1 << pic->sps->log2_max_pic_order_cnt_lsb);
                        put_bit_32(b, tmp, pic->sps->log2_max_pic_order_cnt_lsb);
                        COMMENT(b, "used_by_curr_pic_lt_flag");
                        put_bit(b, pic->rps->ref_pic_s0[i].used_by_curr_pic, 1);
                        COMMENT(b, "delta_poc_msb_present_flag");
                        put_bit(b, 0, 1);
                    }
                    if(pic->rps->ref_pic_s1[i].long_term_flag)
                    {
                        COMMENT(b, "poc_lsb_lt")
                        tmp = (vcenc_instance->asic.regs.poc + pic->rps->ref_pic_s1[i].delta_poc) % (1 << pic->sps->log2_max_pic_order_cnt_lsb);

                        put_bit_32(b, tmp, pic->sps->log2_max_pic_order_cnt_lsb);
                        COMMENT(b, "used_by_curr_pic_lt_flag");
                        put_bit(b, pic->rps->ref_pic_s1[i].used_by_curr_pic, 1);
                        COMMENT(b, "delta_poc_msb_present_flag");
                        put_bit(b, 0, 1);
                    }
                }
            }

            if (pic->sps->temporal_mvp_enable_flag)
            {
                put_bit(b, pic->sps->temporal_mvp_enable_flag, 1);
            }
        }

        if (pic->sps->sao_enabled_flag)
        {
            COMMENT(b, "slice_sao_luma_flag");
            put_bit(b, s->sao_luma_flag, 1);

            COMMENT(b, "slice_sao_chroma_flag");
            put_bit(b, s->sao_chroma_flag, 1);
        }

        if (s->type == P_SLICE || s->type == B_SLICE)
        {

            COMMENT(b, "active_override_flag");
            put_bit(b, s->active_override_flag, 1);

            if (s->active_override_flag)
            {
                COMMENT(b, "active_l0_cnt");
                put_bit_ue(b, s->active_l0_cnt - 1);

                if (s->type == B_SLICE)
                {
                    COMMENT(b, "active_l1_cnt");
                    put_bit_ue(b, s->active_l1_cnt - 1);
                }
            }

            if (pic->pps->lists_modification_present_flag)
                sw_skip_ref_pic_lists_modification(inst, pic, slice);

            if (s->type == B_SLICE)
            {
                //TODO: mvd_l1_zero_flag
                ASSERT(0);
                //mvd_l1_zero_flag==0
                //put_bit(b, 0, 1);
            }

            if (pic->pps->cabac_init_present_flag)
            {
                COMMENT(b, "cabac_init_flag");
                put_bit(b, s->cabac_init_flag, 1);
            }

            if (pic->sps->temporal_mvp_enable_flag)
            {
                //ASSERT(0);
            }

            if ((pic->pps->weighted_pred_flag && (s->type == P_SLICE))
                    || (pic->pps->weighted_bipred_flag && (s->type == B_SLICE)))
            {
                //TODO: pred_weight_table( )
                ASSERT(0);
            }
            COMMENT(b, "five_minus_max_num_merge_cand");
            put_bit_ue(b, 5 - s->max_num_merge_cand);
        }

        COMMENT(b, "slice_qp_delta");
        put_bit_se(b, vcenc_instance->asic.regs.qp - pic->pps->init_qp);

        if (pic->pps->slice_chroma_qp_offsets_present_flag)
        {
            COMMENT(b, "slice_cb_qp_offset");
            put_bit_se(b, s->cb_qp_offset);
            COMMENT(b, "slice_cr_qp_offset");
            put_bit_se(b, s->cr_qp_offset);
        }

        if (pic->pps->deblocking_filter_override_enabled_flag)
        {
            COMMENT(b, "deblocking_filter_override_flag");
            put_bit(b, s->deblocking_filter_override_flag, 1);
        }

        if (s->deblocking_filter_override_flag)
        {
            COMMENT(b, "slice_deblocking_filter_disabled_flag");
            put_bit(b, s->deblocking_filter_disabled_flag, 1);

            if (!s->deblocking_filter_disabled_flag)
            {
                COMMENT(b, "slice_beta_offset_div2");
                put_bit_se(b, s->beta_offset / 2);
                COMMENT(b, "slice_tc_offset_div2");
                put_bit_se(b, s->tc_offset / 2);
            }
        }
    }

    ASSERT(pic->pps->deblocking_filter_disabled_flag &&
           !pic->pps->deblocking_filter_override_enabled_flag ?
           s->deblocking_filter_disabled_flag : 1);

    if (pic->pps->loop_filter_across_slices_enabled_flag && (
                s->sao_luma_flag || s->sao_chroma_flag ||
                !s->deblocking_filter_disabled_flag))
    {
        COMMENT(b, "slice_loop_filter_across_slices_enabled_flag");
        put_bit(b, s->loop_filter_across_slices_enabled_flag, 1);
    }

#if 0 //no tiles
    if (pic->pps->tiles_enabled_flag)
    {
        /*
        TODO: code tile size after all tiles in slice done
        */
        put_bit_ue(b, pic->pps->num_tile_per_slice-1);
        COMMENT(b, "num_entry_point_offsets");

        if((pic->pps->num_tile_per_slice-1)>0)
        {
            put_bit_ue(b, 32-1);
            COMMENT(b, "offset_len_minus1")
        }

        //backup bit status for slice_tile_size finalize after all tiles size known at slice ending
        s->bit_tile_size_in_slice = *b;
        //point cnt to new position to avoid overwrite
        s->bit_tile_size_in_slice.cnt = &(s->slice_head_size_in_byte);
        *s->bit_tile_size_in_slice.cnt = *(b->cnt);

        //tile_idx: first tile in the slice
        pic->pps->slice_start_tile_idx = pic->pps->tile_idx;

        //encoder tile size as 0x0, purpose to insert emu as many as possible to have max possibility length
        for(i=pic->pps->slice_start_tile_idx, j=0; j< (pic->pps->num_tile_per_slice -1); i++, j++)
        {
            pic->pps->tile_size[i] = 1;
            put_bit_32(b, pic->pps->tile_size[i]-1, 32);
            //COMMENT(b, "entry_point_offset_minus1")
        }
    }
#endif
    /* TODO: if (slice_header_extension_present_flag)... */
    rbsp_trailing_bits(b);
}
static void sw_skip_end_of_slice (VCEncInst inst, i32 last_ctb, struct slice *slice, int *mbSkipRun);
static void sw_skip_end_of_slice (VCEncInst inst, i32 last_ctb, struct slice *slice, int *mbSkipRun)
{
    struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
    /* Flush buffer at end of slice/picture */
    if (last_ctb)
    {
        if(vcenc_instance->codecFormat==VCENC_VIDEO_CODEC_H264 && vcenc_instance->asic.regs.entropy_coding_mode_flag == ENCHW_NO)
        {
            put_bit_ue(&slice->cabac.b, *mbSkipRun);
            *mbSkipRun = 0;
        }
        else
            sw_skip_cabac_terminate(&slice->cabac, 1);
        rbsp_trailing_bits(&slice->cabac.b);
    }
    else  /* end_of_slice_flag, there is more data... */
    {
        if(!(vcenc_instance->codecFormat==VCENC_VIDEO_CODEC_H264 && vcenc_instance->asic.regs.entropy_coding_mode_flag == ENCHW_NO))
        {
            sw_skip_cabac_terminate(&slice->cabac, 0);
        }
    }
}

#define SW_SKIP_CU_SPLIT      0x0001  /* 0000 0000 0000 0001 */
#define SW_SKIP_CU_PARTIAL    0x0002  /* 0000 0000 0000 0010 */
#define SW_SKIP_CU_OUT        0x0004  /* 0000 0000 0000 0100 */
#define SW_SKIP_CU_SKIP       0x0200  /* 0000 0010 0000 0000 */

#define SW_SKIP_IS_CU_SKIP(x)     ((x) & SW_SKIP_CU_SKIP)
#define SW_SKIP_IS_CU_SPLIT(x)      ((x) & (SW_SKIP_CU_SPLIT | SW_SKIP_CU_PARTIAL))
#define SW_SKIP_IS_CU_PARTIAL(x)    ((x) & SW_SKIP_CU_PARTIAL)
#define SW_SKIP_IS_CU_OUT(x)      ((x) & SW_SKIP_CU_OUT)

#define SW_SKIP_SET_CU_OUT(x)     ((x) |= SW_SKIP_CU_OUT)
#define SW_SKIP_SET_CU_PARTIAL(x)   ((x) |= SW_SKIP_CU_PARTIAL)
#define SW_SKIP_UNSET_CU_SPLIT(x)   ((x) &= (~SW_SKIP_CU_SPLIT))
#define SW_SKIP_SET_CU_SKIP(x)      ((x) |= SW_SKIP_CU_SKIP)

struct sw_skip_cu
{
    i32 flags;
    i32 x;
    i32 y;
    i32 log2_size;
    struct sw_skip_cu *leaf[4];
};
static i32 sw_skip_above_neighbor_exist(struct sw_skip_cu *cu);
static i32 sw_skip_left_neighbor_exist(struct sw_skip_cu *cu);
static void sw_skip_split_cu_flag(struct cabac *cabac, struct sw_skip_cu *cu);
static void sw_skip_skip_flag(struct cabac *cabac, struct sw_skip_cu *cu);
static void sw_skip_merge_idx(struct cabac *cabac, struct sw_skip_cu *cu);
static i32 sw_skip_ctu_coding(VCEncInst inst, struct cabac *cabac, i32 log2_size, i32 x, i32 y);
static void sw_skip_copy_ref(VCEncInst inst, struct sw_picture *pic, VCEncExtParaIn *ext_para);
static i32 sw_skip_above_neighbor_exist(struct sw_skip_cu *cu)
{
    if(cu->y > 0)
        return 1;
    else
        return 0;
}
static i32 sw_skip_left_neighbor_exist(struct sw_skip_cu *cu)
{
    if(cu->x > 0)
        return 1;
    else
        return 0;
}

static void sw_skip_split_cu_flag(struct cabac *cabac, struct sw_skip_cu *cu)
{
    i32 ctx_idx = cabac->split_cu_flag;
    sw_skip_cabac(cabac, ctx_idx, SW_SKIP_IS_CU_SPLIT(cu->flags) ? 1 : 0);
}

static void sw_skip_skip_flag(struct cabac *cabac, struct sw_skip_cu *cu)
{
    i32 ctx_idx = cabac->skip_flag;
    if(sw_skip_above_neighbor_exist(cu))
        ctx_idx++;
    if(sw_skip_left_neighbor_exist(cu))
        ctx_idx++;
    sw_skip_cabac(cabac, ctx_idx, SW_SKIP_IS_CU_SKIP(cu->flags) ? 1 : 0);
}

static void sw_skip_merge_idx(struct cabac *cabac, struct sw_skip_cu *cu)
{
    i32 ctx_idx = 21;
    i32 merge_idx = 0;
    sw_skip_cabac(cabac, ctx_idx, merge_idx);
}

static i32 sw_skip_ctu_coding(VCEncInst inst, struct cabac *cabac, i32 log2_size, i32 x, i32 y)
{
    struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
    i32 log2_min_cb_size = vcenc_instance->asic.regs.minCbSize;//3
    struct sw_skip_cu *new_cu = NULL;
    i32 x1, y1;
    i32 root_cu = true;

    if (!(new_cu = malloc(sizeof(struct sw_skip_cu)))) return NOK;
    memset(new_cu, 0, sizeof(struct sw_skip_cu));

    new_cu->x = x;
    new_cu->y = y;
    new_cu->log2_size = log2_size;

    if (x >= vcenc_instance->width || y >= vcenc_instance->height)//cu is out
    {
        SW_SKIP_SET_CU_OUT(new_cu->flags);
        free(new_cu);
        return OK;
    }
    if ((x+(1<<new_cu->log2_size) > vcenc_instance->width) || (y+(1<<new_cu->log2_size) > vcenc_instance->height))//cu is partial
        SW_SKIP_SET_CU_PARTIAL(new_cu->flags);

    if ((log2_size > log2_min_cb_size) && SW_SKIP_IS_CU_PARTIAL(new_cu->flags))
    {
        root_cu = false;
        log2_size--;
        x1 = x + (1 << log2_size);
        y1 = y + (1 << log2_size);
        if (sw_skip_ctu_coding(inst, cabac, log2_size, x,  y))
            goto error;
        if (sw_skip_ctu_coding(inst, cabac, log2_size, x1, y))
            goto error;
        if (sw_skip_ctu_coding(inst, cabac, log2_size, x,  y1))
            goto error;
        if (sw_skip_ctu_coding(inst, cabac, log2_size, x1, y1))
            goto error;
    }

    if (root_cu)
    {
        if (SW_SKIP_IS_CU_PARTIAL(new_cu->flags))
        {
            printf("width and height must all allign to 8.");
            goto error;
        }
        SW_SKIP_UNSET_CU_SPLIT(new_cu->flags);
        SW_SKIP_SET_CU_SKIP(new_cu->flags);
        sw_skip_split_cu_flag(cabac, new_cu);
        sw_skip_skip_flag(cabac, new_cu);
        sw_skip_merge_idx(cabac, NULL);
    }
    if(new_cu)
        free(new_cu);
    return OK;
error:
    if(new_cu)
        free(new_cu);
    return NOK;
}

static void sw_skip_copy_ref(VCEncInst inst, struct sw_picture *pic, VCEncExtParaIn *ext_para)
{
    struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
    asicData_s *asic = &vcenc_instance->asic;
    regValues_s *reg = &vcenc_instance->asic.regs;

    if(vcenc_instance->codecFormat == VCENC_VIDEO_CODEC_H264)
    {
        u8 *col_mb_va;
        i32 mb_size = 16;
        i32 mb_per_picture = ((pic->sps->width + mb_size - 1) / mb_size) * ((pic->sps->height + mb_size - 1) / mb_size);
        u32 col_mb_size = (mb_per_picture+1)/2*sizeof(struct h264_mb_col);

        if(ext_para==NULL) {
            col_mb_va = (u8 *)asic->colBuffer[pic->recon.id].virtualAddress;
        } else {
            col_mb_va = (u8 *)ext_para->recon.colBufferH264Recon_va;
        }
        memset(col_mb_va, 0xff, col_mb_size);
    }

    ASSERT(vcenc_instance->asic.regs.frameCodingType != 1);

    //copy ref to recon, no need to copy 4n.
    if (( vcenc_instance->asic.regs.frameCodingType == 0 )||  /* P */
        ( vcenc_instance->asic.regs.frameCodingType == 2 ))   /* B */
    {
        u8 *rec_y, *rec_y_4n, *rec_c, *ref_y, *ref_y_4n, *ref_c;
        u8 *rec_y_tbl=NULL, *rec_c_tbl=NULL, *ref_y_tbl=NULL, *ref_c_tbl=NULL;
        i32 y_tbl_size = 0, c_tbl_size=0, y_4n_size = 0;
        i32 width = ((pic->sps->width + 63) >> 6) << 6;
        i32 height = ((pic->sps->height + 63) >> 6) << 6;

        y_4n_size = (width / 4)*(height / 4);

        if (pic->recon_compress.lumaCompressed) {
            y_tbl_size = (width / 64) * (height / 64) * 8;
            y_tbl_size = ((y_tbl_size + 15) >> 4) << 4;
        }
        if ((pic->recon_compress.chromaCompressed) && (pic->sps->chroma_format_idc > 0))
        {
            int cbs_w = ((width >> 1) + 7) / 8;
            int cbs_h = ((height >> 1) + 3) / 4;
            int cbsg_w = (cbs_w + 15) / 16;
            c_tbl_size = cbsg_w * cbs_h * 16;
        }

        if(ext_para==NULL)
        {
            rec_y = (u8 *)asic->internalreconLuma[pic->recon.id].virtualAddress;
            rec_y_4n = (u8 *)asic->internalreconLuma_4n[pic->recon.id].virtualAddress;
            rec_c = (u8 *)asic->internalreconChroma[pic->recon.id].virtualAddress;
            ref_y = (u8 *)asic->internalreconLuma[pic->rpl[0][0]->recon.id].virtualAddress;
            ref_y_4n = (u8 *)asic->internalreconLuma_4n[pic->rpl[0][0]->recon.id].virtualAddress;
            ref_c = (u8 *)asic->internalreconChroma[pic->rpl[0][0]->recon.id].virtualAddress;

            /* assume recon and reference have same compression setup */
            if (pic->recon_compress.lumaCompressed) {
              rec_y_tbl = (u8 *)(asic->compressTbl[pic->recon.id].virtualAddress);
              ref_y_tbl = (u8 *)(asic->compressTbl[pic->rpl[0][0]->recon.id].virtualAddress);
            }

            if ((pic->recon_compress.chromaCompressed) && (pic->sps->chroma_format_idc > 0))
            {
              ASSERT(rec_y_tbl);
              rec_c_tbl = rec_y_tbl + y_tbl_size;
              ref_c_tbl = ref_y_tbl + y_tbl_size;
            }
        }
        else
        {
            rec_y = (u8 *)ext_para->recon.recon_luma_va;
            rec_y_4n = (u8 *)ext_para->recon.reconLuma_4n_va;
            rec_c = (u8 *)ext_para->recon.recon_chroma_va;
            ref_y = (u8 *)ext_para->reflist0[0].recon_luma_va;
            ref_y_4n = (u8 *)ext_para->reflist0[0].reconLuma_4n_va;
            ref_c = (u8 *)ext_para->reflist0[0].recon_chroma_va;

            /* assume recon and reference have same compression setup */
            if (pic->recon_compress.lumaCompressed) {
              rec_y_tbl = (u8 *)(ext_para->recon.compressTblReconLuma_va);
              ref_y_tbl = (u8 *)(ext_para->reflist0[0].compressTblReconLuma_va);
            }
            if ((pic->recon_compress.chromaCompressed) && (pic->sps->chroma_format_idc > 0))
            {
              ASSERT(rec_y_tbl);
              rec_c_tbl = (u8 *)(ext_para->recon.compressTblReconChroma_va);
              ref_c_tbl =(u8 *)(ext_para->reflist0[0].compressTblReconChroma_va);
            }
        }

        memcpy(rec_y, ref_y, asic->regs.ref_frame_stride * pic->recon.lum_height/4 );
        memcpy(rec_y_4n, ref_y_4n, y_4n_size );
        memcpy(rec_c, ref_c, reg->recon_chroma_half_size*2);

        if (y_tbl_size && rec_y_tbl != NULL && ref_y_tbl != NULL) {
             memcpy(rec_y_tbl, ref_y_tbl, y_tbl_size );
        }
        if (c_tbl_size && rec_c_tbl != NULL && ref_c_tbl != NULL) {
             memcpy(rec_c_tbl, ref_c_tbl, c_tbl_size );
        }
    }
}

void sw_skip_frame(VCEncInst inst, void *sw_pic, VCEncExtParaIn *ext_para)
{
    struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
    struct sw_picture *pic = (struct sw_picture *)sw_pic;
    struct slice slice;
    memset(&slice, 0, sizeof(struct slice));
    i32 ctb_num=0, column=0, row=0, x=0, y=0;
    i32 log2_ctu_size = (vcenc_instance->codecFormat==VCENC_VIDEO_CODEC_H264 ? 4 : 6);
    u32 ctu_size = (1<<log2_ctu_size);
    i32 ctb_per_row = ((vcenc_instance->width + ctu_size - 1) / ctu_size);
    i32 ctb_per_column = ((vcenc_instance->height + ctu_size - 1) / ctu_size);
    i32 ctb_per_picture = ctb_per_row * ctb_per_column;
    i32 mbSkipRun = 0;

    /*init slice header*/
    {
        slice.deblocking_filter_disabled_flag = pic->pps->deblocking_filter_disabled_flag;//ori 1
        slice.loop_filter_across_slices_enabled_flag = 1;//pic->pps->loop_filter_across_slices_enabled_flag//ori 0;
        slice.cabac_init_flag = vcenc_instance->asic.regs.cabac_init_flag;
        slice.deblocking_filter_override_flag = vcenc_instance->asic.regs.slice_deblocking_filter_override_flag;
        slice.tc_offset = vcenc_instance->asic.regs.tc_Offset;
        slice.beta_offset = vcenc_instance->asic.regs.beta_Offset;
        slice.num_long_term_pics = vcenc_instance->asic.regs.num_long_term_pics;
        slice.prev_qp = vcenc_instance->asic.regs.qp;
        if (vcenc_instance->asic.regs.saoEnable)
        {
            slice.sao_luma_flag = 0;
            slice.sao_chroma_flag = 0;
        }

        if (vcenc_instance->codecFormat==VCENC_VIDEO_CODEC_H264)
        {
            slice.cb_qp_offset = vcenc_instance->asic.regs.cbQpOffset;
            slice.cr_qp_offset = vcenc_instance->asic.regs.crQpOffset;
            slice.deblocking_filter_override_flag = ENCHW_YES;//ori none
            slice.sao_luma_flag = false;
            slice.sao_chroma_flag = false;
        }
        else
        {
            slice.nal_unit.type = TRAIL_R;
            slice.nal_unit.temporal_id = 0;
            if( vcenc_instance->asic.regs.frameCodingType == 2 )//B
                slice.type = P_SLICE; /* use P slice as skip instead of B_SLICE */
            else if( vcenc_instance->asic.regs.frameCodingType == 0 )//P
                slice.type = P_SLICE;
            else if( vcenc_instance->asic.regs.frameCodingType == 1 ){//I
                ASSERT(0);
            }
            slice.active_override_flag = vcenc_instance->asic.regs.active_override_flag;
            slice.max_num_merge_cand = 3;
        }
    }

    if(vcenc_instance->codecFormat==VCENC_VIDEO_CODEC_H264)
        sw_skip_slice_h264(inst, pic, &slice, 1);
    else
        sw_skip_slice_hevc(inst, pic, &slice, 1);

    if(vcenc_instance->codecFormat==VCENC_VIDEO_CODEC_H264)
        sw_skip_cabac_init_h264(&slice.cabac, 0, slice.prev_qp);
    else
        sw_skip_cabac_init_hevc(&slice.cabac, slice.type, slice.cabac_init_flag, slice.prev_qp);

    do
    {
        if (column == ctb_per_row)
        {
            column = 0;
            row++;
        }
        x = column * 64;
        y = row * 64;

        if(vcenc_instance->codecFormat==VCENC_VIDEO_CODEC_H264)
        {
            if(vcenc_instance->asic.regs.entropy_coding_mode_flag == ENCHW_NO)
                mbSkipRun++;
            else
            {
                if( vcenc_instance->asic.regs.frameCodingType == 0 )//P
                    sw_skip_cabac(&slice.cabac, 11, 1);
                else if( vcenc_instance->asic.regs.frameCodingType == 2 )//B
                    sw_skip_cabac(&slice.cabac, 24, 1);
            }
        }
        else
        {
            sw_skip_ctu_coding(inst, &slice.cabac, log2_ctu_size, x, y);
        }

        column++;
        ctb_num++;
        sw_skip_end_of_slice (inst, (ctb_num == ctb_per_picture), &slice,  &mbSkipRun);
    }
    while (ctb_num < ctb_per_picture);

    sw_skip_copy_ref(inst, pic, ext_para);

}

