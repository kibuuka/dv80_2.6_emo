/*
 * Bluetooth PCM test
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#define ENTER_PCM_TEST_MODE 0x10

static uint32_t auxpcm_gpio_table[] = {
	GPIO_CFG(111, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),
	GPIO_CFG(112, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),
	GPIO_CFG(113, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),
	GPIO_CFG(114, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),
};

static void msm_auxpcm_init(void)
{
	int i;
	for(i = 0; i < 4; ++i)
		gpio_tlmm_config(auxpcm_gpio_table[i], GPIO_CFG_ENABLE);				
}

static int do_pcm_test(const char *val, struct kernel_param *kp)
{
	char param;
	param = val[0];
	if(param & ENTER_PCM_TEST_MODE) {
		printk("<0>""Init pcm status \n");
		msm_auxpcm_init();
		goto out;
	}
	else {
		printk("<0>""get gpio(%d) = %d \n", 111 + (0xf & param), gpio_get_value(111 + (0xf & param)));
		return gpio_get_value(111 + (0xf & param)) ? 0 : -1;
	}
out:	
	return 0;
}
module_param_call(pcm, do_pcm_test, param_get_bool,
		  NULL, S_IWUSR | S_IRUGO);


