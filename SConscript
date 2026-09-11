# RT-Thread SCons script for hpatchlite-wrapper package.

import os
import subprocess
from building import *

cwd = GetCurrentDir()
src = []
hpatchlite_root = os.path.join(cwd, 'HPatchLite')
CPPPATH = [cwd]

_SUBMODULE_UPDATE_COMMAND = ['git', 'submodule', 'update', '--init', '--recursive', '--', 'HPatchLite']
_SUBMODULE_UPDATE_HINT = 'git submodule update --init --recursive -- HPatchLite'


def _update_submodules():
    # Source archives may not contain Git metadata. In that case, keep the build usable
    # when the required submodule sources have already been packaged with the archive.
    if not os.path.exists(os.path.join(cwd, '.git')):
        return

    try:
        result = subprocess.call(_SUBMODULE_UPDATE_COMMAND, cwd=cwd)
    except OSError as exc:
        raise RuntimeError('Failed to execute "%s": %s' % (_SUBMODULE_UPDATE_HINT, exc))

    if result != 0:
        raise RuntimeError(
            '"%s" failed with exit code %d in %s.' % (_SUBMODULE_UPDATE_HINT, result, cwd)
        )


def _add_required(path, submodule_source=False):
    if os.path.isfile(os.path.join(cwd, path)):
        src.append(path)
        return

    if submodule_source:
        raise RuntimeError(
            'Required HPatchLite submodule source is missing: %s. Run "%s" in %s.'
            % (path, _SUBMODULE_UPDATE_HINT, cwd)
        )

    raise RuntimeError('Required hpatchlite-wrapper source is missing: %s' % path)


if GetDepend('PKG_USING_HPATCHLITE'):
    _add_required('hpatch_impl.c')
    _update_submodules()

    CPPPATH += [
        hpatchlite_root,
        os.path.join(hpatchlite_root, 'HDiffPatch', 'libHDiffPatch', 'HPatchLite'),
        os.path.join(hpatchlite_root, 'HDiffPatch', 'libHDiffPatch', 'HPatch'),
    ]

    _add_required(
        os.path.join('HPatchLite', 'HDiffPatch', 'libHDiffPatch', 'HPatchLite', 'hpatch_lite.c'),
        submodule_source=True,
    )

    if GetDepend('PKG_HPATCHLITE_DECOMPRESSER_TUZ'):
        CPPPATH.append(os.path.join(hpatchlite_root, 'tinyuz', 'decompress'))
        _add_required(
            os.path.join('HPatchLite', 'tinyuz', 'decompress', 'tuz_dec.c'),
            submodule_source=True,
        )


group = DefineGroup('hpatchlite-wrapper', src, depend=['PKG_USING_HPATCHLITE'], CPPPATH=CPPPATH)

Return('group')
