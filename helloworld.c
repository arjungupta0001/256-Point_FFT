#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xaxidma.h"
#include "xparameters.h"
#include <xtime_l.h>
#include <complex.h>
#include <stdbool.h>

#define N 256

typedef float DType;
;

XAxiDma AxiDma;
int init_DMA();
u32 checkIdle(u32 baseAddress, u32 offset);
void FFT_sw(float FFTIn_I[N], float FFTIn_R[N], float FFTOut_I[N], float FFTOut_R[N]);

int main()
{
    init_platform();

    xil_printf("START\n\r");
    xil_printf("\r\n*1024-Point FFT*\r\n");

    XTime tprocessorStart, tprocessorend;
    XTime tfpgaStart, tfpgaend;

    float complex FFT_input[N];

    float data_IN_real[N];
    float data_IN_im[N];

    float FFT_out_R[N];
    float FFT_out_I[N];

    for (int k = 0; k < N; k++)
    {
        FFT_input[k] = (k / 4) + (k / 2) * I;
        data_IN_real[k] = creal(FFT_input[k]);
        data_IN_im[k] = cimag(FFT_input[k]);
    }
    //  PS start
    xil_printf("\r\n*PS Part Start\r\n");
    XTime_GetTime(&tprocessorStart);
    FFT_sw(data_IN_im, data_IN_real, FFT_out_I, FFT_out_R);
    XTime_GetTime(&tprocessorend);
    // PS end

    // PL start
    int status_dma = init_DMA();
    if (status_dma != XST_SUCCESS)
    {
        xil_printf("Couldn't initialize DMA\r\n");
        return XST_FAILURE;
    }
    float complex RX_PNTR[N];

    xil_printf("\nDMA status before transfer\r\nDMA to Device: %d, Device to DMA:%d\r\n", checkIdle(XPAR_AXI_DMA_0_BASEADDR, 0X4), checkIdle(XPAR_AXI_DMA_0_BASEADDR, 0X34));
    xil_printf("\rStarting Data Transfer--------->>>>>>\r\n");

    int status_transfer;
    XTime_GetTime(&tfpgaStart);
    Xil_DCacheFlushRange((UINTPTR)FFT_input, (sizeof(float complex) * N));
    Xil_DCacheFlushRange((UINTPTR)RX_PNTR, (sizeof(float complex) * N));
    status_transfer = XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR)RX_PNTR, (sizeof(float complex) * N), XAXIDMA_DEVICE_TO_DMA);
    if (status_transfer != XST_SUCCESS)
    {
        xil_printf("wRITING data from FFT_IP via DMA failed\r\n");
    }
    status_transfer = XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR)FFT_input, (sizeof(float complex) * N), XAXIDMA_DMA_TO_DEVICE);
    if (status_transfer != XST_SUCCESS)
    {
        xil_printf("Reading data from FFT via DMA failed\r\n");
    }

    // poling
   int status = checkIdle(XPAR_AXI_DMA_0_BASEADDR, 0x4);
   while (status != 2)
  {
       status = checkIdle(XPAR_AXI_DMA_0_BASEADDR, 0X4);
   }
    status = checkIdle(XPAR_AXI_DMA_0_BASEADDR, 0X34);
   while (status != 2){
	status = checkIdle(XPAR_AXI_DMA_0_BASEADDR, 0X34);
    }
    Xil_DCacheInvalidateRange((UINTPTR)RX_PNTR, (sizeof(float complex) * N));
    XTime_GetTime(&tfpgaend);
    xil_printf("\nComparing software FFT output and hardware FFT outout\r\n");

    int j = 0;
    bool err_flag = false;

    for (j = 0; j < N; j++)
    {
        printf("PS Output:%f+ I%f,PL output:%f,I%f \n", FFT_out_R[j], FFT_out_I[j], crealf (RX_PNTR[j]), cimagf(RX_PNTR[j]));
        float diff1 = abs(FFT_out_R[j] - crealf(RX_PNTR[j]));
        float diff2 = abs(FFT_out_I[j] - cimagf(RX_PNTR[j]));
        if (diff1 >= 0.0001 && diff2 >= 0.0001)
        {
            err_flag = true;
            break;
        }
    }
    if (err_flag)
        printf("Data Mismatch found at %d. Software output:%f+I%f.Hardware output: %f +I%f \r\n", j, FFT_out_R[j], FFT_out_I[j], crealf(RX_PNTR[j]), cimagf(RX_PNTR[j]));
    else
        xil_printf("\nFFT ran Successfully!!:) ");

    // calculate Time

    xil_printf("\n-------TIME COMPARISION---------/-\n");

    float time_processor = 0;
    time_processor = (float)1.0 * (tprocessorend - tprocessorStart) / (COUNTS_PER_SECOND / 1000000);
    printf("time for PS : %f\n", time_processor);

    float time_fpga=0;
    time_fpga=(float)1.0*(tfpgaend-tfpgaStart)/ (COUNTS_PER_SECOND/1000000);
    printf("time for PL:%f\n", time_fpga);

    float accFactor;
    accFactor = (time_processor / time_fpga);

    printf("Acceleration factor :%f\n", (float)accFactor);

    cleanup_platform();
    return 0;
}

//------------DMA--------...
int init_DMA()
{
    XAxiDma_Config *CfgPtr;

    int status;
    CfgPtr = XAxiDma_LookupConfig(XPAR_AXI_DMA_0_DEVICE_ID);
    if (!CfgPtr)
    {
        xil_printf("No Config found for %d\r\n", XPAR_AXI_DMA_0_DEVICE_ID);
        return XST_FAILURE;
    }
    status = XAxiDma_CfgInitialize(&AxiDma, CfgPtr);
    if (status !=XST_SUCCESS)
    {
        xil_printf("DMA Initialization Failed.Return Status:%d\r\n", status);
        return XST_FAILURE;
    }
    if (XAxiDma_HasSg(&AxiDma))
    {
        xil_printf("Devices configuration as SG mode \r\n");
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}
u32 checkIdle(u32 baseAddress, u32 offset)
{
    u32 status;
    status = (XAxiDma_ReadReg(baseAddress, offset)) & XAXIDMA_IDLE_MASK;
    return status;
}
