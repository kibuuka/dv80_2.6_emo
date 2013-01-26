/* drivers/xxx/xxx/msm8x60-keypad.c - virtula key and some keys driver
*
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <mach/board.h>
#include <asm/mach-types.h>
#include <linux/jiffies.h>
#include <mach/vreg.h>
#include <linux/slab.h>

#if 1   // Copy from Atmel-mxt224.c (drivers\input\touchscreen)
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/err.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/poll.h>
#include <linux/kfifo.h>
#include <linux/version.h>
#include <linux/irq.h>
#include <linux/regulator/consumer.h>
#endif
#include <linux/pmic8058-pwrkey.h>
#include <mach/gpio.h>

//Henry_lin, 20110627, [BugID 357] Change GPIO for volume up/down.
#include <mach/cci_hw_id.h>

/*---------------------  Static Definitions -------------------------*/
#define HENRY_DEBUG 0   //0:disable, 1:enable
#if(HENRY_DEBUG)
    #define Printhh(string, args...)    printk("Henry(K)=> "string, ##args);
#else
    #define Printhh(string, args...)
#endif

#define HENRY_TIP 1 //give RD information. Set 1 if develop,and set 0 when release.
#if(HENRY_TIP)
    #define PrintTip(string, args...)    printk("Henry(K)=> "string, ##args);
#else
    #define PrintTip(string, args...)
#endif

//
// GPIO(MSM8x60) for Virtual kery (Back, Menu, Home, Search)
//

//Henry_lin, 20110527, [BugID 158] Change Touch more smooth and virtual key config.
#if 0   //for EVB
#define CCI_BACK_KEY_GPIO       41
#define CCI_MENU_KEY_GPIO       39
#define CCI_HOME_KEY_GPIO       29
#define CCI_SEARCH_KEY_GPIO     51
#else   //for EVT
#define CCI_BACK_KEY_GPIO       39
#define CCI_MENU_KEY_GPIO       29
#define CCI_HOME_KEY_GPIO       51
#define CCI_SEARCH_KEY_GPIO     57
#endif

#define CCI_CAP_KEY_RST_GPIO     86

//Henry_lin, 20110627, [BugID 357] Change GPIO for volume up/down.
//Henry_lin, 20110713, [BugID 457] Change GPIO for volume up/down.
#if 0
#define CCI_VOLUME_UP_KEY_GPIO       105
#define CCI_VOLUME_DOWN_KEY_GPIO       106
#endif


//Henry_lin, 20110721, [BugID 510] Add debounce function for volume up and volume down GPIO.
#if 0
#define CCI_VOLUME_UP_KEY_GPIO       96
#define CCI_VOLUME_DOWN_KEY_GPIO       99
#endif

//
// GPIO(MSM8x60) for Others kery (CAMERA_KEY1, CAMERA_KEY2, MUTE/RINGER)
//
#define CCI_CAMERA_AF_GPIO                144
#define CCI_CAMERA_CAPTURE_GPIO       146
//#define CCI_MUTE_RING_GPIO                  34  //move to Keypad-surf-ffa.c


#define DA80_KEY_CAM_AF	                KEY_HP
#define DA80_KEY_CAM_CAPTURE            KEY_CAMERA
//#define DA80_KEY_MUTE_RING                 KEY_MUTE //move to Keypad-surf-ffa.c


#define IRQ_REASON_NORMAL            0
#define IRQ_REASON_RESUME            1


/*---------------------  Static Classes  ----------------------------*/
struct tagVirtualKeyInfo
{
	int iGpio;
	struct work_struct *psWork;
};
typedef struct tagVirtualKeyInfo   SVirtualKeyInfo;

struct tagOthersKeyInfo
{
	int iGpio;
	struct workqueue_struct *psOthersKeyWq;
	struct work_struct work;
};
typedef struct tagOthersKeyInfo   SOthersKeyInfo;

/*---------------------  Static Types  ------------------------------*/
struct sMsm8x60_gpio_data {
    int *paiSupportKey;
    struct early_suspend early_suspend;
    struct device	*pDev;
};


/*---------------------  Static Macros  -----------------------------*/

/*---------------------  Static Functions  --------------------------*/
static void s_vInitVirtualKey(void);

//Henry_lin, 20110826, [BugID 736] Supply early suspend function for virtual key.
#ifdef CONFIG_HAS_EARLYSUSPEND
static void msm8x60_gpio_early_suspend(struct early_suspend *h);
static void msm8x60_gpio_late_resume(struct early_suspend *h);
#endif

/*---------------------  Static Variables  --------------------------*/

extern void s_vKeyArray(int iKeyId);
extern struct input_dev *tskpdev;

//Henry_lin, 20110504, [BugID 66] Add wake key Camera_focus, Camera_cap, and Home key to wake up system.
int g_iIrqCanWakeSys[MAX_WAKE_KEY_NUM];

int g_aiSupportKey[] = {CCI_BACK_KEY_GPIO, CCI_MENU_KEY_GPIO, CCI_HOME_KEY_GPIO, CCI_SEARCH_KEY_GPIO};
char *g_asKeyStr[] = {"back_key", "menu_key", "home_key", "search_key"};
int g_aiSupportKeyIrq[4];
struct work_struct g_sWork;

//Henry_lin, 20111030, [BugID 1175] Ignore key event once when system resume.
int g_iIrqReason = IRQ_REASON_NORMAL;


//Henry_lin, 20110824, [BugID 712] Reset virtual key IC while they can't work.
#if 1
extern int g_iVirtualKeyResetEn;    // define in Atmel-mxt224.c (drivers\input\touchscreen)
extern int g_iHomeKeyWakeUpEn;
#endif


static irqreturn_t virtualKey_irq_handler(int irq, void *dev_id)
{
    SVirtualKeyInfo *psVkInfo = dev_id;
    int iSatus = 0, iKeyId = 0;
    int ii;
#if 1
    int iPressNum = 0;
#endif


    //Printhh("[%s] enter...pgio=%d\n", __FUNCTION__, psVkInfo->iGpio);

    //Henry_lin, 20111030, [BugID 1175] Ignore key event once when system resume.
#if 1   // while virtual key ic power on, will triger irq
    if(g_iIrqReason != IRQ_REASON_NORMAL)
    {
        g_iIrqReason = IRQ_REASON_NORMAL;
        return IRQ_HANDLED;
    }
#endif


    for( ii = 0 ; ii < (sizeof(g_aiSupportKey)/sizeof(int)) ; ii++)
        disable_irq_nosync(g_aiSupportKeyIrq[ii]);

#if 1
    if(g_iVirtualKeyResetEn == 1)
    {
        //Henry_lin, 20110824, [BugID 712] Reset virtual key IC while they can't work.
        for( ii = 0 ; ii < (sizeof(g_aiSupportKey)/sizeof(int)) ; ii++)
        {
            iSatus = (unsigned int) gpio_get_value(g_aiSupportKey[ii]);
            //PrintTip("[%s] gpio(%d) value =%d\n", __FUNCTION__, g_aiSupportKey[ii], iSatus);  // press is 0, release is 1
            if(iSatus == 0)
            {
                iPressNum++;
                if(iPressNum > 1)
                {
                    PrintTip("[%s] more than one key press\n", __FUNCTION__);
                    // do ic reset
                    schedule_work(psVkInfo->psWork);  //==msm8x60_gpio_reset_vk()
                    goto ErrKey;
                }
            }
        }
    }
#endif

    iSatus = (unsigned int) gpio_get_value(psVkInfo->iGpio);
    //Printhh("[%s] value =%d\n", __FUNCTION__, iSatus);  // press is 0, release is 1

    if(iSatus == 1){
        s_vKeyArray(-1);
    }
    else
    {
        switch(psVkInfo->iGpio)
        {
            case CCI_BACK_KEY_GPIO:
                iKeyId = 0x2;
                break;
            case CCI_MENU_KEY_GPIO:
                iKeyId = 0x01;
                break;
            case CCI_HOME_KEY_GPIO:
                iKeyId = 0x04;
                break;
            case CCI_SEARCH_KEY_GPIO:
                iKeyId = 0x08;
                break;
            default:
                PrintTip("[%s] Unknow gpio=%d\n", __FUNCTION__, psVkInfo->iGpio);
                goto ErrKey;
        }
        s_vKeyArray(iKeyId);
    }

ErrKey:   

    for( ii = 0 ; ii < (sizeof(g_aiSupportKey)/sizeof(int)) ; ii++)
        enable_irq(g_aiSupportKeyIrq[ii]);

    return IRQ_HANDLED;
}


static irqreturn_t othersKey_irq_handler(int irq, void *dev_id)
{
    SOthersKeyInfo *sOkInfo = dev_id;
    int iSatus = 0;
    unsigned int uiCode = 0;


    Printhh("[%s] enter...pgio=%d\n", __FUNCTION__, sOkInfo->iGpio);
    iSatus = (unsigned int) gpio_get_value(sOkInfo->iGpio);
    Printhh("[%s] value =%d\n", __FUNCTION__, iSatus);  // press is 0, release is 1


#if 0
    switch(sOkInfo->iGpio)
    {
        case CCI_CAMERA_AF_GPIO:
            uiCode = DA80_KEY_CAM_AF;
            break;
        case CCI_CAMERA_CAPTURE_GPIO:
            uiCode = DA80_KEY_CAM_CAPTURE;
            break;
        default:
            PrintTip("[%s] Unknow gpio=%d\n", __FUNCTION__, sOkInfo->iGpio);
            goto ErrKey;
    }
#endif

//Henry_lin, 20110627, [BugID 357] Change GPIO for volume up/down.
    if(cci_hw_id >= EVT2)
    {
        //Henry_lin, 20110721, [BugID 510] Add debounce function for volume up and volume down GPIO.
        #if 0
        switch(sOkInfo->iGpio)
        {
            case CCI_CAMERA_AF_GPIO:
                uiCode = DA80_KEY_CAM_AF;
            break;
            case CCI_CAMERA_CAPTURE_GPIO:
                uiCode = DA80_KEY_CAM_CAPTURE;
            break;
            case CCI_VOLUME_UP_KEY_GPIO:
                uiCode = KEY_VOLUMEUP;
            break;
            case CCI_VOLUME_DOWN_KEY_GPIO:
                uiCode = KEY_VOLUMEDOWN;
            break;
            default:
                PrintTip("[%s] Unknow gpio=%d\n", __FUNCTION__, sOkInfo->iGpio);
                goto ErrKey;
        }
	 #else
        return IRQ_HANDLED;
	 #endif
    }
    else
    {
        switch(sOkInfo->iGpio)
        {
            case CCI_CAMERA_AF_GPIO:
                uiCode = DA80_KEY_CAM_AF;
            break;
                case CCI_CAMERA_CAPTURE_GPIO:
                uiCode = DA80_KEY_CAM_CAPTURE;
            break;
            default:
                PrintTip("[%s] Unknow gpio=%d\n", __FUNCTION__, sOkInfo->iGpio);
                goto ErrKey;
        }
    }
//Henry_lin


    if(iSatus == 1)
    {
        input_report_key(tskpdev, uiCode, 0);
        input_sync(tskpdev);
    }
    else
    {
        input_report_key(tskpdev, uiCode, 1);
        input_sync(tskpdev);
   }

ErrKey:
    return IRQ_HANDLED;
}


static void s_vInitVirtualKey(void)
{
    int iRet = 0, ii, rc;
#if 0
    unsigned usGetData;
#endif
    SVirtualKeyInfo *sVkInfo[10];


    for( ii = 0 ; ii < (sizeof(g_aiSupportKey)/sizeof(int)) ; ii++)
    {
        //gpio_tlmm_config_read(g_aiSupportKey[ii], &usGetData);
        //Printhh("[%s] bef gpio_request = %#x\n", __FUNCTION__, usGetData);

        //Printhh("[%s] ii = %d, GPIO_pin = %d, name=%s\n", __FUNCTION__, ii, g_aiSupportKey[ii], g_asKeyStr[ii]);
        if (gpio_request(g_aiSupportKey[ii], g_asKeyStr[ii])){
            PrintTip("[%s] gpio_request(%d, %s) fail \n", __FUNCTION__, g_aiSupportKey[ii], g_asKeyStr[ii]);
            goto err_gpio_request_failed;
        }

        //gpio_tlmm_config_read(g_aiSupportKey[ii], &usGetData);
        //Printhh("[%s] aft gpio_request = %#x\n", __FUNCTION__, usGetData);
        
        iRet = gpio_tlmm_config(GPIO_CFG(g_aiSupportKey[ii], 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP,
    				GPIO_CFG_2MA), GPIO_CFG_ENABLE);
        if (iRet){
            PrintTip("[%s] Failed to configure GPIO(%d)  iRet= %d\n", __FUNCTION__, g_aiSupportKey[ii], iRet);
            goto err_gpio_configure_failed;
        }
        //gpio_tlmm_config_read(g_aiSupportKey[ii], &usGetData);
        //Printhh("[%s] aft gpio_tlmm_config = %#x\n", __FUNCTION__, usGetData);

        sVkInfo[ii] = (SVirtualKeyInfo *) kzalloc(sizeof(SVirtualKeyInfo), GFP_KERNEL);
        #if 0   // error handling testing code
        if(ii == 2)
            sVkInfo[ii] = NULL;
        #endif 
        if (sVkInfo[ii] == NULL) {
            PrintTip("[%s]: allocate virtual_key_data failed(ii=%d)\n", __func__, ii);
            goto free_virtual_key;
        }
        sVkInfo[ii]->iGpio = g_aiSupportKey[ii];
        sVkInfo[ii]->psWork = &g_sWork;
        g_aiSupportKeyIrq[ii] = MSM_GPIO_TO_INT(g_aiSupportKey[ii]);

        iRet = request_threaded_irq(MSM_GPIO_TO_INT(g_aiSupportKey[ii]), NULL,
                virtualKey_irq_handler,
                IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                g_asKeyStr[ii],
                sVkInfo[ii]);
        #if 0   // error handling testing code
        if(ii == 3)
            iRet = 1;
        #endif 
        if (iRet) {
            PrintTip("[%s]: Unable to get slot IRQ(%d) (%d)\n", __FUNCTION__, MSM_GPIO_TO_INT(g_aiSupportKey[ii]), iRet);
            goto err_request_irq_failed;
        }

    }

    if (gpio_request(CCI_CAP_KEY_RST_GPIO, "CAP_KEY_RST")){
        PrintTip("[%s] gpio_request(%d, %s) fail \n", __FUNCTION__,CCI_CAP_KEY_RST_GPIO, "CAP_KEY_RST");
        goto err_gpio_request_failed;
    }

    rc = gpio_tlmm_config(GPIO_CFG(CCI_CAP_KEY_RST_GPIO, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL,
        GPIO_CFG_2MA), GPIO_CFG_DISABLE);
    if (rc)
        PrintTip("[%s] Failed to configure GPIO %d\n", __FUNCTION__, rc);
    gpio_direction_output(CCI_CAP_KEY_RST_GPIO, 0);


    return;

//Henry_lin, 20110720, [BugID 508] Add error handler for some GPIO init.
err_request_irq_failed:
    //PrintTip("[%s]: kfree(sVkInfo[%d])\n", __FUNCTION__, ii);
    kfree(sVkInfo[ii]);
err_gpio_configure_failed:
free_virtual_key:
    //PrintTip("[%s]: gpio_free(aiSupportKey[%d])\n", __FUNCTION__, ii);
        gpio_free(g_aiSupportKey[ii]);
err_gpio_request_failed:
        ;
}


static void s_vInitOthersKey(void)
{
    int iRet = 0, ii;
    SOthersKeyInfo *sOKInfo[10];
    int aiOthersKey[] = {CCI_CAMERA_AF_GPIO, CCI_CAMERA_CAPTURE_GPIO};
    char *asKeyStr[] = {"camera_af_key", "camera_capture_key"};
    //Henry_lin, 20110721, [BugID 510] Add debounce function for volume up and volume down GPIO.
#if 0
    int aiOthersKey22[] = {CCI_VOLUME_UP_KEY_GPIO, CCI_VOLUME_DOWN_KEY_GPIO};
    char *asKeyStr22[] = {"volume_up", "volume_down"};
#endif

#if 0
    for( ii = 0 ; ii < (sizeof(aiOthersKey)/sizeof(int)) ; ii++)
    {
        Printhh("[%s] ii = %d, GPIO_pin = %d, name=%s\n", __FUNCTION__, ii, aiOthersKey[ii], asKeyStr[ii]);
        if (gpio_request(aiOthersKey[ii], asKeyStr[ii])){
            Printhh("[%s] gpio_request(%d, %s) fail \n", __FUNCTION__, aiOthersKey[ii], asKeyStr[ii]);
        }
        
        iRet = gpio_tlmm_config(GPIO_CFG(aiOthersKey[ii], 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP,
    				GPIO_CFG_2MA), GPIO_CFG_ENABLE);
        if (iRet){
            Printhh("[%s] Failed to configure GPIO(%d)  iRet= %d\n", __FUNCTION__, aiOthersKey[ii, iRet]);
        }

        sOKInfo = (SOthersKeyInfo *) kzalloc(sizeof(SOthersKeyInfo), GFP_KERNEL);
        if (sOKInfo == NULL) {
            Printhh("[%s]: allocate atmel_ts_data failed\n", __func__);
        }
        
        sOKInfo->iGpio = aiOthersKey[ii];
        iRet = request_threaded_irq(MSM_GPIO_TO_INT(aiOthersKey[ii]), NULL,
                othersKey_irq_handler,
                IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                asKeyStr[ii],
                sOKInfo);
        
        if (iRet) {
            Printhh("[%s]: Unable to get slot IRQ(%d) (%d)\n", __FUNCTION__, MSM_GPIO_TO_INT(aiOthersKey[ii]), iRet);
        }

    }
#endif

//Henry_lin, 20110627, [BugID 357] Change GPIO for volume up/down.
    if(cci_hw_id >= EVT2)
    {
        PrintTip("[%s] cci_hw_id=%d (0:EVT0,1:EVT1;2:EVT2)\n", __FUNCTION__, cci_hw_id);
        //PrintTip("[%s] Do nothing\n", __FUNCTION__);
        //Henry_lin, 20110721, [BugID 510] Add debounce function for volume up and volume down GPIO.
        #if 0
       for( ii = 0 ; ii < (sizeof(aiOthersKey22)/sizeof(int)) ; ii++)
        {
            Printhh("[%s] ii = %d, GPIO_pin = %d, name=%s\n", __FUNCTION__, ii, aiOthersKey22[ii], asKeyStr22[ii]);
            if (gpio_request(aiOthersKey22[ii], asKeyStr22[ii])){
                PrintTip("[%s] gpio_request(%d, %s) fail \n", __FUNCTION__, aiOthersKey22[ii], asKeyStr22[ii]);
                goto err_gpio_request_failed2;
            }
            
            iRet = gpio_tlmm_config(GPIO_CFG(aiOthersKey22[ii], 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP,
        				GPIO_CFG_2MA), GPIO_CFG_ENABLE);
            if (iRet){
                PrintTip("[%s] Failed to configure GPIO(%d)  iRet= %d\n", __FUNCTION__, aiOthersKey22[ii], iRet);
                goto err_gpio_configure_failed2;
            }

            sOKInfo[ii] = (SOthersKeyInfo *) kzalloc(sizeof(SOthersKeyInfo), GFP_KERNEL);
            if (sOKInfo[ii] == NULL) {
                PrintTip("[%s]: allocate others_key_data failed\n", __func__);
                goto free_other_key2;
            }
            
            sOKInfo[ii]->iGpio = aiOthersKey22[ii];
            iRet = request_threaded_irq(MSM_GPIO_TO_INT(aiOthersKey22[ii]), NULL,
                    othersKey_irq_handler,
                    IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                    asKeyStr22[ii],
                    sOKInfo[ii]);
            
            if (iRet) {
                PrintTip("[%s]: Unable to get slot IRQ(%d) (%d)\n", __FUNCTION__, MSM_GPIO_TO_INT(aiOthersKey22[ii]), iRet);
                goto err_request_irq_failed2;
            }

        }
        #endif

    }
    else
    {
        PrintTip("[%s] cci_hw_id=%d (0:EVT0,1:EVT1;2:EVT2)\n", __FUNCTION__, cci_hw_id);
        for( ii = 0 ; ii < (sizeof(aiOthersKey)/sizeof(int)) ; ii++)
        {
            Printhh("[%s] ii = %d, GPIO_pin = %d, name=%s\n", __FUNCTION__, ii, aiOthersKey[ii], asKeyStr[ii]);
            if (gpio_request(aiOthersKey[ii], asKeyStr[ii])){
                    PrintTip("[%s] gpio_request(%d, %s) fail \n", __FUNCTION__, aiOthersKey[ii], asKeyStr[ii]);
                    goto err_gpio_request_failed;
            }
            
            iRet = gpio_tlmm_config(GPIO_CFG(aiOthersKey[ii], 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP,
        				GPIO_CFG_2MA), GPIO_CFG_ENABLE);
            if (iRet){
                    PrintTip("[%s] Failed to configure GPIO(%d)  iRet= %d\n", __FUNCTION__, aiOthersKey[ii], iRet);
                    goto err_gpio_configure_failed;
            }

            sOKInfo[ii] = (SOthersKeyInfo *) kzalloc(sizeof(SOthersKeyInfo), GFP_KERNEL);
            #if 0   // error handling testing code
            if(ii == 1)
                sOKInfo[ii] = NULL;
            #endif 
            if (sOKInfo[ii] == NULL) {
                PrintTip("[%s]: allocate others_key_data failed\n", __func__);
                goto free_other_key;
            }
            
            sOKInfo[ii]->iGpio = aiOthersKey[ii];
            iRet = request_threaded_irq(MSM_GPIO_TO_INT(aiOthersKey[ii]), NULL,
                othersKey_irq_handler,
                IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                asKeyStr[ii],
                    sOKInfo[ii]);
            #if 0   // error handling testing code
            if(ii == 0)
                iRet = 1;
            #endif 
            if (iRet) {
                    PrintTip("[%s]: Unable to get slot IRQ(%d) (%d)\n", __FUNCTION__, MSM_GPIO_TO_INT(aiOthersKey[ii]), iRet);
                    goto err_request_irq_failed;
            }

        }
    }
//Henry_lin
    return;

//Henry_lin, 20110720, [BugID 508] Add error handler for some GPIO init.
//Henry_lin, 20110721, [BugID 510] Add debounce function for volume up and volume down GPIO.
#if 0
err_request_irq_failed2:
        kfree(sOKInfo[ii]);
err_gpio_configure_failed2:
free_other_key2:
        gpio_free(aiOthersKey22[ii]);
err_gpio_request_failed2:
        ;
    return;
#endif

err_request_irq_failed:
        kfree(sOKInfo[ii]);
err_gpio_configure_failed:
free_other_key:
        gpio_free(aiOthersKey[ii]);
err_gpio_request_failed:
        ;
	
}


static void s_vInitWakeUpKey(void)
{
    int ii;


    for(ii = 0 ; ii < MAX_WAKE_KEY_NUM ; ii++)
        g_iIrqCanWakeSys[ii] = -1;  // means not use


    g_iIrqCanWakeSys[0] = MSM_GPIO_TO_INT(CCI_HOME_KEY_GPIO);   // home key
    //g_iIrqCanWakeSys[1] = MSM_GPIO_TO_INT(CCI_CAMERA_AF_GPIO);   // camera focus key
    //g_iIrqCanWakeSys[2] = MSM_GPIO_TO_INT(CCI_CAMERA_CAPTURE_GPIO);   // camera cap key

//Henry_lin, 20110627, [BugID 357] Change GPIO for volume up/down.
    if(cci_hw_id >= EVT2)
    {
    }
    else{
        g_iIrqCanWakeSys[1] = MSM_GPIO_TO_INT(CCI_CAMERA_AF_GPIO);   // camera focus key
        g_iIrqCanWakeSys[2] = MSM_GPIO_TO_INT(CCI_CAMERA_CAPTURE_GPIO);   // camera cap key
    }
//Henry_lin

    //Pmic8058-pwrkey.c (drivers\input\misc) will use.

}


static int msm8x60_keypad_init(void)
{
    int iRetval = 0;

    PrintTip("[%s] Enter...\n", __FUNCTION__);

    s_vInitVirtualKey();
    
    s_vInitOthersKey();
    
    //Henry_lin, 20110504, [BugID 66] Add wake key Camera_focus, Camera_cap, and Home key to wake up system.
    s_vInitWakeUpKey();

    return iRetval;
}


static void msm8x60_keypad_exit(void)
{
    Printhh("[%s] Enter...\n", __FUNCTION__);

    return ;
}


#ifdef CONFIG_PM
static int msm8x60_gpio_suspend(struct device *dev)
{
    //struct sMsm8x60_gpio_data *psGpio = dev_get_drvdata(dev);
    int ii = 0;
    int rc = 0;


    Printhh("[%s] enter...\n", __FUNCTION__);
   
    for( ii = 0 ; ii < (sizeof(g_aiSupportKey)/sizeof(int)) ; ii++)
    {
        if( (g_aiSupportKey[ii] == CCI_HOME_KEY_GPIO)  && (g_iHomeKeyWakeUpEn==1))
            continue;
        disable_irq(g_aiSupportKeyIrq[ii]);

        //Henry_lin, 20110831, [BugID 766] Turn off virtual key IC power when enter suspend mode.
        if(cci_hw_id >= DVT1)
        {
            // home key "no wake up" (modify circuit)
            //PrintTip("[%s] cci_hw_id=%d (0:EVT0,1:EVT1;2:EVT2;3:DVT1)\n", __FUNCTION__, cci_hw_id);
            Printhh("[%s] gpio = %d output pull-down 2ma\n", __FUNCTION__, g_aiSupportKey[ii]);
            rc = gpio_tlmm_config(GPIO_CFG(g_aiSupportKey[ii], 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN,
                GPIO_CFG_2MA), GPIO_CFG_DISABLE);
            if (rc)
                PrintTip("[%s] Failed to configure GPIO %d\n", __FUNCTION__, rc);
        }
        else
        {
            // home key has "wake up", same as resume
            //PrintTip("[%s] cci_hw_id=%d (0:EVT0,1:EVT1;2:EVT2;3:DVT1)\n", __FUNCTION__, cci_hw_id);
            Printhh("[%s] gpio = %d input pull-up 2ma\n", __FUNCTION__, g_aiSupportKey[ii]);
            rc = gpio_tlmm_config(GPIO_CFG(g_aiSupportKey[ii], 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP,
                GPIO_CFG_2MA), GPIO_CFG_DISABLE);
            if (rc)
                PrintTip("[%s] Failed to configure GPIO %d\n", __FUNCTION__, rc);
        }
    }

    rc = gpio_tlmm_config(GPIO_CFG(CCI_CAP_KEY_RST_GPIO, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL,
        GPIO_CFG_2MA), GPIO_CFG_DISABLE);
    if (rc)
        PrintTip("[%s] Failed to configure GPIO %d\n", __FUNCTION__, rc);
        

    if(cci_hw_id >= DVT1)
    {
        //home key "no wake up" (modify circuit)
        Printhh("[%s] CCI_CAP_KEY_RST_GPIO = 1(high)\n", __FUNCTION__);
        gpio_direction_output(CCI_CAP_KEY_RST_GPIO, 1);
    }
    else
    {
        Printhh("[%s] CCI_CAP_KEY_RST_GPIO = 0(low)\n", __FUNCTION__);
        gpio_direction_output(CCI_CAP_KEY_RST_GPIO, 0);
    }

    return 0;
}


static int msm8x60_gpio_resume(struct device *dev)
{
    //struct sMsm8x60_gpio_data *psGpio = dev_get_drvdata(dev);
    int ii = 0;
    int rc = 0;
#if 0
    unsigned usGetData;
#endif


    Printhh("[%s] enter...\n", __FUNCTION__);
    //Henry_lin, 20111030, [BugID 1175] Ignore key event once when system resume.
#if 0
    for( ii = 0 ; ii < (sizeof(g_aiSupportKey)/sizeof(int)) ; ii++)
    {
        //Printhh("[%s] Set GPIO(%d) enable\n", __FUNCTION__, g_aiSupportKey[ii]);
        //gpio_tlmm_config_read(g_aiSupportKey[ii], &usGetData);
        //Printhh("[%s] bef gpio_tlmm_config = %#x\n", __FUNCTION__, usGetData);
        rc = gpio_tlmm_config(GPIO_CFG(g_aiSupportKey[ii], 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP,
            GPIO_CFG_2MA), GPIO_CFG_ENABLE);
        if (rc)
            PrintTip("[%s] Failed to configure GPIO %d\n", __FUNCTION__, rc);

        //gpio_tlmm_config_read(g_aiSupportKey[ii], &usGetData);
        //Printhh("[%s] aft gpio_tlmm_config = %#x\n", __FUNCTION__, usGetData);

        //Printhh("[%s] g_aiSupportKeyIrq[%d]=%d...\n", __FUNCTION__, ii, g_aiSupportKeyIrq[ii]);
        if( (g_aiSupportKey[ii] == CCI_HOME_KEY_GPIO)  && (g_iHomeKeyWakeUpEn==1))
            continue;
        enable_irq(g_aiSupportKeyIrq[ii]);
    }
#endif

    rc = gpio_tlmm_config(GPIO_CFG(CCI_CAP_KEY_RST_GPIO, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL,
        GPIO_CFG_2MA), GPIO_CFG_ENABLE);
    if (rc)
        PrintTip("[%s] Failed to configure GPIO %d\n", __FUNCTION__, rc);
    gpio_direction_output(CCI_CAP_KEY_RST_GPIO, 0);


    //Henry_lin, 20111030, [BugID 1175] Ignore key event once when system resume.
#if 1
    for( ii = 0 ; ii < (sizeof(g_aiSupportKey)/sizeof(int)) ; ii++)
    {
        //Printhh("[%s] Set GPIO(%d) enable\n", __FUNCTION__, g_aiSupportKey[ii]);
        //gpio_tlmm_config_read(g_aiSupportKey[ii], &usGetData);
        //Printhh("[%s] bef gpio_tlmm_config = %#x\n", __FUNCTION__, usGetData);
        rc = gpio_tlmm_config(GPIO_CFG(g_aiSupportKey[ii], 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP,
            GPIO_CFG_2MA), GPIO_CFG_ENABLE);
        if (rc)
            PrintTip("[%s] Failed to configure GPIO %d\n", __FUNCTION__, rc);

        //gpio_tlmm_config_read(g_aiSupportKey[ii], &usGetData);
        //Printhh("[%s] aft gpio_tlmm_config = %#x\n", __FUNCTION__, usGetData);
        //Printhh("[%s] g_aiSupportKeyIrq[%d]=%d...\n", __FUNCTION__, ii, g_aiSupportKeyIrq[ii]);
        if( (g_aiSupportKey[ii] == CCI_HOME_KEY_GPIO)  && (g_iHomeKeyWakeUpEn==1))
            continue;
            
        if(cci_hw_id >= DVT1)
            g_iIrqReason = IRQ_REASON_RESUME;
            
        enable_irq(g_aiSupportKeyIrq[ii]);
    }
#endif

    return 0;
}


static struct dev_pm_ops msm8x60_gpio_pm_ops = {
	.suspend	= msm8x60_gpio_suspend,
	.resume		= msm8x60_gpio_resume,
};
#endif

#if 1   //for virtual key reset while they cann't work.
//Henry_lin, 20110824, [BugID 712] Reset virtual key IC while they can't work.
static void msm8x60_gpio_reset_vk(struct work_struct *work)
{

    Printhh("[%s] enter... \n", __FUNCTION__);


    gpio_direction_output(CCI_CAP_KEY_RST_GPIO, 1);
    mdelay(5);
    gpio_direction_output(CCI_CAP_KEY_RST_GPIO, 0);
}
#endif


//Henry_lin, 20110826, [BugID 736] Supply early suspend function for virtual key.
#ifdef CONFIG_HAS_EARLYSUSPEND
static void msm8x60_gpio_early_suspend(struct early_suspend *h)
{
    struct sMsm8x60_gpio_data *psGpioData;


    psGpioData = container_of(h, struct sMsm8x60_gpio_data, early_suspend);
    //PrintTip("[%s] Enter...\n", __FUNCTION__);
    msm8x60_gpio_suspend(psGpioData->pDev);
}

static void msm8x60_gpio_late_resume(struct early_suspend *h)
{
    struct sMsm8x60_gpio_data *psGpioData;


    psGpioData = container_of(h, struct sMsm8x60_gpio_data, early_suspend);
    //PrintTip("[%s] Enter...\n", __FUNCTION__);
    msm8x60_gpio_resume(psGpioData->pDev);
}
#endif


static int __devinit msm8x60_gpio_probe(struct platform_device *pdev)
{
    struct sMsm8x60_gpio_data *psGpioData;
    int ret = 0;


    Printhh("[%s] enter...\n", __FUNCTION__);

    psGpioData = kzalloc(sizeof(struct sMsm8x60_gpio_data), GFP_KERNEL);
    if (psGpioData == NULL) {
        printk(KERN_ERR"%s: allocate sMsm8x60_gpio_data failed\n", __func__);
        ret = -ENOMEM;
        goto err_alloc_data_failed;
    }
    psGpioData->pDev= &pdev->dev;

    INIT_WORK(&g_sWork, msm8x60_gpio_reset_vk);
    msm8x60_keypad_init();

    //Henry_lin, 20110826, [BugID 736] Supply early suspend function for virtual key.
#ifdef CONFIG_HAS_EARLYSUSPEND
    psGpioData->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;   //EARLY_SUSPEND_LEVEL_STOP_DRAWING or EARLY_SUSPEND_LEVEL_DISABLE_FB ...
    psGpioData->early_suspend.suspend = msm8x60_gpio_early_suspend;
    psGpioData->early_suspend.resume = msm8x60_gpio_late_resume;
    register_early_suspend(&psGpioData->early_suspend);
#endif

    platform_set_drvdata(pdev, psGpioData);

err_alloc_data_failed:
    return ret;
}


static int __devexit msm8x60_gpio_remove(struct platform_device *pdev)
{
    struct sMsm8x60_gpio_data *psGpioData = platform_get_drvdata(pdev);


    Printhh("[%s] enter...\n", __FUNCTION__);
    msm8x60_keypad_exit();
    //Henry_lin, 20110826, [BugID 736] Supply early suspend function for virtual key.
    unregister_early_suspend(&psGpioData->early_suspend);
    kfree(psGpioData);

    return 0;
}


static struct platform_driver msm8x60_gpio_driver = {
	.probe		= msm8x60_gpio_probe,
	.remove		= __devexit_p(msm8x60_gpio_remove),
	.driver		= {
		.name	= "msm8x60-gpio",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &msm8x60_gpio_pm_ops,
#endif
	},
};

static int __init msm8x60_gpio_init(void)
{
    Printhh("[%s] enter...\n", __FUNCTION__);
    return platform_driver_register(&msm8x60_gpio_driver);
}
module_init(msm8x60_gpio_init);

static void __exit msm8x60_gpio_exit(void)
{
    Printhh("[%s] enter...\n", __FUNCTION__);
    platform_driver_unregister(&msm8x60_gpio_driver);
}
module_exit(msm8x60_gpio_exit);


MODULE_AUTHOR("CCI");
MODULE_DESCRIPTION("MSM8x60 keypad driver");
MODULE_LICENSE("GPL v2");


