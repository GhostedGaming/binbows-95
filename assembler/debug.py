#!/usr/bin/env python3
"""
BEXE Debug Tool - Inspect and disassemble BEXE files
"""
import sys
import struct

# Opcodes matching our VM
OPCODES = {
    0x01: ("MV",   2),
    0x02: ("ADD",  2),
    0x03: ("SUB",  2),
    0x04: ("MUL",  2),
    0x05: ("DIV",  2),
    0x06: ("JMP",  1),
    0x07: ("JE",   1),
    0x08: ("JNE",  1),
    0x09: ("JL",   1),
    0x0A: ("JG",   1),
    0x0B: ("JLE",  1),
    0x0C: ("JGE",  1),
    0x10: ("CMP",  2),
    0x20: ("PUSH", 1),
    0x21: ("POP",  1),
    0x30: ("CALL", 1),
    0x31: ("RET",  0),
    0x40: ("HLT",  0),
    0x50: ("INT",  1),
    0x90: ("NOP",  0),
}

REGS = ["rax", "rbx", "rcx", "rdx", "rsp", "rbp", "rsi", "rdi",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"]

MODE_NAMES = {0: "REG", 1: "IMM"}

def hexdump(data, offset=0, width=16):
    """Pretty hexdump with ASCII"""
    for i in range(0, len(data), width):
        chunk = data[i:i+width]
        hex_part = ' '.join(f'{b:02x}' for b in chunk)
        ascii_part = ''.join(chr(b) if 32 <= b < 127 else '.' for b in chunk)
        print(f'{offset+i:08x}  {hex_part:<{width*3}}  |{ascii_part}|')

def disassemble(code, code_offset=0):
    """Disassemble BEXE bytecode"""
    ip = 0
    instruction_num = 0
    
    while ip < len(code):
        offset = ip
        addr = code_offset + ip
        
        if ip >= len(code):
            break
            
        opcode = code[ip]
        ip += 1
        instruction_num += 1
        
        # Format instruction start
        line = f"{addr:04x}:  [{opcode:02x}] "
        
        if opcode not in OPCODES:
            print(f"{line}<unknown opcode 0x{opcode:02x}>")
            continue
        
        mnemonic, operand_count = OPCODES[opcode]
        
        # No operands
        if operand_count == 0:
            print(f"{line}{mnemonic}")
            continue
        
        # INT instruction (special case - single byte operand)
        if opcode == 0x50:  # INT
            if ip >= len(code):
                print(f"{line}{mnemonic} <missing operand>")
                break
            int_num = code[ip]
            ip += 1
            print(f"{line}{int_num:02x}] {mnemonic} 0x{int_num:02x}")
            continue
        
        # One operand instructions (JMP, CALL, PUSH, POP)
        if operand_count == 1:
            if opcode in (0x20, 0x21):  # PUSH, POP
                if ip >= len(code):
                    print(f"{line}{mnemonic} <missing operand>")
                    break
                reg = code[ip]
                ip += 1
                reg_name = REGS[reg] if reg < len(REGS) else f"r{reg}"
                print(f"{line}{reg:02x}] {mnemonic} {reg_name}")
            else:  # JMP, CALL, conditional jumps
                if ip + 3 >= len(code):
                    print(f"{line}{mnemonic} <incomplete address>")
                    break
                addr_val = struct.unpack('<I', code[ip:ip+4])[0]
                bytes_hex = ' '.join(f'{code[ip+i]:02x}' for i in range(4))
                print(f"{line}{bytes_hex}] {mnemonic} 0x{addr_val:04x}")
                ip += 4
            continue
        
        # Two operand instructions (MV, ADD, SUB, MUL, DIV, CMP)
        if operand_count == 2:
            if ip >= len(code):
                print(f"{line}{mnemonic} <missing operands>")
                break
            
            dst = code[ip]
            ip += 1
            
            if ip >= len(code):
                print(f"{line}{dst:02x}] {mnemonic} {REGS[dst]}, <missing>")
                break
            
            mode = code[ip]
            ip += 1
            
            dst_name = REGS[dst] if dst < len(REGS) else f"r{dst}"
            mode_name = MODE_NAMES.get(mode, f"?{mode}")
            
            if mode == 0:  # REG mode
                if ip >= len(code):
                    print(f"{line}{dst:02x} {mode:02x}] {mnemonic} {dst_name}, <missing>")
                    break
                src = code[ip]
                ip += 1
                src_name = REGS[src] if src < len(REGS) else f"r{src}"
                print(f"{line}{dst:02x} {mode:02x} {src:02x}] {mnemonic} {dst_name}, {src_name}")
                
            elif mode == 1:  # IMM mode
                if ip + 3 >= len(code):
                    print(f"{line}{dst:02x} {mode:02x}] {mnemonic} {dst_name}, <incomplete imm>")
                    break
                imm = struct.unpack('<I', code[ip:ip+4])[0]
                bytes_hex = ' '.join(f'{code[ip+i]:02x}' for i in range(4))
                print(f"{line}{dst:02x} {mode:02x} {bytes_hex}] {mnemonic} {dst_name}, 0x{imm:x}")
                ip += 4
            else:
                print(f"{line}{dst:02x} {mode:02x}] {mnemonic} {dst_name}, <invalid mode>")

def parse_bexe(filename):
    """Parse and display BEXE file"""
    try:
        with open(filename, 'rb') as f:
            data = f.read()
    except FileNotFoundError:
        print(f"Error: File '{filename}' not found")
        return
    except Exception as e:
        print(f"Error reading file: {e}")
        return
    
    # Check minimum size
    if len(data) < 12:
        print("Error: File too small to be valid BEXE")
        return
    
    # Parse header (10 bytes: magic=4, code=2, data=2, bss=2)
    magic, code_size, data_size, bss_size = struct.unpack('<IHHH', data[:10])
    
    print("=" * 70)
    print(f"BEXE FILE: {filename}")
    print("=" * 70)
    print(f"Magic:      0x{magic:08X}", end="")
    if magic == 0x42455845:
        print(" ✓ (BEXE)")
    else:
        print(" ✗ INVALID (expected 0x42455845)")
        # Try to show what we got
        print(f"  Bytes: {' '.join(f'{b:02x}' for b in data[:4])}")
        return
    
    print(f"Code Size:  {code_size} bytes")
    print(f"Data Size:  {data_size} bytes")
    print(f"BSS Size:   {bss_size} bytes")
    print(f"File Size:  {len(data)} bytes")
    print()
    
    # Validate sizes
    expected_size = 10 + code_size + data_size
    if len(data) != expected_size:
        print(f"⚠ Warning: Expected {expected_size} bytes, got {len(data)} bytes")
        print()
    
    # Extract sections
    code_start = 10
    code_end = code_start + code_size
    data_start = code_end
    data_end = data_start + data_size
    
    code = data[code_start:code_end]
    data_section = data[data_start:data_end]
    
    # Raw header dump
    print("=" * 70)
    print("RAW HEADER (10 bytes)")
    print("=" * 70)
    hexdump(data[:10])
    print()
    
    # Code section
    if code_size > 0:
        print("=" * 70)
        print(f"CODE SECTION ({code_size} bytes at offset 0x00)")
        print("=" * 70)
        hexdump(code, 0)
        print()
        
        print("=" * 70)
        print("DISASSEMBLY")
        print("=" * 70)
        disassemble(code, 0)
        print()
    
    # Data section
    if data_size > 0:
        print("=" * 70)
        print(f"DATA SECTION ({data_size} bytes at offset 0x{code_size:04x})")
        print("=" * 70)
        hexdump(data_section, code_size)
        print()
        
        # Try to display as text
        try:
            text = data_section.decode('utf-8', errors='replace')
            if any(32 <= ord(c) < 127 or c in '\n\r\t' for c in text):
                print("As text:")
                print(repr(text))
                print()
        except:
            pass
    
    # Summary
    print("=" * 70)
    print("SUMMARY")
    print("=" * 70)
    print(f"Total memory layout:")
    print(f"  0x{0:04x} - 0x{code_size-1:04x} : CODE  ({code_size} bytes)")
    print(f"  0x{code_size:04x} - 0x{code_size+data_size-1:04x} : DATA  ({data_size} bytes)")
    if bss_size > 0:
        print(f"  0x{code_size+data_size:04x} - 0x{code_size+data_size+bss_size-1:04x} : BSS   ({bss_size} bytes)")
    print(f"  Total: {code_size + data_size + bss_size} bytes")
    print()

def main():
    if len(sys.argv) < 2:
        print("BEXE Debug Tool - Inspect and disassemble BEXE files")
        print()
        print(f"Usage: {sys.argv[0]} <file.bexe>")
        print()
        print("This tool will:")
        print("  - Parse the BEXE header")
        print("  - Display hexdump of code and data sections")
        print("  - Disassemble the bytecode")
        print("  - Show memory layout")
        sys.exit(1)
    
    for filename in sys.argv[1:]:
        parse_bexe(filename)
        if len(sys.argv) > 2:
            print("\n\n")

if __name__ == "__main__":
    main()