#!/bin/sh

echo 'Building deej (development)...'

FOLDER="bin"
# shove git commit, version tag into env
GIT_COMMIT=$(git rev-list -1 --abbrev-commit HEAD)
VERSION_TAG=$(git describe --tags --always)
BUILD_TYPE=dev
echo 'Embedding build-time parameters:'
echo "- gitCommit $GIT_COMMIT"
echo "- versionTag $VERSION_TAG"
echo "- buildType $BUILD_TYPE"

go build -o $FOLDER/deej-dev -ldflags "-X main.gitCommit=$GIT_COMMIT -X main.versionTag=$VERSION_TAG -X main.buildType=$BUILD_TYPE" ./cmd
if [ $? -eq 0 ]; then
    cp scripts/misc/default-config_linux.yaml bin/config.yaml
    echo 'Done.'
else
    echo 'Error: "go build" exited with a non-zero code. Are you running this script from the deej directory?'
    exit 1
fi
