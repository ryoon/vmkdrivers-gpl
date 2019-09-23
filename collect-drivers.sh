#! /bin/bash

# Iterate through this directory and copy all of the driver files into a directory
mkdir drivers
for filename in *
do
	if [[ "$filename" == vmkdriver* ]]; then
		driverName=${filename//vmkdriver-/}
		driverName=${driverName//-CUR/}
		driverPath=$filename/release/vmkernel64/$driverName
		cp $driverPath drivers
	fi
done
