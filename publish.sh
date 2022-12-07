# Build
# Publish on github
echo "Publishing on Github..."
token=$1
# Get the last tag name
tag=$(git describe --tags)
# Get the full message associated with this tag
message="$(git for-each-ref refs/tags/$tag --format='%(contents)')"
# Get the title and the description as separated variables
name=$(echo "$message" | head -n1)
description=$(echo "$message" | tail -n +3)
description=$(echo "$description" | sed -z 's/\n/\\n/g') # Escape line breaks to prevent json parsing problems
# Create a release
release=$(curl -XPOST -H "Authorization:token $token" --data "{\"tag_name\": \"$tag\", \"target_commitish\": \"$2\", \"name\": \"$name\", \"body\": \"$description\", \"draft\": false, \"prerelease\": false}" https://api.github.com/repos/$3/releases)
# Extract the id of the release from the creation response
id=$(echo "$release" | sed -n -e 's/"id":\ \([0-9]\+\),/\1/p' | head -n 1 | sed 's/[[:blank:]]//g')
# Upload the artifacts
COLLECTD=$(basename `find . -type f -regextype posix-egrep -regex "\./collectd-[0-9].+\.rpm" -print`)
FILEDATA=$(basename `find . -type f -regextype posix-egrep -regex "\./collectd-filedata.+\.rpm" -print`)
SSH=$(basename `find . -type f -regextype posix-egrep -regex "\./collectd-ssh.+\.rpm" -print`)
curl -XPOST -H "Authorization:token $token" -H "Content-Type:application/octet-stream" --data-binary @$COLLECTD https://uploads.github.com/repos/$3/releases/$id/assets?name=$COLLECTD
curl -XPOST -H "Authorization:token $token" -H "Content-Type:application/octet-stream" --data-binary @$FILEDATA https://uploads.github.com/repos/$3/releases/$id/assets?name=$FILEDATA
curl -XPOST -H "Authorization:token $token" -H "Content-Type:application/octet-stream" --data-binary @$SSH https://uploads.github.com/repos/$3/releases/$id/assets?name=$SSH
