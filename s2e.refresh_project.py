#!/usr/bin/python

from subprocess import Popen, PIPE
import mimetypes
import sys
import os

blacklist = [
    'experiments/',
    'klee/',
    'linux/',
    'Makefile/',
    'Projects/',
    'qemu-0.9.1/',
    'S2EPlugins/',
    'scripts/',
    'setup_scripts/',
    'template_net/',
    'windows/',
    'qemu-0.12.2-src/target-alpha/',
    'qemu-0.12.2-src/target-arm/',
    'qemu-0.12.2-src/target-cris/',
    'qemu-0.12.2-src/target-m68k/',
    'qemu-0.12.2-src/target-microblaze/',
    'qemu-0.12.2-src/target-mips/',
    'qemu-0.12.2-src/target-ppc/',
    'qemu-0.12.2-src/target-s390x/',
    'qemu-0.12.2-src/target-sh4/',
    'qemu-0.12.2-src/target-sparc/',
    'qemu-0.12.2-src/darwin-user/',
    'qemu-0.12.2-src/bsd-user/',
    'qemu-0.12.2-src/pc-bios/',
    'qemu-0.12.2-src/roms/',
    'qemu-0.12.2-src/tests/',
    'qemu-0.12.2-src/tcg/arm/',
    'qemu-0.12.2-src/tcg/hppa/',
    'qemu-0.12.2-src/tcg/i386/',
    'qemu-0.12.2-src/tcg/mips/',
    'qemu-0.12.2-src/tcg/ppc/',
    'qemu-0.12.2-src/tcg/ppc64/',
    'qemu-0.12.2-src/tcg/s390/',
    'qemu-0.12.2-src/tcg/sparc/',
]

if os.path.isdir('.git'):
    git_files = Popen(['git', 'ls-files'],
                  stdout=PIPE).communicate()[0].split('\n')
elif os.path.isdir('.svn'):
    git_files = Popen(['svn', 'ls', '--recursive'],
                  stdout=PIPE).communicate()[0].split('\n')
else:
    sys.stderr.write('Found neither .git nor .svn subfolder\n')
    sys.exit(1)

git_files.sort()

dirs = set([""])
s2e_files = open('s2e.files', 'w')
s2e_includes = open('s2e.includes', 'w')
for fname in git_files:
    for b in blacklist:
        if fname.startswith(b):
            break
    else:
        if not os.path.isdir(fname):
            if mimetypes.guess_type(fname)[0] is not None:
                s2e_files.write(fname + '\n')

            fdir = fname
            while fdir != "":
                fdir = os.path.dirname(fdir)
                if fdir not in dirs and os.path.isdir(fdir):
                    s2e_includes.write(fdir + '\n')
                    dirs.add(fdir)

s2e_includes.write('\n'.join([
    '../llvm-2.6/include',
    '../llvm-build/include',
    '../stp-build/include',
    '../klee-build/include',
    '../qemu-build',
    '../qemu-build/i386-s2e-softmmu',
    '/usr/include/sigc++-2.0'
]))

s2e_files.close()
s2e_includes.close()
