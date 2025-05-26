//---
// gint:cpg:overclock - Clock speed control
//
// Most of the data in this file has been reused from Sentaro21's Ftune and
// Ptune utilities, which have long been the standard for overclocking CASIO
// calculators.
// See: http://pm.matrix.jp/ftune2e.html
//
// SlyVTT also contributed early testing on both the fx-CG 10/20 and fx-CG 50.
//---

#include <gint/clock.h>
#include <gint/gint.h>
#include <gint/hardware.h>
#include <gint/drivers.h>
#include <gint/mpu/cpg.h>
#include <gint/mpu/bsc.h>
#include <gint/mpu/wdt.h>

//---
// Low-level clock speed access
//---

#define SH7305_SDMR3_CL2 ((volatile uint16_t *)0xFEC15040)
#define SH7305_SDMR3_CL3 ((volatile uint16_t *)0xFEC15060)

//---
// Predefined clock speeds
//---

/* SH7305 CPG */
#define SH4_SELXM    1
#define SH4_FRQCR(PLL,IFC,SFC,BFC,PFC) \
    (((PLL)-1)<<24)+((IFC)<<20)+((SFC)<<12)+((BFC)<<8)+(PFC)
#define SH4_DIV_2    0
#define SH4_DIV_4    1
#define SH4_DIV_8    2
#define SH4_DIV_16   3
#define SH4_DIV_32   4

/* SH7305 BSC */
#define SH4_BCR(IWW,IWRWD,IWRWS,IWRRD,IWRRS) \
    (((IWW)<<28)+((IWRWD)<<25)+((IWRWS)<<22)+((IWRRD)<<19)+((IWRRS)<<16))
#define SH4_CS0BCR(IWW,IWRWD,IWRWS,IWRRD,IWRRS) \
    SH4_BCR(IWW,IWRWD,IWRWS,IWRRD,IWRRS)+0x0400
#define SH4_CS2BCR(IWW,IWRWD,IWRWS,IWRRD,IWRRS) \
    SH4_BCR(IWW,IWRWD,IWRWS,IWRRD,IWRRS)+0x3400
#define SH4_CS3BCR(IWW,IWRWD,IWRWS,IWRRD,IWRRS) \
    SH4_BCR(IWW,IWRWD,IWRWS,IWRRD,IWRRS)+0x4400
#define SH4_CS5ABCR_T6K11(IWW,IWRWD,IWRWS,IWRRD,IWRRS) \
    SH4_BCR(IWW,IWRWD,IWRWS,IWRRD,IWRRS)+0x0200
#define SH4_CS5ABCR_R6152x(IWW,IWRWD,IWRWS,IWRRD,IWRRS) \
    SH4_BCR(IWW,IWRWD,IWRWS,IWRRD,IWRRS)+0x0400
#define SH4_BCR_4    3
#define SH4_BCR_6    4
#define SH4_BCR_8    5
#define SH4_BCR_10   6
#define SH4_BCR_12   7

#define SH4_CSnWCR(WW,SW,WR,HW) \
    (((WW)<<16)+((SW)<<11)+((WR)<<7)+(HW)+0x0040)
#define SH4_CS3WCR(TRP,TRCD,A3CL,TRWL,TRC) \
    (((TRP)<<13)+((TRCD)<<10)+((A3CL)<<7)+((TRWL)<<3)+(TRC)+0x0040)
#define SH4_WW_EQWR  0
#define SH4_WW(n)    ((n)+1)
#define SH4_WR_8     7
#define SH4_WR_10    8
#define SH4_WR_12    9
#define SH4_WR_14    10
#define SH4_WR_18    11
#define SH4_WR_24    12
#define SH4_TRP_2    1
#define SH4_TRCD_2   1
#define SH4_A3CL_2   1
#define SH4_TRC_3    0
#define SH4_TRC_4    1
#define SH4_TRC_6    2
#define SH4_TRC_9    3

/* SH7705-like CPG */
#define SH3_PLL_1x   0
#define SH3_PLL_2x   1
#define SH3_PLL_3x   2
#define SH3_PLL_4x   3
#define SH3_DIV_1    0
#define SH3_DIV_2    1
#define SH3_DIV_3    2
#define SH3_DIV_4    3

void cpg_get_overclock_setting(struct cpg_overclock_setting *s)
{
    if(isSH3()) {
        s->FLLFRQ = -1;
        s->FRQCR = SH7705_CPG.FRQCR.word;

        s->CS0BCR = SH7705_BSC.CS0BCR.lword;
        s->CS0WCR = SH7705_BSC.CS0WCR.lword;
        s->CS2BCR = SH7705_BSC.CS2BCR.lword;
        s->CS2WCR = SH7705_BSC.CS2WCR.lword;
        s->CS3BCR = SH7705_BSC.CS3BCR.lword;
        s->CS3WCR = SH7705_BSC.CS3WCR.lword;
        s->CS5aBCR = SH7705_BSC.CS5ABCR.lword;
        s->CS5aWCR = SH7705_BSC.CS5AWCR.lword;
    }
    else {
        s->FLLFRQ = SH7305_CPG.FLLFRQ.lword;
        s->FRQCR = SH7305_CPG.FRQCR.lword;

        s->CS0BCR = SH7305_BSC.CS0BCR.lword;
        s->CS0WCR = SH7305_BSC.CS0WCR.lword;
        s->CS2BCR = SH7305_BSC.CS2BCR.lword;
        s->CS2WCR = SH7305_BSC.CS2WCR.lword;
        s->CS3BCR = SH7305_BSC.CS3BCR.lword;
        s->CS3WCR = SH7305_BSC.CS3WCR.lword;
        s->CS5aBCR = SH7305_BSC.CS5ABCR.lword;
        s->CS5aWCR = SH7305_BSC.CS5AWCR.lword;
    }
}

static void cpg_low_level_set_setting(struct cpg_overclock_setting const *s)
{
    if(isSH3()) {
        SH7705_WDT.WTCNT.WRITE = 0;
        SH7705_WDT.WTCSR.WRITE = 0x65;
        SH7705_CPG.FRQCR.word = s->FRQCR | 0x1000;
        SH7705_BSC.CS0BCR.lword = s->CS0BCR;
        SH7705_BSC.CS0WCR.lword = s->CS0WCR;
        SH7705_BSC.CS2BCR.lword = s->CS2BCR;
        SH7705_BSC.CS2WCR.lword = s->CS2WCR;
        SH7705_BSC.CS3BCR.lword = s->CS3BCR;
        SH7705_BSC.CS3WCR.lword = s->CS3WCR;
        SH7705_BSC.CS5ABCR.lword = s->CS5aBCR;
        SH7705_BSC.CS5AWCR.lword = s->CS5aWCR;
    }
    else {
        SH7305_BSC.CS0WCR.WR = SH4_WR_18;

        SH7305_CPG.FLLFRQ.lword = s->FLLFRQ;
        SH7305_CPG.FRQCR.lword = s->FRQCR;
        SH7305_CPG.FRQCR.KICK = 1;
        while(SH7305_CPG.LSTATS != 0) {}

        SH7305_BSC.CS0BCR.lword = s->CS0BCR;
        SH7305_BSC.CS0WCR.lword = s->CS0WCR;
        SH7305_BSC.CS2BCR.lword = s->CS2BCR;
        SH7305_BSC.CS2WCR.lword = s->CS2WCR;
        SH7305_BSC.CS3BCR.lword = s->CS3BCR;
        SH7305_BSC.CS3WCR.lword = s->CS3WCR;

        if(SH7305_BSC.CS3WCR.A3CL == 1)
            *SH7305_SDMR3_CL2 = 0;
        else
            *SH7305_SDMR3_CL3 = 0;

        SH7305_BSC.CS5ABCR.lword = s->CS5aBCR;
        SH7305_BSC.CS5AWCR.lword = s->CS5aWCR;
    }
}

void cpg_set_overclock_setting(struct cpg_overclock_setting const *s)
{
    uint32_t old_Pphi = clock_freq()->Pphi_f;

    /* Wait for asynchronous tasks to complete */
    gint_world_sync();

    /* Disable interrupts during the change */
    cpu_atomic_start();

    /* Load the clock settings */
    cpg_low_level_set_setting(s);

    /* Determine the change in frequency for Pϕ and recompute CPG data */
    cpg_compute_freq();
    uint32_t new_Pphi = clock_freq()->Pphi_f;

    /* Update timers' TCNT and TCOR to match the new clock speed */
    void timer_rescale(uint32_t old_Pphi, uint32_t new_Pphi);
    timer_rescale(old_Pphi, new_Pphi);

    cpu_atomic_end();
}

void cpg_set_overclock_permanent(bool permanent)
{
    extern gint_driver_t drv_cpg;
    int i = &drv_cpg - gint_drivers;

    if(permanent)
        gint_driver_flags[i] |= GINT_DRV_SHARED;
    else
        gint_driver_flags[i] &= ~GINT_DRV_SHARED;
}

#if GINT_HW_FX

static struct cpg_overclock_setting const settings_fx9860g_sh3[5] = {
    /* CLOCK_SPEED_F1 */
    { .FRQCR   = 0x1001,
      .CS0BCR  = 0x02480400,
      .CS2BCR  = 0x02483400,
      .CS3BCR  = 0x36DB0600,
      .CS5aBCR = 0x224A0200,
      .CS0WCR  = 0x00000140,
      .CS2WCR  = 0x00000140,
      .CS3WCR  = 0x00000500,
      .CS5aWCR = 0x00000D41 },
    /* CLOCK_SPEED_F2 */
    { .FRQCR   = (SH3_PLL_2x<<8)+(SH3_DIV_1<<4)+SH3_DIV_2,
      .CS0BCR  = 0x02480400,
      .CS2BCR  = 0x02483400,
      .CS3BCR  = 0x36DB0600,
      .CS5aBCR = 0x224A0200,
      .CS0WCR  = 0x00000140,
      .CS2WCR  = 0x00000140,
      .CS3WCR  = 0x00000500,
      .CS5aWCR = 0x00000D41 },
    /* CLOCK_SPEED_F3 */
    { .FRQCR   = (SH3_PLL_3x<<8)+(SH3_DIV_1<<4)+SH3_DIV_3,
      .CS0BCR  = 0x02480400,
      .CS2BCR  = 0x02483400,
      .CS3BCR  = 0x36DB0600,
      .CS5aBCR = 0x224A0200,
      .CS0WCR  = 0x00000140,
      .CS2WCR  = 0x00000140,
      .CS3WCR  = 0x00000500,
      .CS5aWCR = 0x00000D41 },
    /* CLOCK_SPEED_F4 */
    { .FRQCR   = (SH3_PLL_4x<<8)+(SH3_DIV_1<<4)+SH3_DIV_4,
      .CS0BCR  = 0x02480400,
      .CS2BCR  = 0x02483400,
      .CS3BCR  = 0x36DB0600,
      .CS5aBCR = 0x224A0200,
      .CS0WCR  = 0x00000140,
      .CS2WCR  = 0x00000140,
      .CS3WCR  = 0x00000500,
      .CS5aWCR = 0x00000D41 },
    /* CLOCK_SPEED_F5 */
    { .FRQCR   = (SH3_PLL_4x<<8)+(SH3_DIV_1<<4)+SH3_DIV_4,
      .CS0BCR  = 0x02480400,
      .CS2BCR  = 0x02483400,
      .CS3BCR  = 0x36DB0600,
      .CS5aBCR = 0x224A0200,
      .CS0WCR  = 0x000000C0,
      .CS2WCR  = 0x000100C0,
      .CS3WCR  = 0x00000500,
      .CS5aWCR = 0x00000D41 },
};

static struct cpg_overclock_setting const settings_fx9860g_sh4[5] = {
    /* CLOCK_SPEED_F1 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = 0x0F202203,
      .CS0BCR  = 0x24920400,
      .CS2BCR  = 0x24923400,
      .CS3BCR  = 0x24924400,  /* Don't care */
      .CS5aBCR = 0x224A0200,
      .CS0WCR  = 0x000005C0,
      .CS2WCR  = 0x00000140,
      .CS3WCR  = 0x000024D0,
      .CS5aWCR = 0x00000D41 },
    /* CLOCK_SPEED_F2 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = SH4_FRQCR(16,SH4_DIV_4,SH4_DIV_8,SH4_DIV_8,SH4_DIV_16),
      .CS0BCR  = SH4_CS0BCR(2,2,2,2,2),
      .CS2BCR  = SH4_CS2BCR(2,2,2,2,2),
      .CS3BCR  = 0x24924400,
      .CS5aBCR = SH4_CS5ABCR_T6K11(2,1,1,1,2),
      .CS0WCR  = SH4_CSnWCR(SH4_WW_EQWR,0,3,0),
      .CS2WCR  = SH4_CSnWCR(SH4_WW_EQWR,0,2,0),
      .CS3WCR  = SH4_CS3WCR(SH4_TRP_2,SH4_TRCD_2,SH4_A3CL_2,2,SH4_TRC_3),
      .CS5aWCR = SH4_CSnWCR(SH4_WW_EQWR,1,SH4_WR_14,1) },
    /* CLOCK_SPEED_F3 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = SH4_FRQCR(16,SH4_DIV_8,SH4_DIV_8,SH4_DIV_8,SH4_DIV_16),
      .CS0BCR  = SH4_CS0BCR(0,2,2,2,0),
      .CS2BCR  = SH4_CS2BCR(0,2,2,2,0),
      .CS3BCR  = 0x24924400,
      .CS5aBCR = SH4_CS5ABCR_T6K11(2,2,2,2,2),
      .CS0WCR  = SH4_CSnWCR(SH4_WW_EQWR,0,2,0),
      .CS2WCR  = SH4_CSnWCR(SH4_WW_EQWR,0,2,0),
      .CS3WCR  = SH4_CS3WCR(SH4_TRP_2,SH4_TRCD_2,SH4_A3CL_2,2,SH4_TRC_3),
      .CS5aWCR = SH4_CSnWCR(SH4_WW_EQWR,1,SH4_WR_14,1) },
    /* CLOCK_SPEED_F4 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = SH4_FRQCR(32,SH4_DIV_4,SH4_DIV_8,SH4_DIV_8,SH4_DIV_16),
      .CS0BCR  = SH4_CS0BCR(0,2,2,2,0),
      .CS2BCR  = SH4_CS2BCR(0,2,2,2,0),
      .CS3BCR  = 0x24924400,
      .CS5aBCR = SH4_CS5ABCR_T6K11(2,1,1,1,2),
      .CS0WCR  = SH4_CSnWCR(SH4_WW_EQWR,0,3,0),
      .CS2WCR  = SH4_CSnWCR(SH4_WW(1),0,2,0),
      .CS3WCR  = SH4_CS3WCR(SH4_TRP_2,SH4_TRCD_2,SH4_A3CL_2,2,SH4_TRC_3),
      .CS5aWCR = SH4_CSnWCR(SH4_WW_EQWR,1,SH4_WR_14,1) },
    /* CLOCK_SPEED_F5 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = SH4_FRQCR(32,SH4_DIV_2,SH4_DIV_4,SH4_DIV_4,SH4_DIV_16),
      .CS0BCR  = SH4_CS0BCR(1,2,2,2,0),
      .CS2BCR  = SH4_CS2BCR(0,2,2,2,0),
      .CS3BCR  = 0x24924400,
      .CS5aBCR = SH4_CS5ABCR_T6K11(2,1,1,1,2),
      .CS0WCR  = SH4_CSnWCR(SH4_WW_EQWR,0,SH4_WR_8,0),
      .CS2WCR  = SH4_CSnWCR(SH4_WW(2),0,5,0),
      .CS3WCR  = SH4_CS3WCR(SH4_TRP_2,SH4_TRCD_2,SH4_A3CL_2,2,SH4_TRC_3),
      .CS5aWCR = SH4_CSnWCR(SH4_WW_EQWR,1,SH4_WR_14,1) },
};

static struct cpg_overclock_setting const settings_g35pe2[5] = {
    /* CLOCK_SPEED_F1 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = 0x0F112213,
      .CS0BCR  = 0x24920400,
      .CS2BCR  = 0x24923400,
      .CS3BCR  = 0x24924400,  /* Don't care */
      .CS5aBCR = 0x224A0200,
      .CS0WCR  = 0x000001C0,
      .CS2WCR  = 0x000001C0,
      .CS3WCR  = 0x000024D0,
      .CS5aWCR = 0x00031340 },
    /* CLOCK_SPEED_F2 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = SH4_FRQCR(16,SH4_DIV_4,SH4_DIV_8,SH4_DIV_8,SH4_DIV_16),
      .CS0BCR  = SH4_CS0BCR(2,2,2,2,2),
      .CS2BCR  = SH4_CS2BCR(2,2,2,2,2),
      .CS3BCR  = 0x24924400,
      .CS5aBCR = SH4_CS5ABCR_T6K11(2,1,1,1,2),
      .CS0WCR  = SH4_CSnWCR(SH4_WW_EQWR,0,3,0),
      .CS2WCR  = SH4_CSnWCR(SH4_WW_EQWR,0,2,0),
      .CS3WCR  = SH4_CS3WCR(SH4_TRP_2,SH4_TRCD_2,SH4_A3CL_2,2,SH4_TRC_3),
      .CS5aWCR = SH4_CSnWCR(SH4_WW_EQWR,1,SH4_WR_14,1) },
    /* CLOCK_SPEED_F3 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = SH4_FRQCR(16,SH4_DIV_8,SH4_DIV_8,SH4_DIV_8,SH4_DIV_16),
      .CS0BCR  = SH4_CS0BCR(0,2,2,2,0),
      .CS2BCR  = SH4_CS2BCR(0,2,2,2,0),
      .CS3BCR  = 0x24924400,
      .CS5aBCR = SH4_CS5ABCR_T6K11(2,2,2,2,2),
      .CS0WCR  = SH4_CSnWCR(SH4_WW_EQWR,0,2,0),
      .CS2WCR  = SH4_CSnWCR(SH4_WW_EQWR,0,2,0),
      .CS3WCR  = SH4_CS3WCR(SH4_TRP_2,SH4_TRCD_2,SH4_A3CL_2,2,SH4_TRC_3),
      .CS5aWCR = SH4_CSnWCR(SH4_WW_EQWR,1,SH4_WR_14,1) },
    /* CLOCK_SPEED_F4 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = SH4_FRQCR(32,SH4_DIV_4,SH4_DIV_8,SH4_DIV_8,SH4_DIV_16),
      .CS0BCR  = SH4_CS0BCR(0,2,2,2,0),
      .CS2BCR  = SH4_CS2BCR(0,2,2,2,0),
      .CS3BCR  = 0x24924400,
      .CS5aBCR = SH4_CS5ABCR_T6K11(2,1,1,1,2),
      .CS0WCR  = SH4_CSnWCR(SH4_WW_EQWR,0,3,0),
      .CS2WCR  = SH4_CSnWCR(SH4_WW(1),0,2,0),
      .CS3WCR  = SH4_CS3WCR(SH4_TRP_2,SH4_TRCD_2,SH4_A3CL_2,2,SH4_TRC_3),
      .CS5aWCR = SH4_CSnWCR(SH4_WW(2),3,6,0) },
    /* CLOCK_SPEED_F5 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = SH4_FRQCR(32,SH4_DIV_2,SH4_DIV_4,SH4_DIV_8,SH4_DIV_16),
      .CS0BCR  = SH4_CS0BCR(1,2,2,2,0),
      .CS2BCR  = SH4_CS2BCR(0,2,2,2,0),
      .CS3BCR  = 0x24924400,
      .CS5aBCR = SH4_CS5ABCR_T6K11(2,1,1,1,2),
      .CS0WCR  = SH4_CSnWCR(SH4_WW_EQWR,0,3,0),
      .CS2WCR  = SH4_CSnWCR(SH4_WW(1),0,2,0),
      .CS3WCR  = SH4_CS3WCR(SH4_TRP_2,SH4_TRCD_2,SH4_A3CL_2,2,SH4_TRC_3),
      .CS5aWCR = SH4_CSnWCR(SH4_WW(2),3,6,0) },
};

#endif

#if GINT_HW_CG

static struct cpg_overclock_setting const settings_prizm[5] = {
    /* CLOCK_SPEED_F1 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = 0x0F102203,
      .CS0BCR  = 0x24920400,
      .CS2BCR  = 0x24923400,
      .CS3BCR  = 0x24924400,  /* Don't care */
      .CS5aBCR = 0x15140400,
      .CS0WCR  = 0x000001C0,
      .CS2WCR  = 0x00000140,
      .CS3WCR  = 0x000024D0,
      .CS5aWCR = 0x00010240 },
    /* CLOCK_SPEED_F2 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = SH4_FRQCR(16,SH4_DIV_4,SH4_DIV_8,SH4_DIV_8,SH4_DIV_16),
      .CS0BCR  = SH4_CS0BCR(0,2,2,2,0),
      .CS2BCR  = SH4_CS2BCR(0,2,2,2,0),
      .CS3BCR  = 0x24924400,
      .CS5aBCR = SH4_CS5ABCR_R6152x(1,2,SH4_BCR_6,2,SH4_BCR_6),
      .CS0WCR  = SH4_CSnWCR(SH4_WW_EQWR,0,2,0),
      .CS2WCR  = SH4_CSnWCR(SH4_WW(0),0,1,0),
      .CS3WCR  = SH4_CS3WCR(SH4_TRP_2,SH4_TRCD_2,SH4_A3CL_2,2,SH4_TRC_3),
      .CS5aWCR = SH4_CSnWCR(SH4_WW(0),0,4,0) },
    /* CLOCK_SPEED_F3 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = SH4_FRQCR(16,SH4_DIV_2,SH4_DIV_4,SH4_DIV_4,SH4_DIV_16),
      .CS0BCR  = SH4_CS0BCR(2,2,2,2,0),
      .CS2BCR  = SH4_CS2BCR(0,2,2,2,0),
      .CS3BCR  = 0x24924400,
      .CS5aBCR = SH4_CS5ABCR_R6152x(1,2,SH4_BCR_6,2,SH4_BCR_6),
      .CS0WCR  = SH4_CSnWCR(SH4_WW_EQWR,0,5,0),
      .CS2WCR  = SH4_CSnWCR(SH4_WW(1),0,3,0),
      .CS3WCR  = SH4_CS3WCR(SH4_TRP_2,SH4_TRCD_2,SH4_A3CL_2,2,SH4_TRC_3),
      .CS5aWCR = SH4_CSnWCR(SH4_WW(0),0,4,0) },
    /* CLOCK_SPEED_F4 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = SH4_FRQCR(32,SH4_DIV_2,SH4_DIV_4,SH4_DIV_8,SH4_DIV_32),
      .CS0BCR  = SH4_CS0BCR(2,2,2,2,0),
      .CS2BCR  = SH4_CS2BCR(0,2,2,2,0),
      .CS3BCR  = 0x24924400,
      .CS5aBCR = SH4_CS5ABCR_R6152x(1,2,SH4_BCR_6,2,SH4_BCR_6),
      .CS0WCR  = SH4_CSnWCR(SH4_WW_EQWR,0,5,0),
      .CS2WCR  = SH4_CSnWCR(SH4_WW(1),0,3,0),
      .CS3WCR  = SH4_CS3WCR(SH4_TRP_2,SH4_TRCD_2,SH4_A3CL_2,2,SH4_TRC_3),
      .CS5aWCR = SH4_CSnWCR(SH4_WW(0),0,4,0) },
    /* CLOCK_SPEED_F5 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = SH4_FRQCR(32,SH4_DIV_2,SH4_DIV_4,SH4_DIV_4,SH4_DIV_32),
      .CS0BCR  = SH4_CS0BCR(SH4_BCR_6,2,2,2,0),
      .CS2BCR  = SH4_CS2BCR(0,2,2,2,0),
      .CS3BCR  = 0x24924400,
      .CS5aBCR = SH4_CS5ABCR_R6152x(1,2,SH4_BCR_6,2,SH4_BCR_6),
      .CS0WCR  = SH4_CSnWCR(SH4_WW_EQWR,0,SH4_WR_10,0),
      .CS2WCR  = SH4_CSnWCR(SH4_WW(3),0,6,0),
      .CS3WCR  = SH4_CS3WCR(SH4_TRP_2,SH4_TRCD_2,SH4_A3CL_2,2,SH4_TRC_3),
      .CS5aWCR = SH4_CSnWCR(SH4_WW(0),0,4,0) },
};

static struct cpg_overclock_setting const settings_fxcg_50_100[5] = {
    /* CLOCK_SPEED_F1 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = 0x0F011112,
      .CS0BCR  = 0x36DA0400,
      .CS2BCR  = 0x36DA3400,  /* Don't care */
      .CS3BCR  = 0x36DB4400,
      .CS5aBCR = 0x17DF0400,
      .CS0WCR  = 0x000003C0,
      .CS2WCR  = 0x000003C0,  /* Don't care */
      .CS3WCR  = 0x000024D1,
      .CS5aWCR = 0x000203C1 },
    /* CLOCK_SPEED_F2 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = SH4_FRQCR(32,SH4_DIV_8,SH4_DIV_16,SH4_DIV_16,SH4_DIV_16),
      .CS0BCR  = SH4_CS0BCR(2,2,2,2,2),
      .CS2BCR  = 0x36DA3400,
      .CS3BCR  = SH4_CS3BCR(2,2,2,2,2),
      .CS5aBCR = SH4_CS5ABCR_R6152x(1,SH4_BCR_4,SH4_BCR_12,SH4_BCR_4,SH4_BCR_12),
      .CS0WCR  = SH4_CSnWCR(SH4_WW_EQWR,0,4,0),
      .CS2WCR  = 0x000003C0,
      .CS3WCR  = SH4_CS3WCR(SH4_TRP_2,SH4_TRCD_2,SH4_A3CL_2,2,SH4_TRC_3),
      .CS5aWCR = SH4_CSnWCR(SH4_WW(0),0,4,0) },
    /* CLOCK_SPEED_F3 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = SH4_FRQCR(32,SH4_DIV_4,SH4_DIV_8,SH4_DIV_8,SH4_DIV_16),
      .CS0BCR  = SH4_CS0BCR(2,2,2,2,0),
      .CS2BCR  = 0x36DA3400,
      .CS3BCR  = SH4_CS3BCR(0,2,2,2,0),
      .CS5aBCR = SH4_CS5ABCR_R6152x(1,SH4_BCR_4,SH4_BCR_12,SH4_BCR_4,SH4_BCR_12),
      .CS0WCR  = SH4_CSnWCR(SH4_WW_EQWR,0,5,0),
      .CS2WCR  = 0x000003C0,
      .CS3WCR  = SH4_CS3WCR(SH4_TRP_2,SH4_TRCD_2,SH4_A3CL_2,2,SH4_TRC_4),
      .CS5aWCR = SH4_CSnWCR(SH4_WW(0),0,4,0) },
    /* CLOCK_SPEED_F4 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = SH4_FRQCR(32,SH4_DIV_2,SH4_DIV_4,SH4_DIV_8,SH4_DIV_16),
      .CS0BCR  = SH4_CS0BCR(2,2,2,2,0),
      .CS2BCR  = 0x36DA3400,
      .CS3BCR  = SH4_CS3BCR(0,2,2,2,0),
      .CS5aBCR = SH4_CS5ABCR_R6152x(1,SH4_BCR_4,SH4_BCR_12,SH4_BCR_4,SH4_BCR_12),
      .CS0WCR  = SH4_CSnWCR(SH4_WW_EQWR,0,5,0),
      .CS2WCR  = 0x000003C0,
      .CS3WCR  = SH4_CS3WCR(SH4_TRP_2,SH4_TRCD_2,SH4_A3CL_2,2,SH4_TRC_4),
      .CS5aWCR = SH4_CSnWCR(SH4_WW(0),0,4,0) },
    /* CLOCK_SPEED_F5 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = SH4_FRQCR(32,SH4_DIV_2,SH4_DIV_4,SH4_DIV_4,SH4_DIV_16),
      .CS0BCR  = SH4_CS0BCR(SH4_BCR_6,2,2,2,0),
      .CS2BCR  = 0x36DA3400,
      .CS3BCR  = SH4_CS3BCR(0,2,2,2,0),
      .CS5aBCR = SH4_CS5ABCR_R6152x(1,SH4_BCR_4,SH4_BCR_12,SH4_BCR_4,SH4_BCR_12),
      .CS0WCR  = SH4_CSnWCR(SH4_WW_EQWR,0,SH4_WR_12,0),
      .CS2WCR  = 0x000003C0,
      .CS3WCR  = SH4_CS3WCR(SH4_TRP_2,SH4_TRCD_2,SH4_A3CL_2,2,SH4_TRC_6),
      .CS5aWCR = SH4_CSnWCR(SH4_WW(1),0,SH4_WR_8,1) },
};

#endif

#if GINT_HW_CP

static struct cpg_overclock_setting const settings_fxcp400[5] = {
    /* CLOCK_SPEED_F1 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = 0x0F011112,
      .CS0BCR  = 0x36DA0400,
      .CS2BCR  = 0x36DA0400,  /* Don't care */
      .CS3BCR  = 0x76DB4400,
      .CS5aBCR = 0x17DF0400,
      .CS0WCR  = 0x000003C0,
      .CS2WCR  = 0x000003C0,  /* Don't care */
      .CS3WCR  = 0x000024D1,
      .CS5aWCR = 0x00020141 },
    /* CLOCK_SPEED_F2 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = SH4_FRQCR(32,SH4_DIV_4,SH4_DIV_8,SH4_DIV_8,SH4_DIV_16),
      .CS0BCR  = SH4_CS0BCR(2,2,2,2,0),
      .CS2BCR  = 0x36DA3400,
      .CS3BCR  = SH4_CS3BCR(SH4_BCR_8,2,2,2,0),
      .CS5aBCR = SH4_CS5ABCR_R6152x(1,SH4_BCR_4,SH4_BCR_12,SH4_BCR_4,SH4_BCR_12),
      .CS0WCR  = SH4_CSnWCR(SH4_WW_EQWR,0,5,0),
      .CS2WCR  = 0x000003C0,
      .CS3WCR  = SH4_CS3WCR(SH4_TRP_2,SH4_TRCD_2,SH4_A3CL_2,2,SH4_TRC_4),
      .CS5aWCR = SH4_CSnWCR(SH4_WW(1),0,2,1) },
    /* CLOCK_SPEED_F3 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = SH4_FRQCR(32,SH4_DIV_4,SH4_DIV_8,SH4_DIV_8,SH4_DIV_16),
      .CS0BCR  = SH4_CS0BCR(2,2,2,2,0),
      .CS2BCR  = 0x36DA3400,
      .CS3BCR  = SH4_CS3BCR(SH4_BCR_8,2,2,2,0),
      .CS5aBCR = SH4_CS5ABCR_R6152x(1,SH4_BCR_4,SH4_BCR_12,SH4_BCR_4,SH4_BCR_12),
      .CS0WCR  = SH4_CSnWCR(SH4_WW_EQWR,0,5,0),
      .CS2WCR  = 0x000003C0,
      .CS3WCR  = SH4_CS3WCR(SH4_TRP_2,SH4_TRCD_2,SH4_A3CL_2,2,SH4_TRC_4),
      .CS5aWCR = SH4_CSnWCR(SH4_WW(0),0,2,0) },
    /* CLOCK_SPEED_F4 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = SH4_FRQCR(32,SH4_DIV_2,SH4_DIV_4,SH4_DIV_8,SH4_DIV_16),
      .CS0BCR  = SH4_CS0BCR(2,2,2,2,0),
      .CS2BCR  = 0x36DA3400,
      .CS3BCR  = SH4_CS3BCR(SH4_BCR_8,2,2,2,0),
      .CS5aBCR = SH4_CS5ABCR_R6152x(1,SH4_BCR_4,SH4_BCR_12,SH4_BCR_4,SH4_BCR_12),
      .CS0WCR  = SH4_CSnWCR(SH4_WW_EQWR,0,5,0),
      .CS2WCR  = 0x000003C0,
      .CS3WCR  = SH4_CS3WCR(SH4_TRP_2,SH4_TRCD_2,SH4_A3CL_2,2,SH4_TRC_4),
      .CS5aWCR = SH4_CSnWCR(SH4_WW(0),0,2,0) },
    /* CLOCK_SPEED_F5 */
    { .FLLFRQ  = (SH4_SELXM<<14)+900,
      .FRQCR   = SH4_FRQCR(25,SH4_DIV_2,SH4_DIV_4,SH4_DIV_4,SH4_DIV_8),
      .CS0BCR  = SH4_CS0BCR(SH4_BCR_4,2,2,2,0),
      .CS2BCR  = 0x36DA3400,
      .CS3BCR  = SH4_CS3BCR(SH4_BCR_8,2,2,2,0),
      .CS5aBCR = SH4_CS5ABCR_R6152x(1,SH4_BCR_4,SH4_BCR_12,SH4_BCR_4,SH4_BCR_12),
      .CS0WCR  = SH4_CSnWCR(SH4_WW_EQWR,0,SH4_WR_10,0),
      .CS2WCR  = 0x000003C0,
      .CS3WCR  = SH4_CS3WCR(SH4_TRP_2,SH4_TRCD_2,SH4_A3CL_2,2,SH4_TRC_4),
      .CS5aWCR = SH4_CSnWCR(SH4_WW(0),0,2,0) },
};

#endif

static struct cpg_overclock_setting const *get_settings(void)
{
#if GINT_HW_FX
    if(gint[HWCALC] == HWCALC_FX9860G_SH3)
        return settings_fx9860g_sh3;
    if(gint[HWCALC] == HWCALC_FX9860G_SH4)
        return settings_fx9860g_sh4;
    if(gint[HWCALC] == HWCALC_G35PE2)
        return settings_g35pe2;
#endif

#if GINT_HW_CG
    if(gint[HWCALC] == HWCALC_PRIZM)
        return settings_prizm;
    if(gint[HWCALC] == HWCALC_FXCG50 || gint[HWCALC] == HWCALC_FXCG100)
        return settings_fxcg_50_100;
#endif

#if GINT_HW_CP
    if(gint[HWCALC] == HWCALC_FXCP400)
        return settings_fxcp400;
#endif

    return NULL;
}

int clock_get_speed(void)
{
    struct cpg_overclock_setting const *settings = get_settings();
    if(!settings)
        return CLOCK_SPEED_UNKNOWN;

    if(isSH3()) {
        for(int i = 0; i < 5; i++) {
            struct cpg_overclock_setting const *s = &settings[i];

            if(SH7705_CPG.FRQCR.word == (s->FRQCR | 0x1000)
                && SH7705_BSC.CS0BCR.lword == s->CS0BCR
                && SH7705_BSC.CS2BCR.lword == s->CS2BCR
                && SH7705_BSC.CS3BCR.lword == s->CS3BCR
                && SH7705_BSC.CS5ABCR.lword == s->CS5aBCR
                && SH7705_BSC.CS0WCR.lword == s->CS0WCR
                && SH7705_BSC.CS2WCR.lword == s->CS2WCR
                && SH7705_BSC.CS3WCR.lword == s->CS3WCR
                && SH7705_BSC.CS5AWCR.lword == s->CS5aWCR)
                return CLOCK_SPEED_F1 + i;
        }
    }
    else {
        for(int i = 0; i < 5; i++) {
            struct cpg_overclock_setting const *s = &settings[i];

            if(SH7305_CPG.FLLFRQ.lword == s->FLLFRQ
                && SH7305_CPG.FRQCR.lword == s->FRQCR
                && SH7305_BSC.CS0BCR.lword == s->CS0BCR
                && SH7305_BSC.CS2BCR.lword == s->CS2BCR
                && SH7305_BSC.CS3BCR.lword == s->CS3BCR
                && SH7305_BSC.CS5ABCR.lword == s->CS5aBCR
                && SH7305_BSC.CS0WCR.lword == s->CS0WCR
                && SH7305_BSC.CS2WCR.lword == s->CS2WCR
                && SH7305_BSC.CS3WCR.lword == s->CS3WCR
                && SH7305_BSC.CS5AWCR.lword == s->CS5aWCR)
                return CLOCK_SPEED_F1 + i;
        }
    }

    return CLOCK_SPEED_UNKNOWN;
}

void clock_set_speed(int level)
{
    if(level < CLOCK_SPEED_F1 || level > CLOCK_SPEED_F5)
        return;
    if(clock_get_speed() == level)
        return;

    struct cpg_overclock_setting const *settings = get_settings();
    if(!settings)
        return;

    struct cpg_overclock_setting const *s = &settings[level - CLOCK_SPEED_F1];
    cpg_set_overclock_setting(s);
}
