/*
 *  kcgroup device to setup ecryptfs key and signature.
 */

/* Version verification for shared data structures w/ userspace */
#define ECRYPTFS_VERSION_MAJOR 0x00
#define ECRYPTFS_VERSION_MINOR 0x04
#define ECRYPTFS_SUPPORTED_FILE_VERSION 0x03
/* These flags indicate which features are supported by the kernel
 * module; userspace tools such as the mount helper read
 * ECRYPTFS_VERSIONING_MASK from a sysfs handle in order to determine
 * how to behave. */
#define ECRYPTFS_VERSIONING_PASSPHRASE            0x00000001
#define ECRYPTFS_VERSIONING_PUBKEY                0x00000002
#define ECRYPTFS_VERSIONING_PLAINTEXT_PASSTHROUGH 0x00000004
#define ECRYPTFS_VERSIONING_POLICY                0x00000008
#define ECRYPTFS_VERSIONING_XATTR                 0x00000010
#define ECRYPTFS_VERSIONING_MULTKEY               0x00000020
#define ECRYPTFS_VERSIONING_DEVMISC               0x00000040
#define ECRYPTFS_VERSIONING_HMAC                  0x00000080
#define ECRYPTFS_VERSIONING_FILENAME_ENCRYPTION   0x00000100
#define ECRYPTFS_VERSIONING_GCM                   0x00000200
#define ECRYPTFS_VERSIONING_MASK (ECRYPTFS_VERSIONING_PASSPHRASE \
				  | ECRYPTFS_VERSIONING_PLAINTEXT_PASSTHROUGH \
				  | ECRYPTFS_VERSIONING_PUBKEY \
				  | ECRYPTFS_VERSIONING_XATTR \
				  | ECRYPTFS_VERSIONING_MULTKEY \
				  | ECRYPTFS_VERSIONING_DEVMISC \
				  | ECRYPTFS_VERSIONING_FILENAME_ENCRYPTION)
#define ECRYPTFS_MAX_PASSWORD_LENGTH 64
#define ECRYPTFS_MAX_PASSPHRASE_BYTES ECRYPTFS_MAX_PASSWORD_LENGTH
#define ECRYPTFS_SALT_SIZE 8
#define ECRYPTFS_SALT_SIZE_HEX (ECRYPTFS_SALT_SIZE*2)
/* The original signature size is only for what is stored on disk; all
 * in-memory representations are expanded hex, so it better adapted to
 * be passed around or referenced on the command line */
#define ECRYPTFS_SIG_SIZE 8
#define ECRYPTFS_SIG_SIZE_HEX (ECRYPTFS_SIG_SIZE*2)
#define ECRYPTFS_PASSWORD_SIG_SIZE ECRYPTFS_SIG_SIZE_HEX
#define ECRYPTFS_MAX_KEY_BYTES 64
#define ECRYPTFS_MAX_ENCRYPTED_KEY_BYTES 512
#define ECRYPTFS_DEFAULT_IV_BYTES 16
#define ECRYPTFS_FILE_VERSION 0x03
#define ECRYPTFS_DEFAULT_EXTENT_SIZE 4096
#define ECRYPTFS_MINIMUM_HEADER_EXTENT_SIZE 8192
#define ECRYPTFS_DEFAULT_MSG_CTX_ELEMS 32
#define ECRYPTFS_DEFAULT_SEND_TIMEOUT HZ
#define ECRYPTFS_MAX_MSG_CTX_TTL (HZ*3)
#define ECRYPTFS_MAX_PKI_NAME_BYTES 16
#define ECRYPTFS_DEFAULT_NUM_USERS 4
#define ECRYPTFS_MAX_NUM_USERS 32768
#define ECRYPTFS_XATTR_NAME "user.ecryptfs"

#define ECRYPTFS_DEFAULT_CIPHER "aes"
#define ECRYPTFS_DEFAULT_KEY_BYTES 16
#define ECRYPTFS_DEFAULT_HASH "md5"

struct ecryptfs_session_key {
#define ECRYPTFS_USERSPACE_SHOULD_TRY_TO_DECRYPT 0x00000001
#define ECRYPTFS_USERSPACE_SHOULD_TRY_TO_ENCRYPT 0x00000002
#define ECRYPTFS_CONTAINS_DECRYPTED_KEY 0x00000004
#define ECRYPTFS_CONTAINS_ENCRYPTED_KEY 0x00000008
    u32 flags;
    u32 encrypted_key_size;
    u32 decrypted_key_size;
    u8 encrypted_key[ECRYPTFS_MAX_ENCRYPTED_KEY_BYTES];
    u8 decrypted_key[ECRYPTFS_MAX_KEY_BYTES];
};

struct ecryptfs_password {
    u32 password_bytes;
    s32 hash_algo;
    u32 hash_iterations;
    u32 session_key_encryption_key_bytes;
#define ECRYPTFS_PERSISTENT_PASSWORD 0x01
#define ECRYPTFS_SESSION_KEY_ENCRYPTION_KEY_SET 0x02
    u32 flags;
    /* Iterated-hash concatenation of salt and passphrase */
    u8 session_key_encryption_key[ECRYPTFS_MAX_KEY_BYTES];
    u8 signature[ECRYPTFS_PASSWORD_SIG_SIZE + 1];
    /* Always in expanded hex */
    u8 salt[ECRYPTFS_SALT_SIZE];
};

enum ecryptfs_token_types {ECRYPTFS_PASSWORD, ECRYPTFS_PRIVATE_KEY};

struct ecryptfs_private_key {
    u32 key_size;
    u32 data_len;
    u8 signature[ECRYPTFS_PASSWORD_SIG_SIZE + 1]; 
    char pki_type[ECRYPTFS_MAX_PKI_NAME_BYTES + 1]; 
    u8 data[];
};

/* May be a password or a private key */
struct ecryptfs_auth_tok {
    u16 version; /* 8-bit major and 8-bit minor */
    u16 token_type;
#define ECRYPTFS_ENCRYPT_ONLY 0x00000001
    u32 flags;
    struct ecryptfs_session_key session_key;
    u8 reserved[32];
    union {
        struct ecryptfs_password password;
        struct ecryptfs_private_key private_key;
    } token;
} __attribute__ ((packed));


#define KEY_LOOKUP_CREATE   0x01
#define KEY_WRITE   0x04
#define KEY_SEARCH  0x08    

extern void key_type_put(struct key_type *ktype);
extern struct key_type *key_type_lookup(const char *type);
extern key_ref_t lookup_user_key(key_serial_t id, unsigned long flags,
                 key_perm_t perm);
extern long keyctl_setperm_key(key_serial_t, key_perm_t);


#define KCGROUP_OWNER   		"rpeserver"
#define KCGROUP_DEV 			"rpe-crypto" 
#define KCGROUP_SALT 			"6a778e8e9340a8dd" 
#define KCGROUP_KEYRING_TYPE 	"user" 
#define KCGROUP_MAX_SIG_SIZE 	512
#define KCGROUP_MAX_KEY_SIZE 	KCGROUP_MAX_SIG_SIZE/2

static int kcgroup_crypto_open(struct inode *, struct file *);
static int kcgroup_crypto_release(struct inode *, struct file *);
static ssize_t kcgroup_crypto_read(struct file *, char *, size_t, loff_t *);
static ssize_t kcgroup_crypto_write(struct file *, const char *, size_t, loff_t *);

