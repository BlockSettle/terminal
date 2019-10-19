#!/bin/bash
echo "____________________________________"
echo "............Build.................."
echo "____________________________________"

set -o errexit -o nounset

# Hold on to current directory
project_dir=$(pwd)
script_dir=${project_dir}/Travis/
third_dir=${project_dir}/../3rd/
build_dir=${project_dir}/build_terminal/Release/bin/

echo "Project dir: ${project_dir}"
echo "3rd Party dir: ${third_dir}"

TRAVIS_TAG=${TRAVIS_TAG:-$(date +'%Y.%m.%d_%H.%M.%S')-$(git log --format=%h -1)}
APP_FILE_NAME=BlockSettle_${TRAVIS_TAG}_Linux_x64

echo "Building for branch ${TRAVIS_BRANCH}"
echo "Building TAG ${TRAVIS_TAG}"

ZIP_FILE_NAME=${APP_FILE_NAME}.tar.gz

echo "Target file is ${ZIP_FILE_NAME}"
echo "Building on platform: $(lsb_release -a)"

# Build App
# Due 120 minutes limit build either 3rd party as first step or project as second step
# When 3rd party build completed it will be cached by travis

echo "Building App..."
cd ${project_dir}
if [ ! -d "${third_dir}/release/Qt5" ]
then
   python3 generate.py | cut -c1-100
   exit 0
else
   cd ${project_dir}/terminal.release
   make -j2
   make clean
fi


# Build and run tests here

echo "____________________________________"
echo "............Deploy.................."
echo "____________________________________"

# Package 
echo "Packaging ..."
ls -al ${build_dir}
tar -czvf  ${script_dir}/${ZIP_FILE_NAME} ${build_dir}
echo "Deploy is done"

# _________________________________________________________________
#echo "Deploy to Releases"
#chmod +x ${script_dir}/push_github.sh

# $GH_API_TOKEN comes from travis.yml env  

#GH_TAG=${TRAVIS_TAG}
#${script_dir}/push_github.sh github_api_token=$GH_API_TOKEN owner_repo=$TRAVIS_REPO_SLUG tag=$GH_TAG filename=${script_dir}/${ZIP_FILE_NAME}


exit 0
