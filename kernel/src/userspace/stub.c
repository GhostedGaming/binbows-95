/* Tiny userspace stub data to be copied into user memory. This is raw bytes
   for a tiny position-independent function that invokes int 0x80 to write
   to fd=1. */

const unsigned char userspace_stub[] = {
    /* push registers we will use */
    0x48,0x31,0xC0,                   /* xor rax, rax */
    /* prepare arguments for write: fd=1 (rdi), buf=addr, len=17 (rdx) */
    0x48,0xC7,0xC7,0x01,0x00,0x00,0x00, /* mov rdi,1 */
    /* lea rsi, [rip+offset_to_msg] */
    0x48,0x8D,0x35,0x0E,0x00,0x00,0x00, /* lea rsi,[rip+0xe] */
    0x48,0xC7,0xC2,0x11,0x00,0x00,0x00, /* mov rdx,17 */
    /* int 0x80 */
    0xCD,0x80,
    /* infinite loop */
    0xEB,0xFE,
    /* message: "hello from user\n" (17 bytes) */
    'h','e','l','l','o',' ','f','r','o','m',' ','u','s','e','r','\n','\0'
};

const unsigned int userspace_stub_len = sizeof(userspace_stub);
