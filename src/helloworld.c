// main.c — HDMI passthrough (VDMA + LBP) with IRQ-based detect + HPD fallback
// Requires Digilent display_ctrl, video_capture, intc, timer_ps libs in project.

#include "xparameters.h"
#include "xstatus.h"
#include "xil_printf.h"
#include "xil_cache.h"
#include "xil_types.h"
#include <string.h>

#include "xaxivdma.h"
#include "xgpio.h"
#include "display_ctrl/display_ctrl.h"
#include "video_capture/video_capture.h"
#include "intc/intc.h"
#include "timer_ps/timer_ps.h"

#include "xlbp_filter.h"   // LBP HLS driver header

// ===== IRQ IDs (from xparameters.h) =====
#ifndef VID_VTC_IRPT_ID
  #define VID_VTC_IRPT_ID   XPAR_FABRIC_V_TC_1_IRQ_INTR
#endif
#ifndef VID_GPIO_IRPT_ID
  #define VID_GPIO_IRPT_ID  XPAR_FABRIC_AXI_GPIO_VIDEO_IP2INTC_IRPT_INTR
#endif

// ===== Device IDs / Base addrs =====
#define DYNCLK_BASEADDR    XPAR_AXI_DYNCLK_0_S_AXI_LITE_BASEADDR
#define VGA_VDMA_ID        XPAR_AXIVDMA_0_DEVICE_ID
#define DISP_VTC_ID        XPAR_VTC_0_DEVICE_ID
#define VID_VTC_ID         XPAR_VTC_1_DEVICE_ID
#define VID_GPIO_ID        XPAR_AXI_GPIO_VIDEO_DEVICE_ID
#define SCU_TIMER_ID       XPAR_XSCUTIMER_0_DEVICE_ID
#define AXI_GPIO_VIDEO_BASE XPAR_AXI_GPIO_VIDEO_BASEADDR

#define LBP_ID             XPAR_LBP_FILTER_0_DEVICE_ID  // auto-generated in xparameters.h

// ===== Framebuffer (max 1080p RGB888) =====
#define MAX_W              1920
#define MAX_H              1080
#define BPP                3
#define DISPLAY_NUM_FRAMES 3
#define DEMO_STRIDE        (MAX_W*BPP)
#define MAX_FRAME_BYTES    (MAX_W*MAX_H*BPP)

static u8 frameBuf[DISPLAY_NUM_FRAMES][MAX_FRAME_BYTES] __attribute__((aligned(64)));
static u8 *pFrames[DISPLAY_NUM_FRAMES];

// ===== Globals =====
static DisplayCtrl  gDisp;
static VideoCapture gCap;
static XAxiVdma     gVdma;
static INTC         gIntc;
static XGpio        gGpio;
static XLbp_filter  gLbp;   // LBP instance

// ===== LBP helper =====
static int Lbp_Init(void) {
    XLbp_filter_Config *cfg = XLbp_filter_LookupConfig(LBP_ID);
    if (!cfg) { xil_printf("LBP cfg not found\r\n"); return XST_FAILURE; }
    int s = XLbp_filter_CfgInitialize(&gLbp, cfg);
    if (s != XST_SUCCESS) { xil_printf("LBP init fail %d\r\n", s); return s; }
    XLbp_filter_EnableAutoRestart(&gLbp);
    xil_printf("LBP init OK\r\n");
    return XST_SUCCESS;
}
static inline void Lbp_Run(u32 w, u32 h) {
    XLbp_filter_Set_width (&gLbp, w);
    XLbp_filter_Set_height(&gLbp, h);
    XLbp_filter_Start(&gLbp);
    xil_printf("LBP started with %ux%u\r\n", w, h);
}

// ===== Video helpers =====
static volatile char gRefresh = 0;
static void OnVideoDetect(void *cbRef, void *pVideo) {
    (void)pVideo;
    *(volatile char*)cbRef = 1;
}

static int ReconfigVDMA_RW_ConstStride(XAxiVdma *vdma, u32 width, u32 height, u8 **frames) {
    XAxiVdma_DmaSetup wr; memset(&wr, 0, sizeof(wr));
    XAxiVdma_DmaSetup rd; memset(&rd, 0, sizeof(rd));
    u32 hsize = width * BPP;

    wr.VertSizeInput = height;
    wr.HoriSizeInput = hsize;
    wr.Stride        = DEMO_STRIDE;
    wr.EnableCircularBuf = 1;

    int s = XAxiVdma_DmaConfig(vdma, XAXIVDMA_WRITE, &wr);
    if (s != XST_SUCCESS) { xil_printf("VDMA WR cfg err %d\r\n", s); return s; }
    UINTPTR wrBuf[DISPLAY_NUM_FRAMES] = { (UINTPTR)frames[0], (UINTPTR)frames[1], (UINTPTR)frames[2] };
    s = XAxiVdma_DmaSetBufferAddr(vdma, XAXIVDMA_WRITE, wrBuf);
    if (s != XST_SUCCESS) { xil_printf("VDMA WR buf err %d\r\n", s); return s; }

    rd.VertSizeInput = height;
    rd.HoriSizeInput = hsize;
    rd.Stride        = DEMO_STRIDE;
    rd.EnableCircularBuf = 1;

    s = XAxiVdma_DmaConfig(vdma, XAXIVDMA_READ, &rd);
    if (s != XST_SUCCESS) { xil_printf("VDMA RD cfg err %d\r\n", s); return s; }
    UINTPTR rdBuf[DISPLAY_NUM_FRAMES] = { (UINTPTR)frames[0], (UINTPTR)frames[1], (UINTPTR)frames[2] };
    s = XAxiVdma_DmaSetBufferAddr(vdma, XAXIVDMA_READ, rdBuf);
    if (s != XST_SUCCESS) { xil_printf("VDMA RD buf err %d\r\n", s); return s; }

    s = XAxiVdma_DmaStart(vdma, XAXIVDMA_WRITE);
    if (s != XST_SUCCESS) { xil_printf("VDMA WR start err %d\r\n", s); return s; }
    s = XAxiVdma_DmaStart(vdma, XAXIVDMA_READ);
    if (s != XST_SUCCESS) { xil_printf("VDMA RD start err %d\r\n", s); return s; }
    XAxiVdma_StartParking(vdma, 0, XAXIVDMA_READ);
    xil_printf("VDMA reconfig OK: %ux%u\r\n", width, height);
    return XST_SUCCESS;
}

static int ApplyInputTiming(DisplayCtrl *disp, u32 inW, u32 inH) {
    int s;
    xil_printf("ApplyInputTiming: %ux%u\r\n", inW, inH);
    s = DisplayStop(disp);
    if (s != XST_SUCCESS) xil_printf("DisplayStop -> %d\r\n", s);

    if (inW==1920 && inH==1080) {
        s = DisplaySetMode(disp, &VMODE_1920x1080);
    } else if (inW==1280 && inH==720) {
        s = DisplaySetMode(disp, &VMODE_1280x720);
    } else if (inW==1280 && inH==1024) {
        s = DisplaySetMode(disp, &VMODE_1280x1024);
    } else {
        s = DisplaySetMode(disp, &VMODE_1920x1080);
    }
    xil_printf("DisplaySetMode -> %d\r\n", s);
    s = DisplayStart(disp);
    xil_printf("DisplayStart -> %d\r\n", s);
    return s;
}

static void HdmiRx_HpdPulse(void) {
    XGpio_Initialize(&gGpio, VID_GPIO_ID);
    XGpio_SetDataDirection(&gGpio, 1, 0xFFFFFFFE);
    XGpio_DiscreteWrite(&gGpio, 1, 0x00000000);
    TimerDelay(50000);
    XGpio_DiscreteWrite(&gGpio, 1, 0x00000001);
    XGpio_SetDataDirection(&gGpio, 2, 0xFFFFFFFE);
    XGpio_DiscreteWrite(&gGpio, 2, 0x00000000);
    TimerDelay(50000);
    XGpio_DiscreteWrite(&gGpio, 2, 0x00000001);
    xil_printf("HPD pulse asserted\r\n");
}

static void ForceFallbackStart(void) {
    xil_printf("Forcing fallback: HPD + 1280x1024\r\n");
    HdmiRx_HpdPulse();
    ApplyInputTiming(&gDisp, 1280, 1024);
    ReconfigVDMA_RW_ConstStride(&gVdma, 1280, 1024, pFrames);
    Lbp_Run(1280, 1024);  // run LBP in fallback
}

// ===== main =====
int main(void) {
    xil_printf("HDMI passthrough (VDMA + LBP) — detect+fallback start\r\n");

    Xil_DCacheDisable();
    for (int i = 0; i < DISPLAY_NUM_FRAMES; ++i) pFrames[i] = frameBuf[i];
    TimerInitialize(SCU_TIMER_ID);

    XAxiVdma_Config *vCfg = XAxiVdma_LookupConfig(VGA_VDMA_ID);
    if (!vCfg) { xil_printf("No VDMA cfg\r\n"); return 0; }
    if (XAxiVdma_CfgInitialize(&gVdma, vCfg, vCfg->BaseAddress) != XST_SUCCESS) {
        xil_printf("VDMA init fail\r\n"); return 0;
    }

    if (DisplayInitialize(&gDisp, &gVdma, DISP_VTC_ID, DYNCLK_BASEADDR, pFrames, DEMO_STRIDE) != XST_SUCCESS) {
        xil_printf("DisplayInitialize failed\r\n"); return 0;
    }
    DisplayStart(&gDisp);

    if (fnInitInterruptController(&gIntc) != XST_SUCCESS) {
        xil_printf("INTC init failed\r\n"); return 0;
    }
    const ivt_t ivt[] = {
        videoGpioIvt(VID_GPIO_IRPT_ID, &gCap),
        videoVtcIvt (VID_VTC_IRPT_ID,  &gCap),
    };
    fnEnableInterrupts(&gIntc, &ivt[0], sizeof(ivt)/sizeof(ivt[0]));

    if (VideoInitialize(&gCap, &gIntc, &gVdma, VID_GPIO_ID, VID_VTC_ID,
                        VID_VTC_IRPT_ID, pFrames, DEMO_STRIDE, 1) != XST_SUCCESS) {
        xil_printf("VideoInitialize failed\r\n"); return 0;
    }
    VideoSetCallback(&gCap, OnVideoDetect, (void*)&gRefresh);

    // init LBP
    if (Lbp_Init() != XST_SUCCESS) {
        xil_printf("LBP init failed\r\n"); return 0;
    }

    xil_printf("Waiting HDMI input...\r\n");
    int waited = 0;
    for (; waited < 200; ++waited) {
        if (gCap.state != VIDEO_DISCONNECTED) break;
        TimerDelay(10000);
    }
    if (gCap.state == VIDEO_DISCONNECTED) {
        xil_printf("No HDMI signal detected — fallback.\r\n");
        ForceFallbackStart();
    }

    u32 lastW = 0, lastH = 0;
    while (1) {
        if (gCap.state != VIDEO_DISCONNECTED) {
            u32 w = gCap.timing.HActiveVideo;
            u32 h = gCap.timing.VActiveVideo;
            if (w != lastW || h != lastH || gRefresh) {
                xil_printf("HDMI detected: %ux%u (refresh=%d)\r\n", w, h, gRefresh);
                ApplyInputTiming(&gDisp, w, h);
                ReconfigVDMA_RW_ConstStride(&gVdma, w, h, pFrames);
                Lbp_Run(w, h);   // run LBP with current resolution
                lastW = w; lastH = h; gRefresh = 0;
            }
        }
        TimerDelay(50000);
    }
    return 0;
}
