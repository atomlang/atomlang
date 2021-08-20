import os, sys, platform
import subprocess, json, re
from os.path import join, abspath, dirname, relpath

THIS_PATH = abspath(dirname(__file__))

TEST_SUITE = {
  "Unit Tests": (
    "lang/basicsatml",
    "lang/classatml",
    "lang/coreatml",
    "lang/controlflowatml",
    "lang/fibersatml",
    "lang/functionsatml",
    "lang/importatml",
  ),

  "Examples": (
    "examples/brainfuckatml",
    "examples/fibatml",
    "examples/fizzbuzzatml",
    "examples/helloworldatml",
    "examples/piatml",
    "examples/primeatml",
  ),
}

SYSTEM_TO_BINARY_PATH = {
  "Windows": "..\\build\\debug\\bin\\pocket.exe",
  "Linux": "../build/debug/pocket",
  "Darwin": "../build/debug/pocket",
}

tests_failed = False

def main():

  os.system('')
  
  run_all_tests()
  if tests_failed:
    sys.exit(1)

def run_all_tests():
  
  pocket = get_pocket_binary()
  
  for suite in TEST_SUITE:
    print_title(suite)
    for test in TEST_SUITE[suite]:
      path = join(THIS_PATH, test)
      run_test_file(pocket, test, path)

def run_test_file(pocket, test, path):
  FMT_PATH = "%-25s"
  INDENTATION = '  | '
  print(FMT_PATH % test, end='')

  sys.stdout.flush()
  result = run_command([pocket, path])
  if result.returncode != 0:
    print_error('-- Failed')
    err = INDENTATION + result.stderr \
        .decode('utf8')               \
        .replace('\n', '\n' + INDENTATION)
    print_error(err)
  else:
    print_success('-- PASSED')

def get_pocket_binary():
  system = platform.system()
  if system not in SYSTEM_TO_BINARY_PATH:
    error_exit("Unsupported platform %s" % system)

  pocket = abspath(join(THIS_PATH, SYSTEM_TO_BINARY_PATH[system]))
  if not os.path.exists(pocket):
    error_exit("Pocket interpreter not found at: '%s'" % pocket)

  return pocket

def run_command(command):
  return subprocess.run(command,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE)


COLORS = {
    'GREEN'     : '\u001b[32m',
    'YELLOW'    : '\033[93m',
    'RED'       : '\u001b[31m',
    'UNDERLINE' : '\033[4m' ,
    'END'       : '\033[0m' ,
}

def print_error(msg):
  global tests_failed
  tests_failed = True
  for line in msg.splitlines():
    print(COLORS['RED'] + line + COLORS['END'])

def print_success(msg):
  for line in msg.splitlines():
    print(COLORS['GREEN'] + line + COLORS['END'])

def error_exit(msg):
  print("Error:", msg, file=sys.stderr)
  sys.exit(1)

def print_title(title):
  print("----------------------------------")
  print(" %s " % title)
  print("----------------------------------")
  
if __name__ == '__main__':
  main()