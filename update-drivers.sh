#!/bin/sh
# Update driver files with contents of "drivers" directory

set -e

TMP=$PWD/tmp
DRIVERS=$PWD/drivers

mkdir -p $TMP
if [ ! -d $DRIVERS ]
then
    echo "Could not find \"$DRIVERS\" directory"
    exit 1
fi

for driver_path in `find /vmfs -name temp -prune -o -name "*.v0*" -print | sort`
do
    echo "+++ Examining $driver_path"
    REPLACE_IT=0
    basename=`basename $driver_path`

    rm -f $TMP/$basename
    zcat $driver_path > $TMP/$basename

    vmtar -x $TMP/$basename -o $TMP/$basename.tar

    rm -rf $TMP/$basename.tmp
    mkdir -p $TMP/$basename.tmp

    tar -C $TMP/$basename.tmp -xf $TMP/$basename.tar

    # For each driver found in the ESXi tarball, see if it
    # is in the OSS tarball, and replace it if it is.
    if [ -d  $TMP/$basename.tmp/usr/lib/vmware/vmkmod ]
    then
        for driver in `ls $TMP/$basename.tmp/usr/lib/vmware/vmkmod/`
        do
            repl=$DRIVERS/$driver
            if [ -e $repl ]
            then
                dst=$TMP/$basename.tmp/usr/lib/vmware/vmkmod/$driver
                echo Updating $dst with $repl
                cp $repl $dst
                # we found something to replace
                REPLACE_IT=1
            fi
        done
    fi

    # If we updated a driver, make a new tarball and move it in
    # place.
    if [ $REPLACE_IT == 1 ]
    then
        cd $TMP/$basename.tmp
        rm -f $TMP/$basename.new.tar
        tar -cf $TMP/$basename.new.tar *
        cd $OLDPWD

        rm -f $TMP/$basename.new
        vmtar -c $TMP/$basename.new.tar -o $TMP/$basename.new

        rm -f $TMP/$basename.new.gz
        gzip $TMP/$basename.new

        echo +++ Replacing $driver_path with $TMP/$basename.new.gz
        #echo -n OLD:
        #ls -la $driver_path
        cp $TMP/$basename.new.gz $driver_path
        #echo -n SRC:
        #ls -la $TMP/$basename.new.gz
        #echo -n NEW:
        #ls -la $driver_path
    else
        echo +++ No updates needed for $driver_path
    fi
done
