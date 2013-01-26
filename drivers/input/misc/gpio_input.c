/* drivers/input/misc/gpio_input.c
 *
 * Copyright (C) 2007 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/gpio_event.h>
#include <linux/hrtimer.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/wakelock.h>

#if 1   //henry test mute event
#include <linux/switch.h>
#endif

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
/*---------------------  Static Classes  ----------------------------*/


enum {
	DEBOUNCE_UNSTABLE     = BIT(0),	/* Got irq, while debouncing */
	DEBOUNCE_PRESSED      = BIT(1),
	DEBOUNCE_NOTPRESSED   = BIT(2),
	DEBOUNCE_WAIT_IRQ     = BIT(3),	/* Stable irq state */
	DEBOUNCE_POLL         = BIT(4),	/* Stable polling state */

	DEBOUNCE_UNKNOWN =
		DEBOUNCE_PRESSED | DEBOUNCE_NOTPRESSED,
};

struct gpio_key_state {
	struct gpio_input_state *ds;
	uint8_t debounce;
};

struct gpio_input_state {
	struct gpio_event_input_devs *input_devs;
	const struct gpio_event_input_info *info;
	struct hrtimer timer;
	int use_irq;
	int debounce_count;
	spinlock_t irq_lock;
	struct wake_lock wake_lock;
	//Decken_Kang, 20111130, [BugID 1489] When suspending, the mute key can��t work normally.
	struct wake_lock mutekey_wake_lock;
	struct gpio_key_state key_state[0];
};

#if 1   //henry test mute event
//Henry_lin, 20110511, [BugID 86] Implement Ringer Switch.
struct switch_dev g_sSwDev;
//Henry_lin
#endif


//Henry_lin, 20110926, [BugID 949] Use ringer switch work queue to notify event.
struct workqueue_struct *g_psKeyWq;
struct work_struct g_sKeyWork;
int g_iPress;

//Decken_Kang, 20111130, [BugID 1489] When suspending, the mute key can��t work normally.
#define CCI_MUTE_RING_GPIO 34
unsigned int g_mutekeyIrq;
//Decken_Kang


static enum hrtimer_restart gpio_event_input_timer_func(struct hrtimer *timer)
{
	int i;
	int pressed;
	struct gpio_input_state *ds =
		container_of(timer, struct gpio_input_state, timer);
	unsigned gpio_flags = ds->info->flags;
	unsigned npolarity;
	int nkeys = ds->info->keymap_size;
	const struct gpio_event_direct_entry *key_entry;
	struct gpio_key_state *key_state;
	unsigned long irqflags;
	uint8_t debounce;

	//Printhh("[%s] enter...\n", __FUNCTION__);

#if 0
	key_entry = kp->keys_info->keymap;
	key_state = kp->key_state;
	for (i = 0; i < nkeys; i++, key_entry++, key_state++)
		pr_info("gpio_read_detect_status %d %d\n", key_entry->gpio,
			gpio_read_detect_status(key_entry->gpio));
#endif
	key_entry = ds->info->keymap;
	key_state = ds->key_state;
	spin_lock_irqsave(&ds->irq_lock, irqflags);
	for (i = 0; i < nkeys; i++, key_entry++, key_state++) {
		debounce = key_state->debounce;
		if (debounce & DEBOUNCE_WAIT_IRQ)
			continue;
		if (key_state->debounce & DEBOUNCE_UNSTABLE) {
			debounce = key_state->debounce = DEBOUNCE_UNKNOWN;
			enable_irq(gpio_to_irq(key_entry->gpio));
			pr_info("gpio_keys_scan_keys: key %#x-%#x, %d "
				"(%d) continue debounce\n",
				ds->info->type, key_entry->code,
				i, key_entry->gpio);
		}
		npolarity = !(gpio_flags & GPIOEDF_ACTIVE_HIGH);
		pressed = gpio_get_value(key_entry->gpio) ^ npolarity;
		if (debounce & DEBOUNCE_POLL) {
			if (pressed == !(debounce & DEBOUNCE_PRESSED)) {
				ds->debounce_count++;
				key_state->debounce = DEBOUNCE_UNKNOWN;
				if (gpio_flags & GPIOEDF_PRINT_KEY_DEBOUNCE)
					pr_info("gpio_keys_scan_keys: key %#x-"
						"%#x, %d (%d) start debounce\n",
						ds->info->type, key_entry->code,
						i, key_entry->gpio);
			}
			continue;
		}
		if (pressed && (debounce & DEBOUNCE_NOTPRESSED)) {
			if (gpio_flags & GPIOEDF_PRINT_KEY_DEBOUNCE)
				pr_info("gpio_keys_scan_keys: key %#x-%#x, %d "
					"(%d) debounce pressed 1\n",
					ds->info->type, key_entry->code,
					i, key_entry->gpio);
			key_state->debounce = DEBOUNCE_PRESSED;
			continue;
		}
		if (!pressed && (debounce & DEBOUNCE_PRESSED)) {
			if (gpio_flags & GPIOEDF_PRINT_KEY_DEBOUNCE)
				pr_info("gpio_keys_scan_keys: key %#x-%#x, %d "
					"(%d) debounce pressed 0\n",
					ds->info->type, key_entry->code,
					i, key_entry->gpio);
			key_state->debounce = DEBOUNCE_NOTPRESSED;
			continue;
		}
		/* key is stable */
		//Printhh("[%s] ds->debounce_count--\n", __FUNCTION__);
		ds->debounce_count--;
		if (ds->use_irq)
			key_state->debounce |= DEBOUNCE_WAIT_IRQ;
		else
			key_state->debounce |= DEBOUNCE_POLL;
		if (gpio_flags & GPIOEDF_PRINT_KEYS)
			pr_info("gpio_keys_scan_keys: key %#x-%#x, %d (%d) "
				"changed to %d\n", ds->info->type,
				key_entry->code, i, key_entry->gpio, pressed);
#if 1   //henry test mute event
		//Henry_lin, 20110511, [BugID 86] Implement Ringer Switch.
		//Henry_lin, 20110721, [BugID 510] Add debounce function for volume up and volume down GPIO.
		#if 1
		if(key_entry->code != KEY_MUTE)
		{
        		input_event(ds->input_devs->dev[key_entry->dev], ds->info->type,
        			    key_entry->code, pressed);
		}
		else
		{
        		//Henry_lin, 20110926, [BugID 949] Use ringer switch work queue to notify event.
        		g_iPress = pressed;
        		Printhh("[%s] call queue_work()\n", __FUNCTION__);
			//Decken_Kang, 20111130, [BugID 1489] When suspending, the mute key can��t work normally.
			wake_lock_timeout(&ds->mutekey_wake_lock, HZ);
        		queue_work(g_psKeyWq, &g_sKeyWork);    //==s_vKeyWorkFunc()
        		//switch_set_state(&g_sSwDev, pressed);
        		input_report_switch(ds->input_devs->dev[key_entry->dev], SW_MUTE_RING, pressed);
		}
		#endif
		//Henry_lin
#endif
			    
	}

#if 0
	key_entry = kp->keys_info->keymap;
	key_state = kp->key_state;
	for (i = 0; i < nkeys; i++, key_entry++, key_state++) {
		pr_info("gpio_read_detect_status %d %d\n", key_entry->gpio,
			gpio_read_detect_status(key_entry->gpio));
	}
#endif

	//Printhh("[%s] ds->debounce_count = %d\n", __FUNCTION__, ds->debounce_count);
	if (ds->debounce_count)
		hrtimer_start(timer, ds->info->debounce_time, HRTIMER_MODE_REL);
	else if (!ds->use_irq)
		hrtimer_start(timer, ds->info->poll_time, HRTIMER_MODE_REL);
	else
		wake_unlock(&ds->wake_lock);

	spin_unlock_irqrestore(&ds->irq_lock, irqflags);

	return HRTIMER_NORESTART;
}

static irqreturn_t gpio_event_input_irq_handler(int irq, void *dev_id)
{
	struct gpio_key_state *ks = dev_id;
	struct gpio_input_state *ds = ks->ds;
	int keymap_index = ks - ds->key_state;
	const struct gpio_event_direct_entry *key_entry;
	unsigned long irqflags;
	int pressed;


	//Printhh("[%s] enter...\n", __FUNCTION__);
	if (!ds->use_irq)
		return IRQ_HANDLED;

	key_entry = &ds->info->keymap[keymap_index];

	//Printhh("[%s] ds->info->debounce_time.tv64 = %lld\n", __FUNCTION__, ds->info->debounce_time.tv64);
	if (ds->info->debounce_time.tv64) {
		spin_lock_irqsave(&ds->irq_lock, irqflags);
		//Printhh("[%s] ks->debounce = %#x\n", __FUNCTION__, ks->debounce);
		if (ks->debounce & DEBOUNCE_WAIT_IRQ) {
			ks->debounce = DEBOUNCE_UNKNOWN;
			if (ds->debounce_count++ == 0) {
				wake_lock(&ds->wake_lock);
				//Printhh("[%s] call hrtimer_start()\n", __FUNCTION__);
				hrtimer_start(
					&ds->timer, ds->info->debounce_time,
					HRTIMER_MODE_REL);  // == gpio_event_input_timer_func()
			}
			if (ds->info->flags & GPIOEDF_PRINT_KEY_DEBOUNCE)
				pr_info("gpio_event_input_irq_handler: "
					"key %#x-%#x, %d (%d) start debounce\n",
					ds->info->type, key_entry->code,
					keymap_index, key_entry->gpio);
		} else {
			disable_irq_nosync(irq);
			ks->debounce = DEBOUNCE_UNSTABLE;
		}
		spin_unlock_irqrestore(&ds->irq_lock, irqflags);
	} else {
		pressed = gpio_get_value(key_entry->gpio) ^
			!(ds->info->flags & GPIOEDF_ACTIVE_HIGH);
		if (ds->info->flags & GPIOEDF_PRINT_KEYS)
			pr_info("gpio_event_input_irq_handler: key %#x-%#x, %d "
				"(%d) changed to %d\n",
				ds->info->type, key_entry->code, keymap_index,
				key_entry->gpio, pressed);
		input_event(ds->input_devs->dev[key_entry->dev], ds->info->type,
			    key_entry->code, pressed);
	}
	return IRQ_HANDLED;
}

static int gpio_event_input_request_irqs(struct gpio_input_state *ds)
{
	int i;
	int err;
	unsigned int irq;
	unsigned long req_flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;


	//Printhh("[%s] enter...\n", __FUNCTION__);
	for (i = 0; i < ds->info->keymap_size; i++) {
		err = irq = gpio_to_irq(ds->info->keymap[i].gpio);  // KEY_P!

		//Decken_Kang, 20111130, [BugID 1489] When suspending, the mute key can��t work normally.
		if(ds->info->keymap[i].gpio == CCI_MUTE_RING_GPIO)
			g_mutekeyIrq = irq;
		//Decken_Kang
		
		if (err < 0)
			goto err_gpio_get_irq_num_failed;
		err = request_irq(irq, gpio_event_input_irq_handler,
				  req_flags, "gpio_keys", &ds->key_state[i]);   // KEY_P!
		if (err) {
			pr_err("gpio_event_input_request_irqs: request_irq "
				"failed for input %d, irq %d\n",
				ds->info->keymap[i].gpio, irq);
			goto err_request_irq_failed;
		}
		enable_irq_wake(irq);   // trigger first time interrupt?
	}
	return 0;

	for (i = ds->info->keymap_size - 1; i >= 0; i--) {
		free_irq(gpio_to_irq(ds->info->keymap[i].gpio),
			 &ds->key_state[i]);
err_request_irq_failed:
err_gpio_get_irq_num_failed:
		;
	}
	return err;
}


#if 1   //henry test mute event
//Henry_lin, 20110511, [BugID 86] Implement Ringer Switch.
static ssize_t s_iMute_print_name(struct switch_dev *sdev, char *buf)
{
	switch (switch_get_state(sdev)) {
	//case OTHC_NO_DEVICE:
	case 0:
		return sprintf(buf, "Ring Mode\n");
	//case OTHC_HEADSET:
	case 1:
		return sprintf(buf, "Mute Mode\n");
	}
	return -EINVAL;
}
#endif


//Henry_lin, 20110926, [BugID 949] Use ringer switch work queue to notify event.
static void s_vKeyWorkFunc(struct work_struct *work)
{

    Printhh("[%s] call switch_set_state(g_iPress=%d)\n", __FUNCTION__, g_iPress);
    switch_set_state(&g_sSwDev, g_iPress);
}


int gpio_event_input_func(struct gpio_event_input_devs *input_devs,
			struct gpio_event_info *info, void **data, int func)
{
    int ret;
    int i;
    unsigned long irqflags;
    struct gpio_event_input_info *di;
    struct gpio_input_state *ds = *data;
    int rc;


#if 1   //henry test mute event
    //Henry_lin, 20110511, [BugID 86] Implement Ringer Switch.
    g_sSwDev.name = "mute_sw";  // == mute/ring switch
    g_sSwDev.print_name = s_iMute_print_name;

    Printhh("[%s] call switch_dev_register()\n", __FUNCTION__);
    rc = switch_dev_register(&g_sSwDev);
    if (rc) {
        PrintTip("[%s] Unable to register switch device\n", __FUNCTION__);
        return rc;
    }
    //Henry_lin
#endif


	//Printhh("[%s] enter... func=%d\n", __FUNCTION__, func);
	di = container_of(info, struct gpio_event_input_info, info);

	if (func == GPIO_EVENT_FUNC_SUSPEND) {
		if (ds->use_irq)
			for (i = 0; i < di->keymap_size; i++)
				disable_irq(gpio_to_irq(di->keymap[i].gpio));
		hrtimer_cancel(&ds->timer);
		return 0;
	}
	if (func == GPIO_EVENT_FUNC_RESUME) {
		spin_lock_irqsave(&ds->irq_lock, irqflags);
		if (ds->use_irq)
			for (i = 0; i < di->keymap_size; i++)
				enable_irq(gpio_to_irq(di->keymap[i].gpio));
		hrtimer_start(&ds->timer, ktime_set(0, 0), HRTIMER_MODE_REL);
		spin_unlock_irqrestore(&ds->irq_lock, irqflags);
		return 0;
	}

	if (func == GPIO_EVENT_FUNC_INIT) { // KEY_P!
		if (ktime_to_ns(di->poll_time) <= 0)
			di->poll_time = ktime_set(0, 20 * NSEC_PER_MSEC);

		*data = ds = kzalloc(sizeof(*ds) + sizeof(ds->key_state[0]) *
					di->keymap_size, GFP_KERNEL);
		if (ds == NULL) {
			ret = -ENOMEM;
			pr_err("gpio_event_input_func: "
				"Failed to allocate private data\n");
			goto err_ds_alloc_failed;
		}
		ds->debounce_count = di->keymap_size;
		ds->input_devs = input_devs;    // KEY_P!
		ds->info = di;
		wake_lock_init(&ds->wake_lock, WAKE_LOCK_SUSPEND, "gpio_input");
		//Decken_Kang, 20111130, [BugID 1489] When suspending, the mute key can��t work normally.
		wake_lock_init(&ds->mutekey_wake_lock, WAKE_LOCK_SUSPEND, "mutekey_gpio_input");
		spin_lock_init(&ds->irq_lock);

		for (i = 0; i < di->keymap_size; i++) {
			int dev = di->keymap[i].dev;
			if (dev >= input_devs->count) {
				pr_err("gpio_event_input_func: bad device "
					"index %d >= %d for key code %d\n",
					dev, input_devs->count,
					di->keymap[i].code);
				ret = -EINVAL;
				goto err_bad_keymap;
			}
			input_set_capability(input_devs->dev[dev], di->type,
					     di->keymap[i].code);   // KEY_P!
			ds->key_state[i].ds = ds;
			ds->key_state[i].debounce = DEBOUNCE_UNKNOWN;
#if 1   //henry test mute event
			//Henry_lin, 20110511, [BugID 86] Implement Ringer Switch.
			input_set_capability(input_devs->dev[dev], EV_SW, SW_MUTE_RING);
			//Henry_lin
#endif
		}
		for (i = 0; i < di->keymap_size; i++) {
			ret = gpio_request(di->keymap[i].gpio, "gpio_kp_in");
			if (ret) {
				pr_err("gpio_event_input_func: gpio_request "
					"failed for %d\n", di->keymap[i].gpio);
				goto err_gpio_request_failed;
			}
			ret = gpio_direction_input(di->keymap[i].gpio);
			if (ret) {
				pr_err("gpio_event_input_func: "
					"gpio_direction_input failed for %d\n",
					di->keymap[i].gpio);
				goto err_gpio_configure_failed;
			}
		}

		ret = gpio_event_input_request_irqs(ds);

		//Henry_lin, 20110926, [BugID 949] Use ringer switch work queue to notify event.
		g_psKeyWq = create_rt_workqueue("switch_key_rtwq");
		if (!g_psKeyWq) {
        		printk(KERN_ERR"%s: create rt workqueue failed\n", __func__);
        		ret = -ENOMEM;
        		goto err_create_rtwq_failed;
		}
		INIT_WORK(&g_sKeyWork, s_vKeyWorkFunc);

		spin_lock_irqsave(&ds->irq_lock, irqflags);
		ds->use_irq = ret == 0;

		pr_info("GPIO Input Driver: Start gpio inputs for %s%s in %s "
			"mode\n", input_devs->dev[0]->name,
			(input_devs->count > 1) ? "..." : "",
			ret == 0 ? "interrupt" : "polling");

		hrtimer_init(&ds->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ds->timer.function = gpio_event_input_timer_func;
		hrtimer_start(&ds->timer, ktime_set(0, 0), HRTIMER_MODE_REL);
		spin_unlock_irqrestore(&ds->irq_lock, irqflags);
		return 0;
	}

	ret = 0;
	spin_lock_irqsave(&ds->irq_lock, irqflags);
	hrtimer_cancel(&ds->timer);
	if (ds->use_irq) {
		for (i = di->keymap_size - 1; i >= 0; i--) {
			free_irq(gpio_to_irq(di->keymap[i].gpio),
				 &ds->key_state[i]);
		}
	}
	spin_unlock_irqrestore(&ds->irq_lock, irqflags);

	for (i = di->keymap_size - 1; i >= 0; i--) {
err_gpio_configure_failed:
		gpio_free(di->keymap[i].gpio);
err_gpio_request_failed:
		;
	}
err_bad_keymap:
	wake_lock_destroy(&ds->wake_lock);
	//Decken_Kang, 20111130, [BugID 1489] When suspending, the mute key can��t work normally.
	wake_lock_destroy(&ds->mutekey_wake_lock);
	kfree(ds);
err_ds_alloc_failed:

//Henry_lin, 20110926, [BugID 949] Use ringer switch work queue to notify event.
err_create_rtwq_failed:

	return ret;
}