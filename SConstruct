# vim: ft=python
# $Id$
# Copyright (C) 2004 Scott Lamb <slamb@slamb.org>.
# This file is part of sigsafe, which is released under the MIT license.

import os
import re
import string

# Determine our platform
arch = os.uname()[4]
if arch == 'Power Macintosh': arch = 'ppc'
if re.compile('i[3456]86').match(arch): arch = 'i386'
if re.compile('sun.*').match(arch): arch = 'sparc'
Export('arch')
os_name = string.replace(string.lower(os.uname()[0]),'-','')
Export('os_name')
type = 'st' # mt (multi-threaded) or st (single-threaded)
debug = 1

buildDir = 'build-%s-%s-%s' % (arch, os_name, type)

env = None
if os_name == 'osf1':
    # gcc doesn't work with pthreads, so force use of the native tools
    env = Environment(tools = ['cc','link','ar','as'])
else:
    env = Environment()

env.Append(
    CPPPATH = [
        '#/src',
        '#/src/' + arch + '-' + os_name
    ],
    LIBPATH = [
        '#/' + buildDir,
    ]
)

if type == 'mt': # enable threading
    env.Append(
        CPPDEFINES = [
            '_REENTRANT',
            '_THREAD_SAFE',
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
            env.Append(
                CCFLAGS = ['-pthread'],
                LINKFLAGS = ['-pthread']
            )
    elif os_name == 'osf1':
        env.Append(
            CCFLAGS = ['-pthread'],
            LINKFLAGS = ['-pthread']
        )
    else:
        env.Append(LIBS = ['pthread'])

# Misc. other flags

if os_name == 'osf1':
    env.Append(CPPDEFINES = [
        '_XOPEN_SOURCE=600',     # use socklen_t
        '_OSF_SOURCE',           # ... but publish mcontext_t members anyway
    ])

if env['CC'] == 'gcc':
    env.Append(CCFLAGS = ['-Wall'])

if debug:
    env.Append(
        CPPDEFINES=[
            'SIGSAFE_DEBUG_SIGNAL', # write [S] to stderr whenever
                                    # safe signal received
            'SIGSAFE_DEBUG_JUMP',   # write [J] to stderr whenever
                                    # jumping from sighandler
        ],
        CCFLAGS=['-g'],
        LINKFLAGS=['-g'],
    )

conf = env.Configure()

if os_name != 'darwin':
    # Darwin poll is emulated through select
    conf.env.Append(CPPDEFINES = ['SIGSAFE_HAVE_POLL'])

if os_name != 'sunos':
    # Solaris select is emulated through poll
    conf.env.Append(CPPDEFINES = ['SIGSAFE_HAVE_SELECT'])

if conf.CheckFunc('kevent'):
    conf.env.Append(CPPDEFINES = ['SIGSAFE_HAVE_KEVENT'])

if conf.CheckFunc('epoll_wait'):
    conf.env.Append(CPPDEFINES = ['SIGSAFE_HAVE_EPOLL'])

if conf.CheckHeader('stdint.h'):
    conf.env.Append(CPPDEFINES = ['SIGSAFE_HAVE_STDINT_H'])

env = conf.Finish()
Export('env')
BuildDir(buildDir, 'src', 0)
SConscript(buildDir + '/SConscript')
SConscript('tests/SConscript')
