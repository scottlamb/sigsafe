# vim: ft=python
# $Id$
# Copyright (C) 2004 Scott Lamb <slamb@slamb.org>.
# This file is part of sigsafe, which is released under the MIT license.

import os
import re

# Determine our platform
arch = os.uname()[4]
if arch == 'Power Macintosh': arch = 'ppc'
if re.compile('i[3456]86').match(arch): arch = 'i386'
Export('arch')
os_name = os.uname()[0].lower().replace('-','')
Export('os_name')
type = 'debug'
buildDir = 'build-%s-%s-%s' % (arch, os_name, type)

env = Environment(
    CPP = 'cpp3',
    CPPPATH = [
        '#/src',
        '#/src/' + arch + '-' + os_name
    ],
    CPPDEFINES = [
        '_REENTRANT',
        '_THREAD_SAFE',
    ],
    LIBPATH = [
        '#/' + buildDir
    ]
)

if os_name == 'freebsd':
    if True:
        # use LinuxThreads
        env.Append(
            CPPPATH = ['/usr/local/include/pthread/linuxthreads'],
            LIBPATH = ['/usr/local/lib'],
            LIBS = ['lthread'],
        )
    else:
        # use built-in user pthreads
        # DON'T USE ON 4.X.
        env.Append(CCFLAGS = ['-pthread'])
        env.Append(LINKFLAGS = ['-pthread'])
else:
    env.Append(LIBS = ['pthread'])

if env['CC'] == 'gcc':
    env.Append(CCFLAGS = ['-Wall'])
    if type == 'debug':
        env.Append(CCFLAGS=['-g'], LINKFLAGS=['-g'])
    else:
        env.Append(CPPDEFINES=['NDEBUG'],CCFLAGS=['-O2'],LINKFLAGS=['-O2'])

if type == 'debug':
    env.Append(CPPDEFINES=[
        'ORG_SLAMB_SIGSAFE_DEBUG_SIGNAL', # write [S] to stderr whenever
                                          # safe signal received
        'ORG_SLAMB_SIGSAFE_DEBUG_JUMP',   # write [J] to stderr whenever
                                          # jumping from sighandler
    ])

conf = env.Configure()

# Manually excluding Darwin from poll() because they have a poll() that
# is emulated poorly with select().
if os_name != 'darwin':
    conf.env.Append(CPPDEFINES = ['HAVE_POLL'])

if conf.CheckFunc('kevent'):
    conf.env.Append(CPPDEFINES = ['HAVE_KEVENT'])

if conf.CheckFunc('epoll_wait'):
    conf.env.Append(CPPDEFINES = ['HAVE_EPOLL'])

env = conf.Finish()
Export('env')
BuildDir(buildDir, 'src', 0)
SConscript(buildDir + '/SConscript')
SConscript('tests/SConscript')
