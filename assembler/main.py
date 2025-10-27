import sys
import struct
import re

# =========================
# Opcodes
# =========================
MNEMONIC_TO_OPCODE = {
    "mv": 0x01, "mov": 0x01,
    "ad": 0x02, "add": 0x02,
    "su": 0x03, "sub": 0x03,
    "mu": 0x04, "mul": 0x04,
    "di": 0x05, "div": 0x05,
    "jmp": 0x06,
    "je": 0x07, "jz": 0x07,
    "jne": 0x08, "jnz": 0x08,
    "jl": 0x09, "jb": 0x09,
    "jg": 0x0A, "ja": 0x0A,
    "jle": 0x0B, "jbe": 0x0B,
    "jge": 0x0C, "jae": 0x0C,
    "cmp": 0x10,
    "push": 0x20,
    "pop": 0x21,
    "call": 0x30,
    "ret": 0x31,
    "hlt": 0x40,
    "int": 0x50,
    "syscall": 0x50,
    "nop": 0x90,
}

# =========================
# Registers
# =========================
REGISTERS = {
    # 64-bit registers
    "rax": 0x00, "rbx": 0x01, "rcx": 0x02, "rdx": 0x03,
    "rsp": 0x04, "rbp": 0x05, "rsi": 0x06, "rdi": 0x07,
    "r8": 0x08, "r9": 0x09, "r10": 0x0A, "r11": 0x0B,
    "r12": 0x0C, "r13": 0x0D, "r14": 0x0E, "r15": 0x0F,
    
    # 8-bit registers
    "al": 0x10, "bl": 0x11, "cl": 0x12, "dl": 0x13,
    "ah": 0x14, "bh": 0x15, "ch": 0x16, "dh": 0x17,
}

# =========================
# Syscall Numbers
# =========================
SYSCALLS = {
    "SYS_RESERVED": 0,
    "SYS_FB_PRINT": 1,
    "SYS_READ_FILE": 2,
    "SYS_WRITE_FILE": 3,
    "SYS_OPEN_FILE": 4,
    "SYS_CLOSE_FILE": 5,
    "SYS_KMALLOC": 6,
    "SYS_KFREE": 7,
    "SYS_MEMCPY": 8,
    "SYS_MEMCMP": 9,
    "SYS_MEMSET": 10,
}

# =========================
# Instruction Encoding Modes
# =========================
MODE_REG_REG = 0x00  # Both operands are registers
MODE_REG_IMM = 0x01  # Dest is register, source is immediate

# =========================
# Advanced Assembler
# =========================
class Assembler:
    def __init__(self, verbose=False):
        self.code = bytearray()
        self.data = bytearray()
        self.bss_size = 0
        self.labels = {}
        self.data_labels = {}
        self.bss_labels = {}
        self.unresolved_refs = []
        self.current_section = "text"
        self.constants = {}
        self.macros = {}
        self.verbose = verbose
        self.current_file = ""
        self.current_line = 0

    def log(self, msg):
        if self.verbose:
            print(f"[DEBUG] {msg}")

    def error(self, msg):
        location = f"{self.current_file}:{self.current_line}" if self.current_file else f"line {self.current_line}"
        raise ValueError(f"Error at {location}: {msg}")

    # -------------------------
    # Expression Evaluation
    # -------------------------
    def eval_expression(self, expr):
        expr = expr.strip()
        
        if expr in SYSCALLS:
            return SYSCALLS[expr]
        
        if expr in self.constants:
            return self.constants[expr]
        
        # Try direct integer parsing first (handles 0x, 0b, 0o formats)
        try:
            return int(expr, 0)
        except:
            pass
        
        # Replace constants with their values
        for const_name, const_val in self.constants.items():
            expr = expr.replace(const_name, str(const_val))
        
        # Try evaluation for expressions
        try:
            return int(eval(expr))
        except:
            return None

    # -------------------------
    # Operand Parsing
    # -------------------------
    def is_register(self, op):
        op = op.strip().lower()
        if op.startswith('[') and op.endswith(']'):
            op = op[1:-1].strip()
        return op in REGISTERS

    def get_register_code(self, op):
        op = op.strip().lower()
        if op.startswith('[') and op.endswith(']'):
            op = op[1:-1].strip()
        return REGISTERS.get(op, 0xFF)

    def is_memory_operand(self, op):
        return op.strip().startswith('[') and op.strip().endswith(']')

    def is_number(self, op):
        op = op.strip()
        if op.startswith('-'):
            op = op[1:]
        return op.startswith('0x') or op.isdigit()

    def parse_number(self, op):
        val = self.eval_expression(op.strip())
        if val is not None:
            return val
        return int(op.strip(), 0)

    # -------------------------
    # String Parsing
    # -------------------------
    def parse_string(self, s):
        if (s.startswith('"') and s.endswith('"')) or (s.startswith("'") and s.endswith("'")):
            s = s[1:-1]
        
        s = s.replace('\\n', '\n')
        s = s.replace('\\r', '\r')
        s = s.replace('\\t', '\t')
        s = s.replace('\\0', '\0')
        s = s.replace('\\\\', '\\')
        return s.encode('utf-8')

    # -------------------------
    # Data Directives
    # -------------------------
    def parse_data_directive(self, directive, operands):
        directive = directive.lower()
        
        if not operands:
            self.error(f"Directive {directive} requires operands")
        
        if directive.startswith("res"):
            if self.current_section != "bss":
                self.error("Reserve directives (resb/resw/resd/resq) only allowed in .bss section")
            multiplier = {'resb':1,'resw':2,'resd':4,'resq':8}[directive]
            count = self.parse_number(operands.strip())
            self.bss_size += count * multiplier
            return
        
        operands = self.split_operands(operands)
        
        for val in operands:
            val = val.strip()
            
            if directive == "db":
                if val.startswith('"') or val.startswith("'"):
                    self.data.extend(self.parse_string(val))
                else:
                    num = self.parse_number(val)
                    self.data.append(num & 0xFF)
                    
            elif directive == "dw":
                num = self.parse_number(val)
                self.data.extend(struct.pack("<H", num & 0xFFFF))
                
            elif directive == "dd":
                num = self.parse_number(val)
                self.data.extend(struct.pack("<I", num & 0xFFFFFFFF))
                
            elif directive == "dq":
                num = self.parse_number(val)
                self.data.extend(struct.pack("<Q", num & 0xFFFFFFFFFFFFFFFF))
            else:
                self.error(f"Unknown data directive: {directive}")

    def split_operands(self, operands):
        result = []
        current = ""
        in_string = False
        quote_char = None
        
        for char in operands:
            if char in ('"', "'") and (not in_string or char == quote_char):
                in_string = not in_string
                quote_char = char if in_string else None
                current += char
            elif char == ',' and not in_string:
                if current.strip():
                    result.append(current.strip())
                current = ""
            else:
                current += char
        
        if current.strip():
            result.append(current.strip())
        return result

    # -------------------------
    # Preprocessor
    # -------------------------
    def handle_equ(self, line):
        parts = line.split()
        if len(parts) < 3:
            self.error("EQU requires: name equ value")
        name = parts[0]
        value = ' '.join(parts[2:])
        self.constants[name] = self.parse_number(value)
        self.log(f"Constant '{name}' = {self.constants[name]}")

    def handle_macro(self, lines, start_idx):
        parts = lines[start_idx].split()
        if len(parts) < 3:
            self.error("Macro requires: %macro name param_count")
        
        macro_name = parts[1]
        param_count = int(parts[2])
        
        macro_body = []
        idx = start_idx + 1
        while idx < len(lines):
            if lines[idx].strip().lower() == "%endmacro":
                break
            macro_body.append(lines[idx])
            idx += 1
        
        self.macros[macro_name] = {
            'params': param_count,
            'body': macro_body
        }
        self.log(f"Defined macro '{macro_name}' with {param_count} parameters")
        return idx

    def expand_macro(self, name, args):
        if name not in self.macros:
            self.error(f"Undefined macro: {name}")
        
        macro = self.macros[name]
        if len(args) != macro['params']:
            self.error(f"Macro '{name}' expects {macro['params']} args, got {len(args)}")
        
        expanded = []
        for line in macro['body']:
            for i, arg in enumerate(args):
                line = line.replace(f'%{i+1}', arg)
            expanded.append(line)
        
        return expanded

    # -------------------------
    # Instruction Encoding
    # -------------------------
    def encode_two_operand_instruction(self, opcode, op1, op2):
        """
        Encode instructions with two operands (MV, ADD, SUB, MUL, DIV, CMP)
        Format: OPCODE DST MODE SRC/IMM
        """
        self.code.append(opcode)
        
        if not self.is_register(op1):
            self.error(f"First operand must be a register, got: {op1}")
        
        dst_reg = self.get_register_code(op1)
        self.code.append(dst_reg)
        
        # Determine mode and encode second operand
        if self.is_register(op2):
            # Register to register
            self.code.append(MODE_REG_REG)
            src_reg = self.get_register_code(op2)
            self.code.append(src_reg)
            self.log(f"Encoded: REG[{dst_reg}] <- REG[{src_reg}]")
        else:
            # Immediate or label
            self.code.append(MODE_REG_IMM)
            if self.is_number(op2):
                imm = self.parse_number(op2)
                self.code.extend(struct.pack("<I", imm & 0xFFFFFFFF))
                self.log(f"Encoded: REG[{dst_reg}] <- IMM 0x{imm:X}")
            else:
                # Label reference
                self.log(f"Unresolved reference to '{op2}' at offset {len(self.code)}")
                self.unresolved_refs.append((len(self.code), op2, 'abs'))
                self.code.extend(b'\x00\x00\x00\x00')

    def encode_one_operand_instruction(self, opcode, op):
        """
        Encode instructions with one operand (PUSH, POP, JMP, CALL, etc.)
        """
        self.code.append(opcode)
        
        if self.is_register(op):
            reg = self.get_register_code(op)
            self.code.append(reg)
            self.log(f"Encoded: single operand REG[{reg}]")
        elif self.is_number(op):
            imm = self.parse_number(op)
            self.code.extend(struct.pack("<I", imm & 0xFFFFFFFF))
            self.log(f"Encoded: single operand IMM 0x{imm:X}")
        else:
            # Label reference
            self.log(f"Unresolved reference to '{op}' at offset {len(self.code)}")
            self.unresolved_refs.append((len(self.code), op, 'abs'))
            self.code.extend(b'\x00\x00\x00\x00')

    # -------------------------
    # Parse Line
    # -------------------------
    def parse_line(self, line):
        line = line.split(';', 1)[0].strip()
        if not line:
            return None
        
        if line.startswith('%'):
            return line
        
        if ' equ ' in line.lower():
            self.handle_equ(line)
            return None
        
        if line.startswith('.'):
            section = line.lower().strip('.')
            if section in ("text", "data", "bss"):
                self.current_section = section
                self.log(f"Switched to .{section} section")
            else:
                self.error(f"Unknown section: {line}")
            return None
        
        if ':' in line:
            label = line.split(':')[0].strip()
            rest = line.split(':', 1)[1].strip()
            
            if self.current_section == "text":
                self.labels[label] = len(self.code)
                self.log(f"Text label '{label}' at offset {len(self.code)}")
            elif self.current_section == "data":
                self.data_labels[label] = len(self.data)
                self.log(f"Data label '{label}' at offset {len(self.data)}")
            elif self.current_section == "bss":
                self.bss_labels[label] = self.bss_size
                self.log(f"BSS label '{label}' at offset {self.bss_size}")
            
            if not rest:
                return None
            line = rest
        
        if self.current_section in ("data", "bss"):
            parts = line.split(None, 1)
            directive = parts[0].lower()
            operands = parts[1] if len(parts) > 1 else ""
            self.parse_data_directive(directive, operands)
            return None
        
        parts = line.split()
        if parts and parts[0] in self.macros:
            args = self.split_operands(' '.join(parts[1:])) if len(parts) > 1 else []
            return ('macro', parts[0], args)
        
        # Parse instruction
        parts = line.split(None, 1)
        mnemonic = parts[0].lower()
        operands = parts[1] if len(parts) > 1 else ""
        
        if mnemonic not in MNEMONIC_TO_OPCODE:
            self.error(f"Unknown mnemonic: {mnemonic}")
        
        opcode = MNEMONIC_TO_OPCODE[mnemonic]
        
        # No operand instructions
        if mnemonic in ("ret", "hlt", "nop"):
            self.code.append(opcode)
            self.log(f"Encoded: {mnemonic}")
            return None
        
        # INT/SYSCALL - This is the fixed version!
        if mnemonic in ("int", "syscall"):
            self.code.append(opcode)  # Always append the INT opcode (0x50)
            
            if not operands:
                if mnemonic == "syscall":
                    # Bare "syscall" defaults to int 0x80
                    self.code.append(0x80)
                    self.log(f"Encoded: syscall -> int 0x80")
                else:
                    self.error(f"int requires an operand (e.g., 'int 0x80')")
            else:
                # Parse the operand - could be a number or a syscall constant
                if operands in SYSCALLS:
                    # It's a syscall constant like SYS_FB_PRINT
                    # IMPORTANT: Don't encode the syscall number here!
                    # The syscall number should be loaded into RAX by the user
                    # Just generate int 0x80
                    self.code.append(0x80)
                    self.log(f"Encoded: {mnemonic} {operands} -> int 0x80 (syscall number should be in RAX)")
                else:
                    # It's a numeric interrupt number like 0x80 or 128
                    val = self.parse_number(operands)
                    self.code.append(val & 0xFF)
                    self.log(f"Encoded: int 0x{val:02X}")
            return None
        
        # Parse operands
        if not operands:
            self.error(f"{mnemonic} requires operands")
        
        ops = self.split_operands(operands)
        
        # Two-operand instructions
        if mnemonic in ("mv", "mov", "ad", "add", "su", "sub", "mu", "mul", "di", "div", "cmp"):
            if len(ops) != 2:
                self.error(f"{mnemonic} requires exactly 2 operands")
            self.encode_two_operand_instruction(opcode, ops[0], ops[1])
        
        # One-operand instructions
        elif mnemonic in ("push", "pop", "jmp", "je", "jz", "jne", "jnz", "jl", "jb", "jg", "ja", "jle", "jbe", "jge", "jae", "call"):
            if len(ops) != 1:
                self.error(f"{mnemonic} requires exactly 1 operand")
            self.encode_one_operand_instruction(opcode, ops[0])
        
        else:
            self.error(f"Unknown instruction format for: {mnemonic}")
        
        return None

    # -------------------------
    # Parse File
    # -------------------------
    def parse_file(self, buf, filename=""):
        self.current_file = filename
        lines = buf.split('\n')
        idx = 0
        
        while idx < len(lines):
            self.current_line = idx + 1
            line = lines[idx].strip()
            
            if line.startswith('%macro'):
                idx = self.handle_macro(lines, idx)
                idx += 1
                continue
            
            result = self.parse_line(line)
            
            if result and result[0] == 'macro':
                _, macro_name, args = result
                expanded = self.expand_macro(macro_name, args)
                for exp_line in expanded:
                    self.parse_line(exp_line)
            
            idx += 1

    # -------------------------
    # Resolve Labels
    # -------------------------
    def resolve_labels(self):
        self.log("\nResolving labels...")
        code_size = len(self.code)
        data_size = len(self.data)
        
        for offset, label, ref_type in self.unresolved_refs:
            addr = None
            
            if label in self.labels:
                addr = self.labels[label]
                self.log(f"Resolved text label '{label}' -> {addr}")
                
            elif label in self.data_labels:
                addr = code_size + self.data_labels[label]
                self.log(f"Resolved data label '{label}' -> {addr}")
                
            elif label in self.bss_labels:
                addr = code_size + data_size + self.bss_labels[label]
                self.log(f"Resolved BSS label '{label}' -> {addr}")
                
            else:
                self.error(f"Undefined label: {label}")
            
            self.code[offset:offset+4] = struct.pack("<I", addr & 0xFFFFFFFF)
            self.log(f"Patched offset {offset} with address {addr}")

    # -------------------------
    # Write BEXE
    # -------------------------
    def write_bexe(self, filename):
        # BEXE Header: magic(4) + code_size(2) + data_size(2) + bss_size(2) = 10 bytes
        header = struct.pack("<IHHH", 0x42455845, len(self.code), len(self.data), self.bss_size)
        
        with open(filename, "wb") as f:
            f.write(header)
            f.write(self.code)
            f.write(self.data)
        
        print(f"\n[SUCCESS] Wrote {filename}")
        print(f"  Header: 10 bytes")
        print(f"  Code:   {len(self.code)} bytes")
        print(f"  Data:   {len(self.data)} bytes")
        print(f"  BSS:    {self.bss_size} bytes")
        print(f"  Total:  {10 + len(self.code) + len(self.data)} bytes")
        
        if self.verbose:
            print(f"\nCode section hex dump:")
            for i in range(0, min(len(self.code), 64), 16):
                hex_str = ' '.join(f'{b:02x}' for b in self.code[i:i+16])
                print(f"  {i:04x}: {hex_str}")
            
            if len(self.data) > 0:
                print(f"\nData section hex dump:")
                for i in range(0, min(len(self.data), 64), 16):
                    hex_str = ' '.join(f'{b:02x}' for b in self.data[i:i+16])
                    ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in self.data[i:i+16])
                    print(f"  {i:04x}: {hex_str:48s} | {ascii_str}")

# -------------------------
# Main
# -------------------------
if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} [options] <out.bexe> <in.asm> [more.asm...]")
        print(f"\nOptions:")
        print(f"  -v, --verbose    Enable verbose debug output")
        print(f"\nExample:")
        print(f"  {sys.argv[0]} -v hello.bexe hello.asm")
        sys.exit(1)
    
    verbose = False
    args = sys.argv[1:]
    
    if args[0] in ('-v', '--verbose'):
        verbose = True
        args = args[1:]
    
    if len(args) < 2:
        print(f"[ERROR] Not enough arguments")
        print(f"Usage: {sys.argv[0]} [options] <out.bexe> <in.asm> [more.asm...]")
        sys.exit(1)
    
    output_file = args[0]
    input_files = args[1:]
    
    assembler = Assembler(verbose=verbose)
    
    for filename in input_files:
        print(f"[INFO] Assembling {filename}...")
        try:
            with open(filename, "r", encoding="utf-8") as f:
                assembler.parse_file(f.read(), filename)
        except FileNotFoundError:
            print(f"[ERROR] File not found: {filename}")
            sys.exit(1)
        except Exception as e:
            print(f"[ERROR] {e}")
            import traceback
            if verbose:
                traceback.print_exc()
            sys.exit(1)
    
    try:
        assembler.resolve_labels()
        assembler.write_bexe(output_file)
    except Exception as e:
        print(f"[ERROR] {e}")
        import traceback
        if verbose:
            traceback.print_exc()
        sys.exit(1)