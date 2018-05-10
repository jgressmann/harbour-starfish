#!/bin/sh


Usage()
{
    echo Usage: $(basename $0) \<target names\>
}

SetupTarget()
{
    local target=$1
    # add openrepos.net key
    sb2 -t $target -m sdk-install -R rpm --import https://sailfish.openrepos.net/openrepos.key
    # add private repo
    sb2 -t $target -m sdk-install -R zypper ar -f https://sailfish.openrepos.net/jgressmann/personal-main.repo
    # update database
    sb2 -t $target -m sdk-install -R zypper ref
}

#if [ $# -eq 0 ]; then
#    Usage
#    exit 0
#fi

targets=$(sb2-config -f)
for target in $targets; do
    echo setting up $target
    SetupTarget $target
done
