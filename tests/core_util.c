/*
 * Copyright 2016-2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#if defined(CONFIG_RPMSG_TEST_ENABLE) && ( CONFIG_RPMSG_TEST_ENABLE == 1 )

#include "rpmsg_lite.h"
#include "fsl_device_registers.h"
#include "fsl_common.h"
#include "unity.h"
#include "board.h"

#include "app.h"
#if defined(FSL_FEATURE_MU_SIDE_A) || defined(FSL_FEATURE_MAILBOX_SIDE_A) || \
    (defined(IMU_CPU_INDEX) && (IMU_CPU_INDEX == 1U))
#include "mcmgr.h"
#include "fsl_debug_console.h"
#endif

#if defined(FSL_FEATURE_MU_SIDE_A)
#define MU_INSTANCE MU0_A
#elif defined(FSL_FEATURE_MAILBOX_SIDE_A)
#define MU_INSTANCE MU0_A
#elif defined(FSL_FEATURE_MU_SIDE_B)
#define MU_INSTANCE MU0_B
#elif defined(FSL_FEATURE_MAILBOX_SIDE_B)
#define MU_INSTANCE MU0_B
#endif

#if defined(SDK_OS_FREE_RTOS)
#include "FreeRTOS.h"
#include "task.h"
#elif defined(FSL_RTOS_THREADX)
#include "tx_api.h"
#elif defined(FSL_RTOS_XOS)
#include <xtensa/xos.h>
#endif

#define TEST_TASK_STACK_SIZE 400

#if defined(SDK_OS_FREE_RTOS) && ( configSUPPORT_DYNAMIC_ALLOCATION == 0 )
static StaticTask_t xTaskBuffer;
static StackType_t xStack[ TEST_TASK_STACK_SIZE ];

/* configSUPPORT_STATIC_ALLOCATION is set to 1, so the application must provide an
implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
used by the Idle task. */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize )
{
/* If the buffers to be provided to the Idle task are declared inside this
function then they must be declared static - otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xIdleTaskTCB;
static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
    state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

/* configSUPPORT_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer,
                                     StackType_t **ppxTimerTaskStackBuffer,
                                     uint32_t *pulTimerTaskStackSize )
{
/* If the buffers to be provided to the Timer task are declared inside this
function then they must be declared static - otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xTimerTaskTCB;
static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

    /* Pass out a pointer to the StaticTask_t structure in which the Timer
    task's state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task's stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configTIMER_TASK_STACK_DEPTH is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
#endif

extern void run_tests(void *unused);

void setUp(void)
{
}

void tearDown(void)
{
}

/* This function is used for the Corn test automation framework
   to breakpoint/stop the execution and to capture results
   from the memory. It must be ensured that it will never be inlined
   and optimized to allow proper address recognition and breakpoint
   placement during the Corn execution. */
__attribute__((noinline)) void CornBreakpointFunc(void)
{
    volatile int i=0;
    i++;
}

#if defined(FSL_RTOS_THREADX)
static VOID run_test_suite(ULONG arg)
#else
void run_test_suite(void *unused)
#endif
{
#ifdef CORE1_IMAGE_COPY_TO_RAM
    /* This section ensures the secondary core image is copied from flash location to the target RAM memory.
       It consists of several steps: image size calculation and image copying.
       These steps are not required on MCUXpresso IDE which copies the secondary core image to the target memory during
       startup automatically. */
    uint32_t core1_image_size;
    core1_image_size = get_core1_image_size();
    PRINTF("Copy Secondary core image to address: 0x%x, size: %d\r\n", (void *)(char *)CORE1_BOOT_ADDRESS, core1_image_size);

    /* Copy Secondary core application from FLASH to RAM. Primary core code is executed from FLASH, Secondary from RAM
     * for maximal effectivity.*/
    memcpy((void *)(char *)CORE1_BOOT_ADDRESS, (void *)CORE1_IMAGE_START, core1_image_size);
#endif

#ifdef APP_INVALIDATE_CACHE_FOR_SECONDARY_CORE_IMAGE_MEMORY
    invalidate_cache_for_core1_image_memory(CORE1_BOOT_ADDRESS, core1_image_size);
#endif /* APP_INVALIDATE_CACHE_FOR_SECONDARY_CORE_IMAGE_MEMORY */

    /* Initialize MCMGR before calling its API */
    MCMGR_Init();

/* The CORE1_BOOT_ADDRESS should only be defined on primary core
 * This select code to run only on primary core which is responsible for
 * starting secondary core.
 */
#if defined(CORE1_BOOT_ADDRESS)
    PRINTF("Starting Primary core.\r\n");
    MCMGR_StartCore(kMCMGR_Core1, (void *)(char *)CORE1_BOOT_ADDRESS, 0, kMCMGR_Start_Asynchronous);

    /* Wait for remote side to come up. This delay is arbitrary and may
    * need adjustment for different configuration of remote systems */
    env_sleep_msec(1000);
#endif

    UnityBegin();
    run_tests(NULL);
    UnityEnd();

#if defined(IMU_CPU_INDEX) && (IMU_CPU_INDEX == 1U)
#ifdef __COVERAGESCANNER__
#if defined(SQUISHCOCO_RESULT_DATA_SAVE_TO_FILE)
    /* Store the secondary core measurement data (saved temporarily in shared memory) into the file */
    FILE *fptr;
    /* Wait until all SQUISHCOCO RESULT DATA is stored in the shared memory and the SQUISHCOCO_SEED_FLAG is set */
    while(SQUISHCOCO_SEED_FLAG != (*(uint32_t *)(RPMSG_LITE_SHMEM_BASE + 0x4)));
        ;
    fptr = fopen("rpmsg_lite_test_sec_core.csexe","w");
    fwrite((const void *)(RPMSG_LITE_SHMEM_BASE + 0x10), sizeof(char), *(uint32_t *)RPMSG_LITE_SHMEM_BASE,fptr);
    fclose(fptr);
#elif defined(SQUISHCOCO_RESULT_DATA_SAVE_TO_CONSOLE)
    /* Printf the secondary core measurement data (saved temporarily in shared memory) into the console */
    char *s_ptr = (char *)(RPMSG_LITE_SHMEM_BASE + 0x10);
    for(int32_t i=0; i<(*(uint32_t *)RPMSG_LITE_SHMEM_BASE);i++)
        PRINTF("%c", s_ptr[i]);
#endif
#endif /*__COVERAGESCANNER__*/
#endif

    CornBreakpointFunc();
    while (1)
        ;
}

#if defined(SDK_OS_FREE_RTOS)
TaskHandle_t test_task_handle = NULL;
int main(void)
{
    BOARD_InitHardware();

#if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
    if (xTaskCreate(run_test_suite, "TEST_TASK", TEST_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &test_task_handle) != pdPASS)
    {
        (void)PRINTF("\r\nFailed to create application task\r\n");
        for (;;)
        {
        }
    }
#else
    test_task_handle = xTaskCreateStatic(run_test_suite, "TEST_TASK", TEST_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, xStack, &xTaskBuffer);
#endif

    vTaskStartScheduler();
    return 0;
}
#elif defined(FSL_RTOS_THREADX)
#if defined(__GNUC__)
uint32_t memHeapExt[MinimalHeapSize_c / sizeof(uint32_t)] __attribute__((section(".heap_ext, \"aw\", %nobits @")));
uint32_t *memHeap = &memHeapExt[0];
const uint32_t memHeapEnd = (uint32_t)(memHeapExt + MinimalHeapSize_c / sizeof(uint32_t));
#endif
static TX_THREAD test_task_thread;
static ULONG test_task_stack[TEST_TASK_STACK_SIZE * sizeof(ULONG)];
void tx_application_define(void *first_unused_memory)
{
    UINT status;

    TX_THREAD_NOT_USED(first_unused_memory);

    status = tx_thread_create(&test_task_thread, "TEST_TASK",
                              run_test_suite, 0,
                              (VOID *)test_task_stack, (TEST_TASK_STACK_SIZE * sizeof(ULONG)),
                              (TX_MAX_PRIORITIES - 1), (TX_MAX_PRIORITIES - 1), 1, TX_AUTO_START);
    if (status != TX_SUCCESS)
    {
        return;
    }
}
int main(void)
{
    BOARD_InitHardware();

    /* Enter the ThreadX kernel.  */
    tx_kernel_enter();
    return 0;
}
#else
int main(void)
{
    BOARD_InitHardware();
    run_test_suite(NULL);
    return 0;
}
#endif

#endif /* defined(CONFIG_RPMSG_TEST_ENABLE) && ( CONFIG_RPMSG_TEST_ENABLE == 1 ) */
