# Binbows OS

**Binbows** is a monolithic kernel operating system built from scratch with performance, learning, and extensibility in mind.

> Started this project when I was 13 — and yes, I’m still 13 as I write this!

---

## 🚀 Overview

Binbows is designed to be simple, fast, and educational. It aims to grow into a complete modern OS with support for multiple filesystems, device drivers, and userland applications. It’s a hobby project — but a serious one.

---

## ✨ Features

- 🔹 **Buddy Allocator** — Efficient physical memory management.
- 🔹 **Paging** — Virtual memory and memory protection.
- ⌨️ **PS/2 Keyboard Driver** — Direct input handling from hardware.
- ⚙️ **ACPI Support** — Basic system configuration and power interface.
- 💾 **FAT16 Filesystem** — Read/write file access *(planned: ext4, NTFS, FAT32, etc.)*
- 🔊 **PC Speaker Driver** — Simple audio output *(more sound features coming!)*

---

## 🛠️ Roadmap

- [ ] ext4 & NTFS filesystem support  
- [ ] Virtual File System (VFS) layer  
- [ ] Sound Blaster / Audio drivers  
- [ ] Tasking and process scheduling  
- [ ] System calls and userspace  
- [ ] Graphics driver & basic GUI  
- [ ] Shell & command-line tools

---

## 📚 Goals

- Build a deeper understanding of operating systems
- Create a clean and modular codebase
- Learn C, assembly, and hardware-level programming
- Have fun and share progress with others

---

## 🧩 Tech Stack

- Language: **C** and **x86 Assembly**
- Platform: **x86_64** (BIOS boot with Limine)
- Tools: `make`, `qemu`, `nasm`, `ld`

---

## 💬 Contact

Want to contribute, ask questions, or just say hi?  
**Reach out here or open an issue. I'd love feedback from fellow OS devs!**

---

## 🧪 Disclaimer

This is a learning project. Don’t expect Linux — yet 😎  
