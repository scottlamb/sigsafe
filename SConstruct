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
Export('arch')
os_name = string.replace(string.lower(os.uname()[0]),'-','')
Export('os_name')
type = 'debug'
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
    CPPDEFINES = [
        '_REENTRANT',
        '_THREAD_SAFE',
    ],
    LIBPATH = [
        '#/' + buildDir,
    ]
)

# Flags for threading
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

if os_name != 'freebsd':
    # FreeBSD has at least two problems with this:
    # - NSIG is not visible.
    # - siginfo_t is not visible unless POSIX_SOURCE=200112 also defined.
    #   (That's a bug; it should be implied by _XOPEN_SOURCE=600 but isn't.)
    #
    # I generally like having this defined where possible to make things more
    # standards-like. For example, OSF/1 uses socklen_t when this is set.
    env.Append(CPPDEFINES = ['_XOPEN_SOURCE=600'])

if os_name == 'osf1':
    env.Append(CPPDEFINES = ['_OSF_SOURCE']) # for mcontext_t members

if env['CC'] == 'gcc':
    env.Append(CCFLAGS = ['-Wall'])

if type == 'debug':
    env.Append(
        CPPDEFINES=[
            'ORG_SLAMB_SIGSAFE_DEBUG_SIGNAL', # write [S] to stderr whenever
                                              # safe signal received
            'ORG_SLAMB_SIGSAFE_DEBUG_JUMP',   # write [J] to stderr whenever
                                              # jumping from sighandler
        ],
        CCFLAGS=['-g'],
        LINKFLAGS=['-g'],
    )
else:
    env.Append(
        CPPDEFINES=['NDEBUG'], # turn assertions off
        CCFLAGS=['-O'],
        LINKFLAGS=['-O'],
    )

conf = env.Configure()

# Manually excluding Darwin from poll() because they have a poll() that
# is emulated poorly with select().
if os_name != 'darwin':
    conf.env.Append(CPPDEFINES = ['HAVE_POLL'])

if conf.CheckFunc('kevent'):
    conf.env.Append(CPPDEFINES = ['HAVE_KEVENT'])

if conf.CheckFunc('epoll_wait'):
    conf.env.Append(CPPDEFINES = ['HAVE_EPOLL'])

if conf.CheckHeader('stdint.h'):
    conf.env.Append(CPPDEFINES = ['SIGSAFE_HAVE_STDINT_H'])

env = conf.Finish()
Export('env')
BuildDir(buildDir, 'src', 0)
SConscript(buildDir + '/SConscript')
SConscript('tests/SConscript')
