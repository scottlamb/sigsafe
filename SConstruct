# vim: ft=python
# $Id$

import os

# Determine our platform
arch = os.uname()[4]
if arch == 'Power Macintosh': arch = 'ppc'
Export('arch')
os_name = os.uname()[0].lower().replace('-','')
Export('os_name')
type = 'debug'
buildDir = 'build-%s-%s-%s' % (arch, os_name, type)

env = Environment(
    CPPPATH = [
        '#/src'
    ],
    CPPDEFINES = [
        '_XOPEN_SOURCE=600',
        '_REENTRANT',
        '_THREAD_SAFE',
    ],
    LIBPATH = [
        '#/' + buildDir
    ]
)

if env['CC'] == 'gcc':
    env.Append(CCFLAGS = ['-Wall'])
    if type == 'debug':
        env.Append(CCFLAGS=['-g'], LINKFLAGS=['-g'])
    else:
        env.Append(CPPDEFINES=['NDEBUG'],CCFLAGS=['-O2'],LINKFLAGS=['-O2'])

if os_name == 'darwin':
    env['AS'] = 'gcc'
    env['ASFLAGS'] = ['-c']

Export('env')
BuildDir(buildDir, 'src', 0)
SConscript(buildDir + '/SConscript')
SConscript('tests/SConscript')
