#!/usr/bin/env python3
"""ACME -> ca65 source converter for the PETSCII Robots X16 sources.

The X16 sources use a very small slice of ACME: !BYTE !WORD !SCR !PET !BINARY
!SOURCE !to !cpu !ifndef !eof, plus bare INC/DEC for 65C02 accumulator mode and
a handful of labels written without a colon.  Everything else -- the <>/lo-hi
operators, $ and % literals, comments -- is already ca65 syntax.

Reports every line it could not handle so the residue is countable.
"""
import re
import sys

# ---- ACME string encodings ---------------------------------------------------

def scr(ch):
    """ACME !scr: ASCII/PETSCII byte -> C64 screen code."""
    c = ord(ch)
    if   c <= 0x1f: return c + 0x80
    elif c <= 0x3f: return c
    elif c <= 0x5f: return c - 0x40   # @A-Z[\]^_  -> $00-$1F
    elif c <= 0x7f: return c - 0x60   # `a-z{|}~    -> $00-$1F  (verified against
                                      # X16ROBOTS.PRG: "searching" = 13 05 01 12 ...)
    elif c <= 0x9f: return c + 0x40
    elif c <= 0xbf: return c - 0x40
    elif c <= 0xfe: return c - 0x80
    return 0x5e

def pet(ch):
    """ACME !pet: ASCII -> PETSCII (case swapped)."""
    c = ord(ch)
    if   0x41 <= c <= 0x5a: return c + 0x80   # A-Z -> $C1-$DA
    elif 0x61 <= c <= 0x7a: return c - 0x20   # a-z -> $41-$5A
    return c

# ---- line rewriting ----------------------------------------------------------

LABEL_NO_COLON = re.compile(r'^([A-Za-z_][A-Za-z0-9_]*)(\s+)(?=[!.$%A-Za-z])')
STRING_DIRECTIVE = re.compile(r'!(SCR|PET|TEXT|scr|pet|text)\s*(".*)$')

def split_comment(line):
    out, in_str, i = [], False, 0
    while i < len(line):
        c = line[i]
        if c == '"':
            in_str = not in_str
        elif c == ';' and not in_str:
            return line[:i], line[i:]
        out.append(c)
        i += 1
    return line, ''

def encode_operand(rest, enc):
    """Turn `"abc",13,"d"` into a list of byte values."""
    vals, i = [], 0
    while i < len(rest):
        c = rest[i]
        if c == '"':
            j = rest.index('"', i + 1)
            vals += [str(enc(ch)) for ch in rest[i + 1:j]]
            i = j + 1
        elif c in ', \t':
            i += 1
        else:
            j = i
            while j < len(rest) and rest[j] != ',':
                j += 1
            tok = rest[i:j].strip()
            if tok:
                vals.append(tok)
            i = j
    return vals

def convert_line(line, unhandled):
    code, comment = split_comment(line.rstrip('\n'))

    # label written without a colon -> add one
    m = LABEL_NO_COLON.match(code)
    if m and not re.match(r'^\s', code):
        kw = m.group(1).upper()
        if kw not in ('JSR', 'JMP', 'LDA', 'STA', 'LDX', 'STX', 'LDY', 'STY',
                      'CMP', 'CPX', 'CPY', 'BIT', 'AND', 'ORA', 'EOR', 'ADC',
                      'SBC', 'INC', 'DEC', 'ASL', 'LSR', 'ROL', 'ROR', 'PHA',
                      'PLA', 'RTS', 'RTI', 'NOP', 'SEC', 'CLC', 'SEI', 'CLI',
                      'TAX', 'TAY', 'TXA', 'TYA', 'PHX', 'PLX', 'PHY', 'PLY',
                      'STZ', 'BRA', 'TSX', 'TXS', 'PHP', 'PLP', 'CLD', 'SED'):
            code = m.group(1) + ':' + m.group(2) + code[m.end():]

    # a bare label alone on its own line: ACME allows it, ca65 wants the colon
    if re.match(r'^[A-Za-z_][A-Za-z0-9_]*$', code) and not code.startswith(' '):
        code += ':'

    s = code.strip()

    if s.startswith('!to'):
        return ('; [ca65: output name comes from the linker config] ' + s + comment)
    if s.startswith('!cpu'):
        return '.setcpu "65C02"' + comment
    if s.startswith('*='):
        return '.org ' + s[2:].strip() + comment
    # ACME allows the filename to butt straight against the pseudo-op: !BINARY"x"
    m = re.match(r'^!(SOURCE|source)\s*(.*)$', s)
    if m:
        return '.include ' + m.group(2).strip() + comment
    m = re.match(r'^!(BINARY|binary)\s*(.*)$', s)
    if m:
        return '.incbin ' + m.group(2).strip() + comment
    if s.startswith('!ifndef'):
        # "!ifndef SYM !eof" stops assembly when SYM is undefined, so everything
        # below is conditional on SYM being DEFINED. The .endif is appended at EOF.
        parts = s.split()
        unhandled.append(('__ifdef__', parts[1]))
        return '.ifdef ' + parts[1] + comment
    if s.startswith('!eof'):
        return '; [ca65: !eof folded into the .ifndef above]' + comment

    # data directives, possibly after a label
    m = re.match(r'^(\S*:?\s*)!(BYTE|byte|WORD|word)\s*(.*)$', code.strip('\n'))
    if m:
        lead, kind, rest = m.group(1), m.group(2).lower(), m.group(3)
        return f'{lead}.{kind} {rest}'.rstrip() + comment

    m = re.match(r'^(\S*:?\s*)!(SCR|scr|PET|pet|TEXT|text)\s*(.*)$', code.strip('\n'))
    if m:
        lead, kind, rest = m.group(1), m.group(2).lower(), m.group(3)
        enc = scr if kind == 'scr' else pet
        return f'{lead}.byte {",".join(encode_operand(rest, enc))}'.rstrip() + comment

    # 65C02 accumulator-mode INC/DEC: ACME writes them bare, ca65 wants "inc a"
    m = re.match(r'^(\s*)(INC|DEC)\s*$', code)
    if m:
        return f'{m.group(1)}{m.group(2).lower()} a{comment}'

    # ACME sizes an operand by how the literal is written, so "STA $00C6" is
    # absolute there and zero page in ca65. Force absolute with ca65's a: prefix.
    code = re.sub(r'\b(?!JMP|JSR|jmp|jsr)([A-Za-z]{3})(\s+)(#?)(\$00[0-9A-Fa-f]{2})\b',
                  lambda m: m.group(1) + m.group(2) + m.group(3) +
                            ('a:' if not m.group(3) else '') + m.group(4), code)

    # ACME tolerates a stray colon on a jump target; ca65 does not
    code = re.sub(r'\b(JMP|JSR)(\s+)([A-Za-z_][A-Za-z0-9_]*):', r'\1\2\3', code)

    if '!' in code and not code.lstrip().startswith(';'):
        unhandled.append(code)

    return code + comment


def main():
    src = sys.argv[1]
    unhandled = []
    out = []
    for line in open(src, encoding='latin-1'):
        out.append(convert_line(line, unhandled))
    pending = [u for u in unhandled if isinstance(u, tuple)]
    unhandled[:] = [u for u in unhandled if not isinstance(u, tuple)]
    for _ in pending:
        out.append('.endif ; [ca65: was !eof]')
    sys.stdout.write('\n'.join(out) + '\n')
    for u in unhandled:
        print(f'; UNHANDLED: {u}', file=sys.stderr)
    print(f'{src}: {len(out)} lines, {len(unhandled)} unhandled', file=sys.stderr)

main()
