#!/usr/bin/env python
# Update driver files with contents of "drivers" directory

import os
import sys
import subprocess
import shutil

SIG_LEN = 284

signed_vtar_list = [
    's.v00',
    'sb.v00'
]

def handle_signed_vtar(basename):
    os.system('zcat {0}.gz > {0}.sign.xz'.format(basename))
    unsigned_len = os.path.getsize('{}.sign.xz'.format(basename)) - SIG_LEN
    print('unsigned length of {}.sign.xz is {}'.format(basename, unsigned_len))
    os.system('dd bs={0} count=1 if={1}.sign.xz of={1}.vtar.xz'.format(
        unsigned_len, basename))
    os.system('xz -d {}.vtar.xz'.format(basename))

PWD = os.getcwd()
TMP = PWD + '/tmp'
DRIVERS = PWD + '/drivers'

os.mkdir(TMP)
if not os.access(DRIVERS, os.F_OK):
    print("Could not find '{}' directory".format(DRIVERS))
    sys.exit(1)

vtar_list = subprocess.check_output(
    [
        '/bin/find', '/vmfs', '-name', 'temp',
        '-prune', '-o', '-name', '*.v0*',
        '-print'
    ]
).split()

for driver_path in vtar_list:
    driver_path = driver_path.decode()
    print("+++ Examining {}".format(driver_path))
    REPLACE_IT = 0
    basename = os.path.basename(driver_path)

    if os.access('{}/{}'.format(TMP, basename), os.F_OK):
    	os.remove('{}/{}'.format(TMP, basename))
    os.system('/bin/zcat {} > {}/{}.gz'.format(driver_path, TMP, basename))

    if basename in signed_vtar_list:
        OLDPWD = os.getcwd()
        os.chdir(TMP)
        handle_signed_vtar(basename)
        os.chdir(OLDPWD)
        os.system('/bin/vmtar -x {0}/{1}.vtar -o {0}/{1}.tar'.format(TMP, basename))
    else:
        os.system('/bin/vmtar -x {0}/{1}.gz -o {0}/{1}.tar'.format(TMP, basename))

    shutil.rmtree('{}/{}.tmp'.format(TMP, basename), ignore_errors=True)
    os.makedirs('{}/{}.tmp'.format(TMP, basename))

    os.system('tar -C {0}/{1}.tmp -xf {0}/{1}.tar'.format(TMP, basename))

    # For each driver found in the ESXi tarball, see if it
    # is in the OSS tarball, and replace it if it is.
    if os.access(
            '{}/{}.tmp/usr/lib/vmware/vmkmod'.format(TMP, basename),
            os.F_OK):
        driver_list = subprocess.check_output(
            ['ls', '{}/{}.tmp/usr/lib/vmware/vmkmod'.format(TMP, basename)]
        ).split()
        for driver in driver_list:
            driver = driver.decode()
            repl = DRIVERS + '/' + driver
            if os.access(repl, os.F_OK):
                dst = '{}/{}.tmp/usr/lib/vmware/vmkmod/{}'.format(
                    TMP, basename, driver
                )
                print('Updating {} with {}'.format(dst, repl))
                shutil.copy(repl, dst)
                REPLACE_IT = 1

    if REPLACE_IT == 1:
        OLDPWD = os.getcwd()
        os.chdir('{}/{}.tmp'.format(TMP, basename))
        if os.access('{}/{}.new.tar'.format(TMP, basename), os.F_OK):
            os.remove('{}/{}.new.tar'.format(TMP, basename))
        os.system('tar -cf {}/{}.new.tar *'.format(TMP, basename))
        os.chdir(OLDPWD)


        if os.access('{}/{}.new'.format(TMP, basename), os.F_OK):
            os.remove('{}/{}.new'.format(TMP, basename))
        os.system(
            'vmtar -c {0}/{1}.new.tar -o {0}/{1}.new'.format(TMP, basename)
        )

        if os.access('{}/{}.new.gz'.format(TMP, basename), os.F_OK):
            os.remove('{}/{}.new.gz'.format(TMP, basename))
        os.system('gzip {}/{}.new'.format(TMP, basename))

        print('+++ Replacing {} with {}/{}.new.gz'.format(
            driver_path, TMP, basename
        ))
        shutil.copy('{}/{}.new.gz'.format(TMP, basename), driver_path)
    else:
        print('+++ No updates needed for {}'.format(driver_path))
