#
# QAPI helper library
#
# Copyright IBM, Corp. 2011
#
# Authors:
#  Anthony Liguori <aliguori@us.ibm.com>
#
# This work is licensed under the terms of the GNU GPLv2.
# See the COPYING.LIB file in the top-level directory.

try:
    from collections import OrderedDict
except ImportError:
    #Fallback for Python 2
    from ordereddict import OrderedDict

def tokenize(data):
    while len(data):
        if data[0] in ['{', '}', ':', ',', '[', ']']:
            yield data[0]
            data = data[1:]
        elif data[0] in ' \n':
            data = data[1:]
        elif data[0] == "'":
            data = data[1:]
            string = ''
            while data[0] != "'":
                string += data[0]
                data = data[1:]
            data = data[1:]
            yield string

def parse(tokens):
    if tokens[0] == '{':
        ret = OrderedDict()
        tokens = tokens[1:]
        while tokens[0] != '}':
            key = tokens[0]
            tokens = tokens[1:]

            tokens = tokens[1:] # :

            value, tokens = parse(tokens)

            if tokens[0] == ',':
                tokens = tokens[1:]

            ret[key] = value
        tokens = tokens[1:]
        return ret, tokens
    elif tokens[0] == '[':
        ret = []
        tokens = tokens[1:]
        while tokens[0] != ']':
            value, tokens = parse(tokens)
            if tokens[0] == ',':
                tokens = tokens[1:]
            ret.append(value)
        tokens = tokens[1:]
        return ret, tokens
    else:
        return tokens[0], tokens[1:]

def evaluate(string):
    return parse([x for x in tokenize(string)])[0]

def parse_schema(fp):
    exprs = []
    expr = ''
    expr_eval = None

    for line in fp:
        if line.startswith('#') or line == '\n':
            continue

        if line.startswith(' '):
            expr += line
        elif expr:
            expr_eval = evaluate(expr)
            if 'enum' in expr_eval:
                add_enum(expr_eval['enum'])
            elif 'union' in expr_eval:
                add_enum('%sKind' % expr_eval['union'])
            exprs.append(expr_eval)
            expr = line
        else:
            expr += line

    if expr:
        expr_eval = evaluate(expr)
        if 'enum' in expr_eval:
            add_enum(expr_eval['enum'])
        elif 'union' in expr_eval:
            add_enum('%sKind' % expr_eval['union'])
        exprs.append(expr_eval)

    return exprs

def parse_args(typeinfo):
    for member in typeinfo:
        argname = member
        argentry = typeinfo[member]
        optional = False
        structured = False
        if member.startswith('*'):
            argname = member[1:]
            optional = True
        if isinstance(argentry, OrderedDict):
            structured = True
        yield (argname, argentry, optional, structured)

def de_camel_case(name):
    new_name = ''
    for ch in name:
        if ch.isupper() and new_name:
            new_name += '_'
        if ch == '-':
            new_name += '_'
        else:
            new_name += ch.lower()
    return new_name

def camel_case(name):
    new_name = ''
    first = True
    for ch in name:
        if ch in ['_', '-']:
            first = True
        elif first:
            new_name += ch.upper()
            first = False
        else:
            new_name += ch.lower()
    return new_name

def c_var(name):
    return name.replace('-', '_').lstrip("*")

def c_fun(name):
    return c_var(name).replace('.', '_')

def c_list_type(name):
    return '%sList' % name

def type_name(name):
    if type(name) == list:
        return c_list_type(name[0])
    return name

enum_types = []

def add_enum(name):
    global enum_types
    enum_types.append(name)

def is_enum(name):
    global enum_types
    return (name in enum_types)

def c_type(name):
    if name == 'str':
        return 'char *'
    elif name == 'int':
        return 'int64_t'
    elif name == 'bool':
        return 'bool'
    elif name == 'number':
        return 'double'
    elif type(name) == list:
        return '%s *' % c_list_type(name[0])
    elif is_enum(name):
        return name
    elif name == None or len(name) == 0:
        return 'void'
    elif name == name.upper():
        return '%sEvent *' % camel_case(name)
    else:
        return '%s *' % name

def genindent(count):
    ret = ""
    for i in range(count):
        ret += " "
    return ret

indent_level = 0

def push_indent(indent_amount=4):
    global indent_level
    indent_level += indent_amount

def pop_indent(indent_amount=4):
    global indent_level
    indent_level -= indent_amount

def cgen(code, **kwds):
    indent = genindent(indent_level)
    lines = code.split('\n')
    lines = [indent + x for x in lines]
    return '\n'.join(lines) % kwds + '\n'

def mcgen(code, **kwds):
    return cgen('\n'.join(code.split('\n')[1:-1]), **kwds)

def basename(filename):
    return filename.split("/")[-1]

def guardname(filename):
    guard = basename(filename).rsplit(".", 1)[0]
    for substr in [".", " ", "-"]:
        guard = guard.replace(substr, "_")
    return guard.upper() + '_H'
