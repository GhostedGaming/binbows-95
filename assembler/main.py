import sys
import struct
import re
import os

# =========================
# Opcodes
# =========================
MNEMONIC_TO_OPCODE = {
    # Data movement
    "mv":   0x01, "mov":  0x01,
    "lea":  0x1A,

    # Arithmetic
    "ad":   0x02, "add":  0x02,
    "su":   0x03, "sub":  0x03,
    "mu":   0x04, "mul":  0x04,
    "di":   0x05, "div":  0x05,
    "inc":  0x12,
    "dec":  0x13,
    "cmp":  0x10, "xor":  0x11,
    "and":  0x14, "or":   0x15,
    "not":  0x16, "neg":  0x17,
    "shl":  0x18, "shr":  0x19,

    # Control flow / jumps
    "jmp":  0x06,
    "je":   0x07, "jz":   0x07,
    "jne":  0x08, "jnz":  0x08,
    "jl":   0x09, "jb":   0x09,
    "jg":   0x0A, "ja":   0x0A,
    "jle":  0x0B, "jbe":  0x0B,
    "jge":  0x0C, "jae":  0x0C,

    # Stack
    "push": 0x20,
    "pop":  0x21,

    # Subroutines
    "call": 0x30,
    "ret":  0x31,

    # System / misc
    "hlt":      0x40,
    "int":      0x50,
    "syscall":  0x50,
    "nop":      0x90,
    
    # Beginner-friendly aliases
    "print":    0x50,  # Maps to syscall
    "exit":     0x40,  # Maps to hlt
    "goto":     0x06,  # Maps to jmp
    "if_equal": 0x07,  # Maps to je
    "if_not_equal": 0x08,  # Maps to jne
    "if_less":  0x09,  # Maps to jl
    "if_greater": 0x0A,  # Maps to jg
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

    # 32-bit registers
    "eax": 0x20, "ebx": 0x21, "ecx": 0x22, "edx": 0x23,
    "esp": 0x24, "ebp": 0x25, "esi": 0x26, "edi": 0x27,

    # 16-bit registers
    "ax": 0x30, "bx": 0x31, "cx": 0x32, "dx": 0x33,
    "sp": 0x34, "bp": 0x35, "si": 0x36, "di": 0x37,

    # 8-bit registers
    "al": 0x10, "bl": 0x11, "cl": 0x12, "dl": 0x13,
    "ah": 0x14, "bh": 0x15, "ch": 0x16, "dh": 0x17,
    
    # Beginner-friendly aliases
    "a": 0x00, "b": 0x01, "c": 0x02, "d": 0x03,
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
MODE_REG_REG = 0x00
MODE_REG_IMM = 0x01
MODE_MEM_IMM = 0x02
MODE_MEM_REG = 0x03
MODE_REG_MEM = 0x04

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
        self.include_stack = []

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

        # Check syscalls
        if expr in SYSCALLS:
            return SYSCALLS[expr]

        # Check constants
        if expr in self.constants:
            return self.constants[expr]

        # Try direct integer parsing
        try:
            return int(expr, 0)
        except:
            pass

        # Replace constants with values
        for const_name, const_val in self.constants.items():
            expr = re.sub(r'\b' + re.escape(const_name) + r'\b', str(const_val), expr)

        # Try evaluation
        try:
            # Safe eval with limited namespace
            return int(eval(expr, {"__builtins__": {}}, {}))
        except:
            return None

    # -------------------------
    # Size / Directive Detection
    # -------------------------
    def is_size_directive(self, op):
        op = op.strip()
        m = re.match(r'^(db|dw|dd|dq|byte\s+ptr|byte|word|dword|qword)\b', op, re.IGNORECASE)
        if not m:
            return None
        token = m.group(1).lower()
        if token == 'word': token = 'dw'
        if token == 'dword': token = 'dd'
        if token == 'qword': token = 'dq'
        if token == 'byte ptr': token = 'byte'
        return token

    def size_to_bytes(self, size_token):
        if not size_token:
            return None
        s = size_token.lower()
        if s in ('db', 'byte'):
            return 1
        if s == 'dw':
            return 2
        if s == 'dd':
            return 4
        if s == 'dq':
            return 8
        return None

    # -------------------------
    # Operand Parsing Helpers
    # -------------------------
    def is_register(self, op):
        op = op.strip().lower()
        if op.startswith('[') and op.endswith(']'):
            return False
        return op in REGISTERS

    def get_register_code(self, op):
        op = op.strip().lower()
        if op.startswith('[') and op.endswith(']'):
            op = op[1:-1].strip()
        return REGISTERS.get(op, 0xFF)

    def is_memory_operand(self, op):
        op = op.strip()
        size = self.is_size_directive(op)
        if size:
            op = re.sub(r'^(db|dw|dd|dq|byte\s+ptr|byte|word|dword|qword)\b\s*', '', op, flags=re.IGNORECASE)
        return op.startswith('[') and op.endswith(']')

    def is_number(self, op):
        op = op.strip()
        if not op:
            return False
        if (op.startswith("'") and op.endswith("'")) or (op.startswith('"') and op.endswith('"')):
            return True
        if op.startswith('-') or op.startswith('+'):
            op = op[1:]
        return op.startswith('0x') or op.startswith('0b') or op.startswith('0o') or op.isdigit()

    def parse_number(self, op):
        s = op.strip()
        
        # Character literal
        if (s.startswith("'") and s.endswith("'")) or (s.startswith('"') and s.endswith('"')):
            try:
                b = self.parse_string(s)
                if len(b) == 1:
                    return b[0]
                return None
            except:
                return None

        val = self.eval_expression(s)
        if val is not None:
            return val
            
        try:
            return int(s, 0)
        except:
            return None

    def parse_string(self, s):
        if (s.startswith('"') and s.endswith('"')) or (s.startswith("'") and s.endswith("'")):
            s = s[1:-1]

        # Handle escape sequences
        s = s.replace('\\\\', '\x00')  # Temporary placeholder
        s = s.replace('\\n', '\n')
        s = s.replace('\\r', '\r')
        s = s.replace('\\t', '\t')
        s = s.replace('\\0', '\0')
        s = s.replace('\\a', '\a')
        s = s.replace('\\b', '\b')
        s = s.replace('\\f', '\f')
        s = s.replace('\\v', '\v')
        s = s.replace('\\"', '"')
        s = s.replace("\\'", "'")
        s = s.replace('\x00', '\\')  # Restore backslash
        
        return s.encode('utf-8')

    def parse_memory_operand(self, raw_mem):
        if not (raw_mem.startswith('[') and raw_mem.endswith(']')):
            self.error(f"Invalid memory operand: {raw_mem}")
        inner = raw_mem[1:-1].strip()
        
        # Handle negative numbers and subtraction
        parts = []
        current = ""
        i = 0
        while i < len(inner):
            if inner[i] == '+':
                if current.strip():
                    parts.append(current.strip())
                current = ""
                i += 1
            elif inner[i] == '-' and (i == 0 or inner[i-1] in '+'):
                # Start of negative number
                current += '-'
                i += 1
            else:
                current += inner[i]
                i += 1
        if current.strip():
            parts.append(current.strip())

        base = None
        index = None
        disp = 0
        disp_label = None

        for p in parts:
            p = p.strip()
            if not p:
                continue
                
            if p.lower() in REGISTERS:
                if base is None:
                    base = REGISTERS[p.lower()]
                elif index is None:
                    index = REGISTERS[p.lower()]
                else:
                    self.error(f"Too many registers in memory operand: {raw_mem}")
            else:
                num = self.parse_number(p)
                if num is not None:
                    disp += num
                else:
                    if disp_label is not None:
                        self.error(f"Multiple labels in memory operand: {raw_mem}")
                    disp_label = p

        result = {
            'base': base if base is not None else 0xFF,
            'index': index if index is not None else 0xFF,
            'disp': disp,
            'disp_label': disp_label
        }
        return result

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
            if count is None:
                self.error(f"Invalid count for {directive}: {operands}")
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
                    if num is None:
                        self.error(f"Invalid numeric value in db: {val}")
                    self.data.append(num & 0xFF)

            elif directive == "dw":
                num = self.parse_number(val)
                if num is None:
                    self.error(f"Invalid numeric value in dw: {val}")
                self.data.extend(struct.pack("<H", num & 0xFFFF))

            elif directive == "dd":
                num = self.parse_number(val)
                if num is None:
                    self.error(f"Invalid numeric value in dd: {val}")
                self.data.extend(struct.pack("<I", num & 0xFFFFFFFF))

            elif directive == "dq":
                num = self.parse_number(val)
                if num is None:
                    self.error(f"Invalid numeric value in dq: {val}")
                self.data.extend(struct.pack("<Q", num & 0xFFFFFFFFFFFFFFFF))
            elif directive == "byte":
                num = self.parse_number(val)
                if num is None:
                    self.error(f"Invalid numeric value in byte: {val}")
                self.data.append(num & 0xFF)
            else:
                self.error(f"Unknown data directive: {directive}")

    def split_operands(self, operands):
        result = []
        current = ""
        in_string = False
        quote_char = None
        bracket_depth = 0

        for char in operands:
            if char in ('"', "'") and (not in_string or char == quote_char):
                in_string = not in_string
                quote_char = char if in_string else None
                current += char
            elif char == '[' and not in_string:
                bracket_depth += 1
                current += char
            elif char == ']' and not in_string:
                bracket_depth -= 1
                current += char
            elif char == ',' and not in_string and bracket_depth == 0:
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
        val = self.parse_number(value)
        if val is None:
            self.error(f"Invalid EQU value: {value}")
        self.constants[name] = val
        self.log(f"Constant '{name}' = {self.constants[name]}")

    def handle_include(self, line):
        parts = line.split(None, 1)
        if len(parts) < 2:
            self.error("%include requires a filename")
        
        filename = parts[1].strip()
        # Remove quotes if present
        if (filename.startswith('"') and filename.endswith('"')) or \
           (filename.startswith("'") and filename.endswith("'")):
            filename = filename[1:-1]
        
        # Check for circular includes
        if filename in self.include_stack:
            self.error(f"Circular include detected: {filename}")
        
        # Try to find the file
        if not os.path.exists(filename):
            # Try relative to current file
            if self.current_file:
                base_dir = os.path.dirname(self.current_file)
                alt_path = os.path.join(base_dir, filename)
                if os.path.exists(alt_path):
                    filename = alt_path
                else:
                    self.error(f"Include file not found: {filename}")
            else:
                self.error(f"Include file not found: {filename}")
        
        self.log(f"Including file: {filename}")
        self.include_stack.append(filename)
        
        try:
            with open(filename, "r", encoding="utf-8") as f:
                old_file = self.current_file
                old_line = self.current_line
                self.parse_file(f.read(), filename)
                self.current_file = old_file
                self.current_line = old_line
        finally:
            self.include_stack.pop()

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
                # Use word boundary replacement to avoid partial matches
                line = re.sub(r'%' + str(i+1) + r'\b', arg, line)
            expanded.append(line)

        return expanded

    # -------------------------
    # Beginner Syntax Handler
    # -------------------------
    def handle_beginner_syntax(self, line):
        """Convert beginner-friendly syntax to standard assembly"""
        line_lower = line.lower().strip()
        
        # ARRAY name[size] or ARRAY name[size] = {values}
        if line_lower.startswith('array '):
            match = re.match(r'array\s+(\w+)\[(\d+)\](?:\s*=\s*\{([^}]+)\})?', line, re.IGNORECASE)
            if match:
                name, size, values = match.groups()
                size = int(size)
                
                if values:
                    # Array with initialization: array nums[5] = {1, 2, 3, 4, 5}
                    vals = [v.strip() for v in values.split(',')]
                    lines = [".data"]
                    lines.append(f"{name}: dd " + ", ".join(vals))
                    # Pad with zeros if needed
                    if len(vals) < size:
                        lines.append(f"dd " + ", ".join(["0"] * (size - len(vals))))
                    return "\n".join(lines)
                else:
                    # Uninitialized array: array buffer[100]
                    return f".bss\n{name}: resd {size}"
        
        # SET variable TO value (case insensitive, flexible spacing)
        if ' to ' in line_lower:
            match = re.match(r'set\s+(.+?)\s+to\s+(.+)', line, re.IGNORECASE)
            if match:
                var, val = match.groups()
                return f"mov {var.strip()}, {val.strip()}"
        
        # PRINT string/variable
        if line_lower.startswith('print '):
            rest = line[6:].strip()
            return f"mov rax, 1\nmov rbx, {rest}\nsyscall"
        
        # LET variable = value (alternative to SET)
        if '=' in line and not line_lower.startswith('mov'):
            match = re.match(r'let\s+(\S+)\s*=\s*(.+)', line, re.IGNORECASE)
            if match:
                var, val = match.groups()
                return f"mov {var.strip()}, {val.strip()}"
            # Handle simple assignment without LET
            match = re.match(r'(\w+)\s*=\s*(.+)', line, re.IGNORECASE)
            if match and not any(line_lower.startswith(x) for x in ['mov', 'add', 'sub', 'mul', 'div', 'cmp', 'and', 'or', 'xor', 'array']):
                var, val = match.groups()
                return f"mov {var.strip()}, {val.strip()}"
        
        return None

    # -------------------------
    # Instruction Encoding
    # -------------------------
    def encode_two_operand_instruction(self, opcode, raw_op1, raw_op2):
        size1 = self.is_size_directive(raw_op1)
        size2 = self.is_size_directive(raw_op2)

        op1 = raw_op1.strip()
        op2 = raw_op2.strip()

        if size1:
            op1 = re.sub(r'^(db|dw|dd|dq|byte\s+ptr|byte|word|dword|qword)\b\s*', '', op1, flags=re.IGNORECASE).strip()
        if size2:
            op2 = re.sub(r'^(db|dw|dd|dq|byte\s+ptr|byte|word|dword|qword)\b\s*', '', op2, flags=re.IGNORECASE).strip()

        self.log(f"Encoding two-operand: opcode=0x{opcode:02X}, op1='{raw_op1}'->'{op1}', op2='{raw_op2}'->'{op2}'")

        if self.is_register(op1):
            dst_reg = self.get_register_code(op1)
            self.code.append(opcode)
            self.code.append(dst_reg)

            if self.is_register(op2):
                self.code.append(MODE_REG_REG)
                src_reg = self.get_register_code(op2)
                self.code.append(src_reg)
                self.log(f"Encoded: REG[{dst_reg}] <- REG[{src_reg}]")
                return

            if self.is_memory_operand(raw_op2) or self.is_memory_operand(op2):
                self.code.append(MODE_REG_MEM)
                mem_token = raw_op2 if self.is_memory_operand(raw_op2) else op2
                mem = self.parse_memory_operand(mem_token)
                self.code.append(mem['base'])
                self.code.append(mem['index'])
                if mem['disp_label']:
                    patch_offset = len(self.code)
                    self.unresolved_refs.append((patch_offset, mem['disp_label'], 'abs', 4))
                    self.code.extend(b'\x00\x00\x00\x00')
                else:
                    self.code.extend(struct.pack("<i", mem['disp']))
                self.log(f"Encoded: REG[{dst_reg}] <- MEM")
                return

            self.code.append(MODE_REG_IMM)
            imm_size = self.size_to_bytes(size2) or 4
            if self.is_number(op2):
                imm = self.parse_number(op2)
                if imm is None:
                    self.error(f"Invalid immediate: {op2}")
                if imm_size == 1:
                    self.code.extend(struct.pack("<B", imm & 0xFF))
                elif imm_size == 2:
                    self.code.extend(struct.pack("<H", imm & 0xFFFF))
                elif imm_size == 4:
                    self.code.extend(struct.pack("<I", imm & 0xFFFFFFFF))
                elif imm_size == 8:
                    self.code.extend(struct.pack("<Q", imm & 0xFFFFFFFFFFFFFFFF))
                self.log(f"Encoded: REG[{dst_reg}] <- IMM 0x{imm:X}")
            else:
                patch_offset = len(self.code)
                self.unresolved_refs.append((patch_offset, op2, 'abs', 4))
                self.code.extend(b'\x00\x00\x00\x00')
                self.log(f"Unresolved reference to '{op2}'")
            return

        if self.is_memory_operand(raw_op1) or self.is_memory_operand(op1):
            self.code.append(opcode)
            mem_token = raw_op1 if self.is_memory_operand(raw_op1) else op1
            mem = self.parse_memory_operand(mem_token)
            
            if self.is_register(op2):
                self.code.append(MODE_MEM_REG)
                self.code.append(mem['base'])
                self.code.append(mem['index'])
                if mem['disp_label']:
                    patch_offset = len(self.code)
                    self.unresolved_refs.append((patch_offset, mem['disp_label'], 'abs', 4))
                    self.code.extend(b'\x00\x00\x00\x00')
                else:
                    self.code.extend(struct.pack("<i", mem['disp']))
                src_reg = self.get_register_code(op2)
                self.code.append(src_reg)
                self.log(f"Encoded: MEM <- REG[{src_reg}]")
                return

            if self.is_number(op2) or (not self.is_register(op2) and not self.is_memory_operand(op2)):
                self.code.append(MODE_MEM_IMM)
                self.code.append(mem['base'])
                self.code.append(mem['index'])
                if mem['disp_label']:
                    patch_offset = len(self.code)
                    self.unresolved_refs.append((patch_offset, mem['disp_label'], 'abs', 4))
                    self.code.extend(b'\x00\x00\x00\x00')
                else:
                    self.code.extend(struct.pack("<i", mem['disp']))
                
                imm_size = self.size_to_bytes(size1) or self.size_to_bytes(size2) or 4
                if self.is_number(op2):
                    imm = self.parse_number(op2)
                    if imm is None:
                        self.error(f"Invalid immediate: {op2}")
                    if imm_size == 1:
                        self.code.extend(struct.pack("<B", imm & 0xFF))
                    elif imm_size == 2:
                        self.code.extend(struct.pack("<H", imm & 0xFFFF))
                    elif imm_size == 4:
                        self.code.extend(struct.pack("<I", imm & 0xFFFFFFFF))
                    elif imm_size == 8:
                        self.code.extend(struct.pack("<Q", imm & 0xFFFFFFFFFFFFFFFF))
                    self.log(f"Encoded: MEM <- IMM 0x{imm:X}")
                else:
                    patch_offset = len(self.code)
                    self.unresolved_refs.append((patch_offset, op2, 'abs', 4))
                    self.code.extend(b'\x00\x00\x00\x00')
                    self.log(f"Unresolved reference to '{op2}'")
                return

        self.error(f"Unsupported operand combination: {raw_op1}, {raw_op2}")

    def encode_one_operand_instruction(self, opcode, raw_op):
        op = raw_op.strip()
        self.code.append(opcode)

        size = self.is_size_directive(op)
        if size:
            op = re.sub(r'^(db|dw|dd|dq|byte\s+ptr|byte|word|dword|qword)\b\s*', '', op, flags=re.IGNORECASE).strip()

        if self.is_register(op):
            reg = self.get_register_code(op)
            self.code.append(reg)
            self.log(f"Encoded: single operand REG[{reg}]")
        elif self.is_memory_operand(op):
            mem = self.parse_memory_operand(op)
            self.code.append(mem['base'])
            self.code.append(mem['index'])
            if mem['disp_label']:
                patch_offset = len(self.code)
                self.unresolved_refs.append((patch_offset, mem['disp_label'], 'abs', 4))
                self.code.extend(b'\x00\x00\x00\x00')
            else:
                self.code.extend(struct.pack("<i", mem['disp']))
            self.log(f"Encoded: single operand MEM")
        elif self.is_number(op):
            imm = self.parse_number(op)
            if imm is None:
                self.error(f"Invalid immediate: {op}")
            self.code.extend(struct.pack("<I", imm & 0xFFFFFFFF))
            self.log(f"Encoded: single operand IMM 0x{imm:X}")
        else:
            patch_offset = len(self.code)
            self.unresolved_refs.append((patch_offset, op, 'abs', 4))
            self.code.extend(b'\x00\x00\x00\x00')
            self.log(f"Unresolved reference to '{op}'")

    # -------------------------
    # Parse Line
    # -------------------------
    def parse_line(self, line):
        line = line.split(';', 1)[0].strip()
        if not line:
            return None

        # Check for beginner syntax
        beginner = self.handle_beginner_syntax(line)
        if beginner:
            for bline in beginner.split('\n'):
                self.parse_line(bline)
            return None

        if line.startswith('%include'):
            self.handle_include(line)
            return None

        if line.startswith('%'):
            return line

        if ' equ ' in line.lower():
            self.handle_equ(line)
            return None

        # Check for section directives
        if line.startswith('.') and ':' not in line:
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
            if ' ' in line or '\t' in line:
                parts = line.split(None, 1)
                directive = parts[0].lower()
                operands = parts[1] if len(parts) > 1 else ""
                
                if directive in ('db', 'dw', 'dd', 'dq', 'byte', 'word', 'dword', 'qword',
                                 'resb', 'resw', 'resd', 'resq'):
                    self.parse_data_directive(directive, operands)
                else:
                    self.error(f"Unknown data directive: {directive}")
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
        if mnemonic in ("ret", "hlt", "nop", "exit"):
            self.code.append(opcode)
            self.log(f"Encoded: {mnemonic}")
            return None

        # INT/SYSCALL
        if mnemonic in ("int", "syscall", "print"):
            self.code.append(opcode)

            if not operands:
                if mnemonic in ("syscall", "print"):
                    self.code.append(0x80)
                    self.log(f"Encoded: {mnemonic} -> int 0x80")
                else:
                    self.error(f"int requires an operand")
            else:
                if operands in SYSCALLS:
                    self.code.append(0x80)
                    self.log(f"Encoded: {mnemonic} {operands} -> int 0x80")
                else:
                    val = self.parse_number(operands)
                    if val is None:
                        self.error(f"Invalid interrupt number: {operands}")
                    self.code.append(val & 0xFF)
                    self.log(f"Encoded: int 0x{val:02X}")
            return None

        # Parse operands
        if not operands:
            self.error(f"{mnemonic} requires operands")

        ops = self.split_operands(operands)

        # Two-operand instructions
        if mnemonic in ("mv", "mov", "ad", "add", "su", "sub", "mu", "mul", "di", "div", 
                        "cmp", "xor", "and", "or", "shl", "shr", "lea"):
            if len(ops) != 2:
                self.error(f"{mnemonic} requires exactly 2 operands")
            self.encode_two_operand_instruction(opcode, ops[0], ops[1])

        # One-operand instructions
        elif mnemonic in ("push", "pop", "jmp", "je", "jz", "jne", "jnz", "jl", "jb", 
                          "jg", "ja", "jle", "jbe", "jge", "jae", "call", "inc", "dec",
                          "not", "neg", "goto", "if_equal", "if_not_equal", "if_less", "if_greater"):
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

        for offset, label, ref_type, size_bytes in list(self.unresolved_refs):
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

            # Write patch
            if size_bytes == 1:
                self.code[offset:offset+1] = struct.pack("<B", addr & 0xFF)
            elif size_bytes == 2:
                self.code[offset:offset+2] = struct.pack("<H", addr & 0xFFFF)
            elif size_bytes == 4:
                self.code[offset:offset+4] = struct.pack("<I", addr & 0xFFFFFFFF)
            elif size_bytes == 8:
                self.code[offset:offset+8] = struct.pack("<Q", addr & 0xFFFFFFFFFFFFFFFF)
            else:
                self.error(f"Unsupported patch size: {size_bytes}")
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
            for i in range(0, min(len(self.code), 128), 16):
                hex_str = ' '.join(f'{b:02x}' for b in self.code[i:i+16])
                print(f"  {i:04x}: {hex_str}")

            if len(self.data) > 0:
                print(f"\nData section hex dump:")
                for i in range(0, min(len(self.data), 128), 16):
                    hex_str = ' '.join(f'{b:02x}' for b in self.data[i:i+16])
                    ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in self.data[i:i+16])
                    print(f"  {i:04x}: {hex_str:48s} | {ascii_str}")

            if self.labels:
                print(f"\nText labels:")
                for label, addr in sorted(self.labels.items(), key=lambda x: x[1]):
                    print(f"  {label:20s} = 0x{addr:04x}")
            
            if self.data_labels:
                print(f"\nData labels:")
                for label, addr in sorted(self.data_labels.items(), key=lambda x: x[1]):
                    actual_addr = len(self.code) + addr
                    print(f"  {label:20s} = 0x{actual_addr:04x} (data+{addr})")
            
            if self.bss_labels:
                print(f"\nBSS labels:")
                for label, addr in sorted(self.bss_labels.items(), key=lambda x: x[1]):
                    actual_addr = len(self.code) + len(self.data) + addr
                    print(f"  {label:20s} = 0x{actual_addr:04x} (bss+{addr})")

# -------------------------
# Main
# -------------------------
if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"BinBows 95 BEXE Assembler v1.1")
        print(f"Usage: {sys.argv[0]} [options] <out.bexe> <in.asm> [more.asm...]")
        print(f"\nOptions:")
        print(f"  -v, --verbose    Enable verbose debug output")
        print(f"\nFeatures:")
        print(f"  • Full x86-style assembly syntax")
        print(f"  • Memory operands: [reg], [reg+offset], [reg+reg+offset]")
        print(f"  • Size directives: byte, word, dword, qword, db, dw, dd, dq")
        print(f"  • Labels in .text, .data, and .bss sections")
        print(f"  • Constants via EQU directive")
        print(f"  • Macros with parameters")
        print(f"  • File inclusion with %include")
        print(f"  • Expression evaluation in immediates")
        print(f"  • Character literals and escape sequences")
        print(f"\nBeginner-friendly syntax:")
        print(f"  • SET variable TO value  (converts to mov)")
        print(f"  • PRINT message          (converts to syscall)")
        print(f"  • EXIT                   (converts to hlt)")
        print(f"  • GOTO label             (converts to jmp)")
        print(f"  • IF_EQUAL label         (converts to je)")
        print(f"  • Simple register names: a, b, c, d (map to rax, rbx, rcx, rdx)")
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

    print(f"BinBows 95 BEXE Assembler v1.1")
    print(f"=" * 50)

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