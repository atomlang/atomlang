# import
import os, sys, re
from os import listdir
from os.path import (
  join, abspath, dirname, relpath)

THIS_PATH = abspath(dirname(__file__))

def to_abs_paths(sources):
  return map(lambda s: os.path.join(THIS_PATH, s), sources)

def to_tolevel_path(path):
  return relpath(path, join(THIS_PATH, '..'))

HASH_CHECK_LIST = [
  "../src/pk_core.c",
  "../src/pk_var.c",
]

CHECK_EXTENTIONS = ('.c', '.h', '.py', '.pk', '.js')

ALLOW_LONG = ('http://', 'https://', '<script ', '<link ')

SOURCE_DIRS = [
  "../src/",
  "../cli/",

  "../docs/",
  "../docs/try/",
]

COMMON_HEADERS = [
  "../src/pk_common.h",
  "../cli/common.h",
]

checks_failed = False

def main():
  check_fnv1_hash(to_abs_paths(HASH_CHECK_LIST))
  check_static(to_abs_paths(SOURCE_DIRS))
  check_common_header_match(to_abs_paths(COMMON_HEADERS))
  if checks_failed:
    sys.exit(1)
  print("Static checks were passed.")

def check_fnv1_hash(sources):
  PATTERN = r'CHECK_HASH\(\s*"([A-Za-z0-9_]+)"\s*,\s*(0x[0-9abcdef]+)\)'
  for file in sources:
    fp = open(file, 'r')
    
    line_no = 0
    for line in fp.readlines():
      line_no += 1
      match = re.findall(PATTERN, line)
      if len(match) == 0: continue
      name, val = match[0]
      hash = hex(fnv1a_hash(name))
      
      if val == hash: continue
      
      file_path = to_tolevel_path(file)
      report_error(f"{location(file_path, line_no)} - hash mismatch. "
        f"hash('{name}') = {hash} not {val}")
        
    fp.close()

def check_static(dirs):
  for dir in dirs:
    
    for file in listdir(dir):
      if not file.endswith(CHECK_EXTENTIONS): continue
      if os.path.isdir(join(dir, file)): continue
      
      fp = open(join(dir, file), 'r')
      
      file_path = to_tolevel_path(join(dir, file))

      is_last_empty = False; line_no = 0
      for line in fp.readlines():
        line_no += 1; line = line[:-1] 
        
        if '\t' in line:
          _location = location(file_path, line_no)
          report_error(f"{_location} - contains tab(s) ({repr(line)}).")
            
        if len(line) >= 80:
          skip = False
          for ignore in ALLOW_LONG:
            if ignore in line: skip = True; break
          if skip: continue
          
          _location = location(file_path, line_no)
          report_error(
            f"{_location} - contains {len(line)} (> 79) characters.")
            
        if line.endswith(' '):
          _location = location(file_path, line_no)
          report_error(f"{_location} - contains trailing white space.")
            
        if line == '':
          if is_last_empty:
            _location = location(file_path, line_no)
            report_error(f"{_location} - consequent empty lines.")
          is_last_empty = True
        else:
          is_last_empty = False
        
      fp.close()

def check_common_header_match(headers):
  headers = list(headers)
  assert len(headers) >= 1
  
  content = ''
  with open(headers[0], 'r') as f:
    content = f.read()
  
  for i in range(1, len(headers)):
    with open(headers[i], 'r') as f:
      if f.read() != content:
        main_header = to_tolevel_path(headers[0])
        curr_header = to_tolevel_path(headers[i])
        report_error("File content mismatch: \"%s\" and \"%s\"\n"
                     "    These files contants should be the same."
                     %(main_header, curr_header))

def location(file, line):
  return f"{'%-17s'%file} : {'%4s'%line}"

def fnv1a_hash(string):
  FNV_prime_32_bit = 16777619
  FNV_offset_basis_32_bit = 2166136261
  
  hash = FNV_offset_basis_32_bit
  
  for c in string:
    hash ^= ord(c)
    hash *= FNV_prime_32_bit
    hash &= 0xffffffff 
  return hash

def report_error(msg):
  global checks_failed
  checks_failed = True
  print(msg, file=sys.stderr)

if __name__ == '__main__':
  main()