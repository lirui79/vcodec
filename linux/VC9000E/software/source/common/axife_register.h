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
/* { SWREG, BITS, POSITION, WRITABLE } */
    /* AXI_RD_CHN_NUM                 */ {/*swreg*/ 0, 7, 0, 0},
    /* AXI_WR_CHN_NUM                 */ {/*swreg*/ 0, 7, 7, 0},
    /* AXI_RD_BURST_LENGTH            */ {/*swreg*/ 0, 5, 14, 0},
    /* AXI_WR_BURST_LENGTH            */ {/*swreg*/ 0, 5, 22, 0},
    /* AXI_WMAF_ERRESP_M_R            */ {/*swreg*/ 0, 1, 28, 0},
    /* AXI_WMAF_ERRESP_M_B            */ {/*swreg*/ 0, 1, 29, 0},
    /* AXI_BUS_IDLE_S                 */ {/*swreg*/ 0, 1, 30, 0},
    /* AXI_BUS_IDLE_M                 */ {/*swreg*/ 0, 1, 31, 0},
    /* AXI_SW_AXI_R_LEN_CNT           */ {/*swreg*/ 1, 32, 0, 0},
    /* AXI_SW_AXI_R_DAT_CNT           */ {/*swreg*/ 2, 32, 0, 0},
    /* AXI_SW_AXI_R_REQ_CNT           */ {/*swreg*/ 3, 32, 0, 0},
    /* AXI_SW_AXI_RLAST_CNT           */ {/*swreg*/ 4, 32, 0, 0},
    /* AXI_SW_AXI_W_LEN_CNT           */ {/*swreg*/ 5, 32, 0, 0},
    /* AXI_SW_AXI_W_DAT_CNT           */ {/*swreg*/ 6, 32, 0, 0},
    /* AXI_SW_AXI_W_REG_CNT           */ {/*swreg*/ 7, 32, 0, 0},
    /* AXI_SW_AXI_WLAST_CNT           */ {/*swreg*/ 8, 32, 0, 0},
    /* AXI_SW_AXI_W_ACK_CNT           */ {/*swreg*/ 9, 32, 0, 0},
    /* AXI_SW_SOFT_RESET              */ {/*swreg*/ 10, 1, 0, 1},
    /* AXI_SW_FRONTEND_EN             */ {/*swreg*/ 10, 1, 1, 1},
    /* AXI_SW_SECURE_MODE             */ {/*swreg*/ 11, 1, 0, 1},
    /* AXI_SW_AXI_USER_MODE           */ {/*swreg*/ 11, 2, 1, 1},
    /* AXI_SW_AXI_ADDR_MODE           */ {/*swreg*/ 11, 2, 3, 1},
    /* AXI_SW_AXI_PROT_MODE           */ {/*swreg*/ 11, 1, 5, 1},
    /* AXI_SW_WORK_MODE               */ {/*swreg*/ 11, 1, 6, 1},
    /* AXI_SW_SINGLE_ID_EN            */ {/*swreg*/ 11, 1, 7, 1},
    /* AXI_SW_RESET_REG_EN            */ {/*swreg*/ 11, 1, 8, 1},
    /* AXI_SW_MASTER_BL_RD            */ {/*swreg*/ 11, 5, 12, 1},
    /* AXI_SW_MASTER_BL_WR            */ {/*swreg*/ 11, 5, 17, 1},
    /* AXI_SW_AXI_RD_ID               */ {/*swreg*/ 12, 8, 0, 1},
    /* AXI_SW_PASS_MAX_BL_RD          */ {/*swreg*/ 12, 8, 8, 1},
    /* AXI_SW_AXI_WR_ID               */ {/*swreg*/ 12, 8, 16,1},
    /* AXI_SW_PASS_MAX_BL_WR          */ {/*swreg*/ 12, 8, 24,1},
    /* AXI_SW_AXI_REM_RUSER           */ {/*swreg*/ 13, 8, 0, 1},
    /* AXI_SW_AXI_REM_RNS             */ {/*swreg*/ 13, 1, 8, 1},
    /* AXI_SW_AXI_REM_RQOS            */ {/*swreg*/ 13, 4, 9, 1},
    /* AXI_SW_AXI_REM_WUSER           */ {/*swreg*/ 14, 8, 0, 1},
    /* AXI_SW_AXI_REM_WNS             */ {/*swreg*/ 14, 1, 8, 1},
    /* AXI_SW_AXI_REM_WQOS            */ {/*swreg*/ 14, 4, 9, 1},
    /* AXI_SW_AXI_RD_MAXOST           */ {/*swreg*/ 15, 10, 0, 1},
    /* AXI_SW_AXI_WR_MAXOST           */ {/*swreg*/ 15, 10, 10, 1},

#ifdef AXIFE_V_3_0
    /* NSAID_CORE_PROTECT_VAL         */ {/*swreg*/  16,  4, 0, 1},
    /* NSAID_CORE_PUBLIC_VAL          */ {/*swreg*/  16,  4, 8, 1},
    /* NSAID_META_PROTECT_VAL         */ {/*swreg*/  17,  4, 0, 1},
    /* NSAID_META_PUBLIC_VAL          */ {/*swreg*/  17,  4, 8, 1},
    /* NSAID_DECTS_PROTECT_VAL        */ {/*swreg*/  18,  4, 0, 1},
    /* NSAID_DECTS_PUBLIC_VAL         */ {/*swreg*/  18,  4, 0, 1},
    /* NSAID_SEL_CORE                 */ {/*swreg*/  19,  1, 0, 1},
    /* NSAID_SEL_META                 */ {/*swreg*/  19,  1, 1, 1},
    /* NSAID_SEL_DECTS                */ {/*swreg*/  19,  1, 2, 1},

    /* NSAID_INT_ILLEGAL_CORE         */ {/*swreg*/  20, 1, 0, 1},
    /* NSAID_INT_ILLEGAL_META         */ {/*swreg*/  20, 1, 1, 1},
    /* NSAID_INT_ILLEGAL_DECTS        */ {/*swreg*/  20, 1, 2, 1},
    /* NSAID_INT_ILLEGAL_SEL          */ {/*swreg*/  20, 1, 3, 1},
#else
    /* NSAID_CORE_PROTECT_VAL         */ {/*swreg*/  29,  4, 0, 1},
    /* NSAID_CORE_PUBLIC_VAL          */ {/*swreg*/  29,  4, 8, 1},
    /* NSAID_META_PROTECT_VAL         */ {/*swreg*/  30,  4, 0, 1},
    /* NSAID_META_PUBLIC_VAL          */ {/*swreg*/  30,  4, 8, 1},
    /* NSAID_DECTS_PROTECT_VAL        */ {/*swreg*/  31,  4, 0, 1},
    /* NSAID_DECTS_PUBLIC_VAL         */ {/*swreg*/  31,  4, 0, 1},
    /* NSAID_SEL_CORE                 */ {/*swreg*/  32,  1, 0, 1},
    /* NSAID_SEL_META                 */ {/*swreg*/  32,  1, 1, 1},
    /* NSAID_SEL_DECTS                */ {/*swreg*/  32,  1, 2, 1},

    /* NSAID_INT_ILLEGAL_CORE         */ {/*swreg*/  33,  1, 0, 1},
    /* NSAID_INT_ILLEGAL_META         */ {/*swreg*/  33,  1, 1, 1},
    /* NSAID_INT_ILLEGAL_DECTS        */ {/*swreg*/  33,  1, 2, 1},
    /* NSAID_INT_ILLEGAL_SEL          */ {/*swreg*/  33,  1, 3, 1},
    /* AXI_SW_MID_SEL                 */ {/*swreg*/  29,  1, 0, 1},
    /* AXI_SW_IDLE_LAT                */ {/*swreg*/  34,  8, 0, 1},
    /* AXI_SW_FLUSH_EN                */ {/*swreg*/  35,  1, 0, 1},
#endif
