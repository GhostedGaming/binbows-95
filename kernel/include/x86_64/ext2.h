#ifndef EXT2_H
#define EXT2_H

#include <stdint.h>

/* Basic Constants */
#define EXT2_SUPERBLOCK_OFFSET    1024    /* Superblock starts at byte 1024 */
#define EXT2_SUPERBLOCK_SIZE      1024

#define EXT2_NAME_LEN             255     /* Max filename length */

/* Filesystem feature flags */
#define EXT2_FEATURE_COMPAT_DIR_PREALLOC     0x0001
#define EXT2_FEATURE_COMPAT_IMAGIC_INODES    0x0002
#define EXT2_FEATURE_COMPAT_HAS_JOURNAL      0x0004
#define EXT2_FEATURE_COMPAT_EXT_ATTR         0x0008
#define EXT2_FEATURE_COMPAT_RESIZE_INODE     0x0010
#define EXT2_FEATURE_COMPAT_DIR_INDEX        0x0020

#define EXT2_FEATURE_INCOMPAT_COMPRESSION    0x0001
#define EXT2_FEATURE_INCOMPAT_FILETYPE       0x0002
#define EXT2_FEATURE_INCOMPAT_RECOVER         0x0004
#define EXT2_FEATURE_INCOMPAT_JOURNAL_DEV     0x0008
#define EXT2_FEATURE_INCOMPAT_META_BG          0x0010

#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER   0x0001
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE     0x0002
#define EXT2_FEATURE_RO_COMPAT_BTREE_DIR      0x0004

/* File types for directory entries */
#define EXT2_FT_UNKNOWN    0
#define EXT2_FT_REG_FILE   1
#define EXT2_FT_DIR        2
#define EXT2_FT_CHRDEV     3
#define EXT2_FT_BLKDEV     4
#define EXT2_FT_FIFO       5
#define EXT2_FT_SOCK       6
#define EXT2_FT_SYMLINK    7

/* Inode modes (file types) */
#define EXT2_S_IFSOCK  0xC000  /* socket */
#define EXT2_S_IFLNK   0xA000  /* symbolic link */
#define EXT2_S_IFREG   0x8000  /* regular file */
#define EXT2_S_IFBLK   0x6000  /* block device */
#define EXT2_S_IFDIR   0x4000  /* directory */
#define EXT2_S_IFCHR   0x2000  /* character device */
#define EXT2_S_IFIFO   0x1000  /* fifo */

/* Common permission bits */
#define EXT2_S_IRUSR 0x0100 /* user read */
#define EXT2_S_IWUSR 0x0080 /* user write */
#define EXT2_S_IXUSR 0x0040 /* user execute */
#define EXT2_S_IRGRP 0x0020 /* group read */
#define EXT2_S_IWGRP 0x0010 /* group write */
#define EXT2_S_IXGRP 0x0008 /* group execute */
#define EXT2_S_IROTH 0x0004 /* others read */
#define EXT2_S_IWOTH 0x0002 /* others write */
#define EXT2_S_IXOTH 0x0001 /* others execute */

/* EXT2 Superblock structure */
struct ext2_superblock {
    uint32_t   s_inodes_count;         /* Total inode count */
    uint32_t   s_blocks_count;         /* Total block count */
    uint32_t   s_r_blocks_count;       /* Reserved blocks count */
    uint32_t   s_free_blocks_count;    /* Free blocks count */
    uint32_t   s_free_inodes_count;    /* Free inodes count */
    uint32_t   s_first_data_block;     /* First Data Block */
    uint32_t   s_log_block_size;       /* Block size = 1024 << s_log_block_size */
    uint32_t   s_log_frag_size;        /* Fragment size */
    uint32_t   s_blocks_per_group;     /* Number of blocks per group */
    uint32_t   s_frags_per_group;      /* Number of fragments per group */
    uint32_t   s_inodes_per_group;     /* Number of inodes per group */
    uint32_t   s_mtime;                /* Mount time */
    uint32_t   s_wtime;                /* Write time */
    uint16_t   s_mnt_count;            /* Mount count */
    uint16_t   s_max_mnt_count;        /* Max mount count before check */
    uint16_t   s_magic;                /* Magic signature (0xEF53) */
    uint16_t   s_state;                /* File system state */
    uint16_t   s_errors;               /* Behaviour when detecting errors */
    uint16_t   s_minor_rev_level;      /* Minor revision level */
    uint32_t   s_lastcheck;            /* Time of last check */
    uint32_t   s_checkinterval;        /* Max time between checks */
    uint32_t   s_creator_os;           /* OS */
    uint32_t   s_rev_level;            /* Revision level */
    uint16_t   s_def_resuid;           /* Default uid for reserved blocks */
    uint16_t   s_def_resgid;           /* Default gid for reserved blocks */

    /* EXT2_DYNAMIC_REV superblock fields */
    uint32_t   s_first_ino;            /* First non-reserved inode */
    uint16_t   s_inode_size;           /* Size of inode structure */
    uint16_t   s_block_group_nr;       /* Block group # of this superblock */
    uint32_t   s_feature_compat;       /* Compatible feature set */
    uint32_t   s_feature_incompat;     /* Incompatible feature set */
    uint32_t   s_feature_ro_compat;    /* Readonly-compatible feature set */
    uint8_t    s_uuid[16];             /* 128-bit uuid for volume */
    char       s_volume_name[16];      /* Volume name */
    char       s_last_mounted[64];     /* Directory where last mounted */
    uint32_t   s_algorithm_usage_bitmap; /* For compression */

    /* Performance hints */
    uint8_t    s_prealloc_blocks;      /* Number of blocks to preallocate */
    uint8_t    s_prealloc_dir_blocks;  /* Number of blocks to preallocate for directories */
    uint16_t   s_padding1;

    /* Journaling support */
    uint8_t    s_journal_uuid[16];     /* UUID of journal superblock */
    uint32_t   s_journal_inum;         /* Inode number of journal file */
    uint32_t   s_journal_dev;          /* Device number of journal file */
    uint32_t   s_last_orphan;          /* Start of list of orphaned inodes */

    uint32_t   s_hash_seed[4];         /* HTREE hash seed */
    uint8_t    s_def_hash_version;     /* Default hash version */
    uint8_t    s_jnl_backup_type;
    uint16_t   s_desc_size;            /* Size of group descriptor */
    uint32_t   s_default_mount_opts;
    uint32_t   s_first_meta_bg;        /* First metablock block group */

    uint32_t   s_mkfs_time;            /* When the filesystem was created */
    uint32_t   s_jnl_blocks[17];       /* Backup of the journal blocks */

    /* Remaining fields are reserved and zero */
    uint32_t   s_blocks_count_hi;
    uint32_t   s_r_blocks_count_hi;
    uint32_t   s_free_blocks_count_hi;
    uint16_t   s_min_extra_isize;
    uint16_t   s_want_extra_isize;
    uint32_t   s_flags;
    uint16_t   s_raid_stride;
    uint16_t   s_mmp_interval;
    uint64_t   s_mmp_block;
    uint32_t   s_raid_stripe_width;
    uint8_t    s_log_groups_per_flex;
    uint8_t    s_checksum_type;
    uint16_t   s_reserved_pad;
    uint64_t   s_kbytes_written;
    uint32_t   s_snapshot_inum;
    uint32_t   s_snapshot_id;
    uint64_t   s_snapshot_r_blocks_count;
    uint32_t   s_snapshot_list;
    uint32_t   s_error_count;
    uint32_t   s_first_error_time;
    uint32_t   s_first_error_ino;
    uint64_t   s_first_error_block;
    uint8_t    s_first_error_func[32];
    uint32_t   s_first_error_line;
    uint32_t   s_last_error_time;
    uint32_t   s_last_error_ino;
    uint32_t   s_last_error_line;
    uint64_t   s_last_error_block;
    uint8_t    s_last_error_func[32];
    uint8_t    s_mount_opts[64];
    uint32_t   s_usr_quota_inum;
    uint32_t   s_grp_quota_inum;
    uint32_t   s_overhead_blocks;
    uint32_t   s_backup_bgs[2];
    uint8_t    s_encrypt_algos[4];
    uint8_t    s_encrypt_pw_salt[16];
    uint32_t   s_lpf_ino;
    uint32_t   s_prj_quota_inum;
    uint32_t   s_checksum_seed;
    uint8_t    s_wtime_hi;
    uint8_t    s_mtime_hi;
    uint8_t    s_mkfs_time_hi;
    uint8_t    s_lastcheck_hi;
    uint8_t    s_first_error_time_hi;
    uint8_t    s_last_error_time_hi;
    uint16_t   s_pad;
    uint32_t   s_reserved[96];
} __attribute__((packed));

/* Group descriptor structure */
struct ext2_group_desc {
    uint32_t   bg_block_bitmap;        /* Block bitmap block */
    uint32_t   bg_inode_bitmap;        /* Inode bitmap block */
    uint32_t   bg_inode_table;         /* Inode table block */
    uint16_t   bg_free_blocks_count;   /* Free blocks count */
    uint16_t   bg_free_inodes_count;   /* Free inodes count */
    uint16_t   bg_used_dirs_count;     /* Directories count */
    uint16_t   bg_pad;
    uint32_t   bg_reserved[3];
} __attribute__((packed));

/* Inode structure */
struct ext2_inode {
    uint16_t   i_mode;         /* File mode */
    uint16_t   i_uid;          /* Owner UID */
    uint32_t   i_size;         /* Size in bytes */
    uint32_t   i_atime;        /* Access time */
    uint32_t   i_ctime;        /* Creation time */
    uint32_t   i_mtime;        /* Modification time */
    uint32_t   i_dtime;        /* Deletion Time */
    uint16_t   i_gid;          /* Group ID */
    uint16_t   i_links_count;  /* Links count */
    uint32_t   i_blocks;       /* Blocks count (in 512-byte sectors) */
    uint32_t   i_flags;        /* File flags */
    uint32_t   i_osd1;         /* OS dependent 1 */
    uint32_t   i_block[15];    /* Pointers to blocks */
    uint32_t   i_generation;   /* File version (for NFS) */
    uint32_t   i_file_acl;     /* File ACL */
    uint32_t   i_dir_acl;      /* Directory ACL */
    uint32_t   i_faddr;        /* Fragment address */
    uint8_t    i_osd2[12];     /* OS dependent 2 */
} __attribute__((packed));

/* Directory entry structure */
struct ext2_dir_entry {
    uint32_t   inode;            /* Inode number */
    uint16_t   rec_len;          /* Directory entry length */
    uint8_t    name_len;         /* Name length */
    uint8_t    file_type;        /* File type (since ext2 revision 0.5) */
    char       name[EXT2_NAME_LEN]; /* File name (up to EXT2_NAME_LEN) */
} __attribute__((packed));

#endif // EXT2_H
