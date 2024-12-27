from building import *

cwd = GetCurrentDir()

inc = [
cwd,
cwd+'/decompresser',
cwd+'/decompresser/tinyuz',
cwd+'/hpatch/HPatch',
cwd+'/hpatch/HPatchLite',
]

src  = Glob('hpatch_impl.c')
src += Glob('decompresser/tinyuz/*.c')
src += Glob('hpatch/HPatchLite/*.c')

group = DefineGroup('hpatchlite', src, depend = ['PKG_USING_HPATCHLITE_WRAPPER'], CPPPATH = inc)

Return('group')
