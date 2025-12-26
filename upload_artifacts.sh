#!/bin/sh

echo "Uploading release artifacts to GitHub..."
token=$1
tag_name=""

# Get the latest release
release=$(curl -sL -H "Authorization:token $token" https://api.github.com/repos/$2/releases/latest)

# Extract the id of the release from the creation response
id=$(echo "$release" | sed -n -e 's/"id":\ \([0-9]\+\),/\1/p' | head -n 1 | sed 's/[[:blank:]]//g')
tag_name=$(echo "$release" | sed -n -e 's/"tag_name":\ "\([[:graph:]]\+\)",/\1/p' | head -n 1 | sed 's/[[:blank:]]//g')

COLLECTD=""
FILEDATA=""
SSH=""
GPU_NVIDIA=""
LFS=""

# Upload the artifacts
if [ "$3" = "ubuntu" ]; then
	pkg_prefix="collectd-core_${tag_name#*-}"
	COLLECTD=$(find .. -type f -regextype posix-egrep -regex "\.\./${pkg_prefix}.+\.deb" -print)
	mv ${COLLECTD} .
	COLLECTD=$(basename ${COLLECTD})
else
	COLLECTD=$(basename `find . -type f -regextype posix-egrep -regex "\./collectd-[0-9].+\.rpm" -print`)
	FILEDATA=$(basename `find . -type f -regextype posix-egrep -regex "\./collectd-filedata-[0-9].+\.rpm" -print`)
	SSH=$(basename `find . -type f -regextype posix-egrep -regex "\./collectd-ssh-[0-9].+\.rpm" -print`)
	GPU_NVIDIA=$(basename `find . -type f -regextype posix-egrep -regex "\./collectd-gpu_nvidia-[[:digit:]].+\.rpm" -print`)
	LFS=$(basename `find . -type f -regextype posix-egrep -regex "\./collectd-lfs-[0-9].+\.rpm" -print`)
fi

if [ -n "$COLLECTD" ]; then
	curl -XPOST -H "Authorization:token $token" -H "Content-Type:application/octet-stream" --data-binary @$COLLECTD https://uploads.github.com/repos/$2/releases/$id/assets?name=$COLLECTD
fi

if [ -n "$FILEDATA" ]; then
	curl -XPOST -H "Authorization:token $token" -H "Content-Type:application/octet-stream" --data-binary @$FILEDATA https://uploads.github.com/repos/$2/releases/$id/assets?name=$FILEDATA
fi

if [ -n "$SSH" ]; then
	curl -XPOST -H "Authorization:token $token" -H "Content-Type:application/octet-stream" --data-binary @$SSH https://uploads.github.com/repos/$2/releases/$id/assets?name=$SSH
fi

if [ -n "$GPU_NVIDIA" ]; then
	curl -XPOST -H "Authorization:token $token" -H "Content-Type:application/octet-stream" --data-binary @$GPU_NVIDIA https://uploads.github.com/repos/$2/releases/$id/assets?name=$GPU_NVIDIA
fi

if [ -n "$LFS" ]; then
	curl -XPOST -H "Authorization:token $token" -H "Content-Type:application/octet-stream" --data-binary @$LFS https://uploads.github.com/repos/$2/releases/$id/assets?name=$LFS
fi
