#!/usr/bin/env bash

set -e

MODULES=('men_ll_f404' 'men_ll_z82' 'men_ll_z17_z127');

function load_module()
{
    MODULE=$1;
	MOD=`lsmod | grep -o $MODULE | head -1`
    if [  "x$MOD" == "x" ]; then
		modprobe $MODULE;
    fi
}

for MOD in "${MODULES[@]}";
do 
	load_module $MOD;
done

if [ ! -c /dev/mdis ]; then
    mknod /dev/mdis c 248 0
fi

display_all
