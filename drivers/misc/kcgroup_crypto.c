/*
 *  kcgroup_crypto.c: kernel driver for ecryptfs key & signature operations.
 */

#include <asm/uaccess.h>
#include <linux/crypto.h>
#include <linux/fs.h>
#include <linux/key.h>
#include <linux/keyctl.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/scatterlist.h>
#include <linux/syscalls.h>

#include <kcgroup_crypto.h>

/*
 * Global variables.
 */
static bool
kcgroup_crypto_flag = false;
static char
sig[KCGROUP_MAX_SIG_SIZE] = { 0, };

static int
kcgroup_crypto_open(struct inode *inode, struct file *file)
{
	if (kcgroup_crypto_flag)
		return -EBUSY;

	kcgroup_crypto_flag = true;
	try_module_get(THIS_MODULE);
	return 0;
}

static int
kcgroup_crypto_release(struct inode *inode, struct file *file)
{
	kcgroup_crypto_flag = false;
	module_put(THIS_MODULE);
	return 0;
}

static bool
kcgroup_crypto_validate(void)
{
	if ( !current->comm || strcmp(current->comm, KCGROUP_OWNER) ) {
			printk(KERN_ERR
			"%s invalid owner!\n",
			__func__);
		memset(sig, 0, KCGROUP_MAX_SIG_SIZE);
		return false;
	}

	return true;
}

static void
kcgroup_crypto_hex(char *hex, char *data, int len)
{
	/* hex is pre-allocated buffer in size of len */
	int i;
	for (i=0; i < len; i++) {
		unsigned char c = data[i];
		*hex++ = '0' + ((c&0xf0)>>4) + (c>=0xa0)*('a'-'9'-1);
		*hex++ = '0' + (c&0x0f) + ((c&0x0f)>=0x0a)*('a'-'9'-1);
	}
	hex[len] = '\0';
}

static int
kcgroup_crypto_md5(char *dst, u8 *src, int len)
{
	struct hash_desc desc;
	struct scatterlist sg;
	char *digest;
	int rc = 0;

	sg_init_one(&sg, src, len);

	desc.flags = CRYPTO_TFM_REQ_MAY_SLEEP;
	desc.tfm = crypto_alloc_hash(ECRYPTFS_DEFAULT_HASH, 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(desc.tfm))
		return -ENOMEM;

	rc = crypto_hash_init(&desc);
	if (rc) {
		printk(KERN_ERR
			   "%s: Error initializing crypto hash; rc = (%d)\n",
			   __func__, rc);
		return rc;
	}

	rc = crypto_hash_update(&desc, &sg, len);
	if (rc) {
		printk(KERN_ERR
			   "%s: Error updating crypto hash; rc = (%d)\n",
			   __func__, rc);
		return rc;
	}

	digest = kmalloc(KCGROUP_MAX_SIG_SIZE, GFP_KERNEL);
	if (!digest) {
		printk(KERN_ERR
			"%s out of memory.\n",
			__func__);
		rc = -ENOMEM;
		return rc;
	}

	rc = crypto_hash_final(&desc, digest);
	if (rc) {
		kfree(digest);
		printk(KERN_ERR
			   "%s: Error finalizing crypto hash; rc = (%d)\n",
			   __func__, rc);
		return rc;
	}

	kcgroup_crypto_hex(dst, digest, ECRYPTFS_DEFAULT_KEY_BYTES);
	dst[ECRYPTFS_DEFAULT_KEY_BYTES] = 0;
	kfree(digest);
	return rc;
}

static ssize_t
kcgroup_crypto_read(struct file *filp, char __user *buff,
		size_t len, loff_t * offset)
{
	int size = 0;
	char *out;

	if ( !kcgroup_crypto_validate() ) {
		printk(KERN_ERR
			"%s invalid request.\n",
			__func__);
		return -EPERM;
	}

	size = ECRYPTFS_DEFAULT_KEY_BYTES + 2;
	if ( size > len ) {
		printk(KERN_ERR
			"%s buff length is too small",
			__func__);
		return -EINVAL;
	}

    out = kmalloc(size, GFP_KERNEL);
    if (!out) {
        printk(KERN_ERR
            "%s out of memory.\n",
            __func__);
        return -ENOMEM;
    }

	snprintf(out, size, "%c%s", size - 2, sig);
	if ( copy_to_user(buff, out, size) ) {
		kfree(out);
		return -EFAULT;
    }

	kfree(out);
	return size;
}

static ssize_t
kcgroup_crypto_write(struct file *filp, const char __user *buff,
		size_t len, loff_t * off)
{
	char *key, *fekek, *hash;
	struct key_type *ktype;
	key_ref_t keyring_ref, key_ref;
	struct ecryptfs_auth_tok  auth_tok;
	int key_len = 0, rc = 0;

	memset(&auth_tok, 0, sizeof(struct ecryptfs_auth_tok));
	if ( !kcgroup_crypto_validate() ) {
		printk(KERN_ERR
			"%s invalid request.\n",
			__func__);
		return -EPERM;
	}

	key_len = len - 3;
	if ( key_len * 2 > KCGROUP_MAX_SIG_SIZE ) {
		printk(KERN_ERR
			"%s unsupported key size.\n",
			__func__);
		return -EINVAL;
	}

	key = kmalloc(KCGROUP_MAX_KEY_SIZE, GFP_KERNEL);
	if (!key) {
		printk(KERN_ERR
			"%s out of memory.\n",
			__func__);
		return -ENOMEM;
	}

	if ( copy_from_user(key, buff + 2, key_len) ) {
		printk(KERN_ERR
			"%s copy_from_user failed.\n",
			__func__);
		rc = -EFAULT;
		goto error1;
	}

	fekek = kmalloc(KCGROUP_MAX_SIG_SIZE, GFP_KERNEL);
	if (!fekek) {
		printk(KERN_ERR
			"%s out of memory.\n",
			__func__);
		rc = -ENOMEM;
		goto error1;
	}

	if ( kcgroup_crypto_md5(fekek, key, key_len) ) {
		printk(KERN_ERR
			"%s invalid data.\n",
			__func__);
		rc = -EINVAL;
		goto error2;
	}

	hash = kmalloc(KCGROUP_MAX_SIG_SIZE, GFP_KERNEL);
	if (!hash) {
		printk(KERN_ERR
			"%s out of memory.\n",
			__func__);
		rc = -ENOMEM;
		goto error2;
	}

	if ( kcgroup_crypto_md5(hash, fekek, ECRYPTFS_DEFAULT_KEY_BYTES) ) {
		printk(KERN_ERR
			"%s invalid fekek.\n",
			__func__);
		rc = -EINVAL;
		goto error3;
	}

	/* get the keyring at which to begin the search */
	keyring_ref = lookup_user_key(KEY_SPEC_USER_KEYRING, 0, KEY_SEARCH);
	if (IS_ERR(keyring_ref)) {
		rc = (int)PTR_ERR(keyring_ref);
		goto error3;
	}

	ktype = key_type_lookup(KCGROUP_KEYRING_TYPE);
	if (IS_ERR(ktype)) {
		rc = (int)PTR_ERR(ktype);
		goto error4;
	}

	key_ref = keyring_search(keyring_ref, ktype, hash);
	if (!IS_ERR(key_ref)) {
		printk(KERN_INFO
				"%s key & sig pair inserted alreay [%s]\n",
				__func__, hash);
		strncpy(sig, hash, ECRYPTFS_DEFAULT_KEY_BYTES);
		goto error5;
	}

	auth_tok.version = (((uint16_t)(ECRYPTFS_VERSION_MAJOR << 8) & 0xFF00) |
			((uint16_t)ECRYPTFS_VERSION_MINOR & 0x00FF));
	auth_tok.token_type = ECRYPTFS_PASSWORD;
	memcpy(auth_tok.token.password.salt, KCGROUP_SALT, 
			ECRYPTFS_SALT_SIZE);
	memcpy(auth_tok.token.password.session_key_encryption_key, fekek,
			ECRYPTFS_DEFAULT_KEY_BYTES);
	auth_tok.token.password.session_key_encryption_key_bytes =
			ECRYPTFS_DEFAULT_KEY_BYTES;
	auth_tok.token.password.flags |=
			ECRYPTFS_SESSION_KEY_ENCRYPTION_KEY_SET;
	auth_tok.token.password.flags &=
			~(ECRYPTFS_PERSISTENT_PASSWORD);
	strncpy((char *)auth_tok.token.password.signature, hash, 
			ECRYPTFS_DEFAULT_KEY_BYTES);

	/* add key to the target keyring */
	key_ref_put(keyring_ref);
	keyring_ref = lookup_user_key(KEY_SPEC_USER_KEYRING,
			KEY_LOOKUP_CREATE, KEY_WRITE);
	if (IS_ERR(keyring_ref)) {
		printk(KERN_ALERT
				"%s failed to get keying to write.\n",
				__func__);
		rc = (int)PTR_ERR(keyring_ref);
		goto error4;
	}

	key_ref = key_create_or_update(keyring_ref,
				KCGROUP_KEYRING_TYPE, hash,
				&auth_tok, sizeof(struct ecryptfs_auth_tok),
				KEY_PERM_UNDEF, KEY_ALLOC_IN_QUOTA);
	if (IS_ERR(key_ref)) {
		rc = (int)PTR_ERR(key_ref);
		printk(KERN_ERR
				"%s failed to add key & sig pair (%d).\n",
				__func__, rc);
		goto error5;
	}

	rc = (int) keyctl_setperm_key(key_ref_to_ptr(key_ref)->serial,
			(key_perm_t)(KEY_USR_SEARCH|KEY_USR_ALL));
	if (rc == -EINVAL) {
		printk(KERN_ERR
				"%s Unable to set key permission\n",
			   	__func__);
		goto error6;
	}

	strncpy(sig, hash, ECRYPTFS_DEFAULT_KEY_BYTES);
	printk(KERN_DEBUG "%s key & sig (%s) pair is added.\n", __func__, sig);

error6:
	key_ref_put(key_ref);
error5:
	key_type_put(ktype);
error4:
	key_ref_put(keyring_ref);
error3:
	kfree(hash);
error2:
	kfree(fekek);
error1:
	kfree(key);
	return rc;
}

static struct file_operations kcgroup_crypto_fops = {
	.owner = THIS_MODULE,
	.read = kcgroup_crypto_read,
	.write = kcgroup_crypto_write,
	.open = kcgroup_crypto_open,
	.release = kcgroup_crypto_release
};

struct miscdevice kcgroup_crypto_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = KCGROUP_DEV,
	.fops = &kcgroup_crypto_fops,
};

static int __init kcgroup_crypto_init(void)
{
	printk(KERN_ALERT "%s\n", __func__);
	return misc_register(&kcgroup_crypto_dev);
}

static void __exit kcgroup_crypto_exit (void)
{
	printk(KERN_ALERT "%s\n", __func__);
}

MODULE_AUTHOR("Ken Chen");
MODULE_DESCRIPTION("kcgroup crypto driver");
MODULE_VERSION("0.1");

late_initcall(kcgroup_crypto_init);
module_exit(kcgroup_crypto_exit);
