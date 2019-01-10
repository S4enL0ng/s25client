#!/bin/bash
###############################################################################
#
# This file will only be read by Jenkinsfile.
#
# Deploy all artifacts
#
# Parameter marker (replaced by Jenkins):
# - %deploy_to% : i.e "nightly" "stable"
#
# Directory structure:
# $pwd/result : directory for result files
#

set +x -euo pipefail

if [ -z "$JENKINS_SERVER_COOKIE" ] ; then
    echo "error: this script has to be run by Jenkins only." >&2
    exit 1
fi

###############################################################################

deploy_to="%deploy_to%"

result_dir=$(pwd)/result
archive_dir=/srv/backup/www/s25client/$deploy_to/$(date +%Y)
updater_dir=/www/siedler25.org/nightly/s25client/$deploy_to/

pushd $result_dir

mkdir -p $archive_dir
cp -av *.tar.bz2 *.zip $archive_dir/

artifacts=$(find . -maxdepth 1 -name '*.zip' -o -name '*.tar.bz2')
if [ -z "$artifacts" ] ; then
    echo "error: no artifacts were found." >&2
    exit 1
fi

mkdir -p $updater_dir
for artifact in $artifacts ; do
    echo "Processing file $artifact"

    VERSION=$(echo $(basename $artifact) | cut -f2- -d '_' | cut -f 1-2 -d '.')
    PLATFORM=$(echo $VERSION | cut -f 3 -d '-')

    arch_dir=$PLATFORM

    echo "- Platform: $arch_dir"
    echo ""

    set -x
    unpacked_dir=$arch_dir/unpacked/s25rttr_${VERSION}
    unpacked_remote_dir=$updater_dir/$unpacked_dir

    _changed=1
    if [ -d $unpacked_remote_dir ] && [ -d $unpacked_dir ] ; then
        diff -qrN $unpacked_remote_dir $unpacked_dir && _changed=0 || _changed=1
    fi

    set +x

    if [ $_changed -eq 0 ] ; then
        echo "- Skipping rotation. Nothing has been changed."
    else
        echo "- Uploading files."

        rsync -e 'ssh -i $SSH_KEYFILE' -av $result_dir/*.tar.bz2 $result_dir/*.zip $result_dir/*.txt tyra4.ra-doersch.de:/www/siedler25.org/www/uploads/$deploy_to/
        ssh -i $SSH_KEYFILE tyra4.ra-doersch.de "php -q /www/siedler25.org/www/docs/cron/${deploy_to}sql.php"
        ssh -i $SSH_KEYFILE tyra4.ra-doersch.de "php -q /www/siedler25.org/www/docs/cron/changelogsql.php"

        echo "- Rotating tree."

        rm -rf $updater_dir/$arch_dir.5
        [ -d $updater_dir/$arch_dir.4 ] && mv -v $updater_dir/$arch_dir.4 $updater_dir/$arch_dir.5 || true
        [ -d $updater_dir/$arch_dir.3 ] && mv -v $updater_dir/$arch_dir.3 $updater_dir/$arch_dir.4 || true
        [ -d $updater_dir/$arch_dir.2 ] && mv -v $updater_dir/$arch_dir.2 $updater_dir/$arch_dir.3 || true
        [ -d $updater_dir/$arch_dir.1 ] && mv -v $updater_dir/$arch_dir.1 $updater_dir/$arch_dir.2 || true
        [ -d $updater_dir/$arch_dir ]   && mv -v $updater_dir/$arch_dir   $updater_dir/$arch_dir.1 || true
        cp -a $arch_dir $updater_dir/$arch_dir.tmp
        mv -v $updater_dir/$arch_dir.tmp $updater_dir/$arch_dir
    fi
done