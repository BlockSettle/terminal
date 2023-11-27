# How to run Ubuntu build in docker
## Run this command in terminal repo folder
```
docker build . -t bsterminal:latest -f ubuntu.Dockerfile
```
## Get bs terminal app (bsterminal) from docker container image
```
docker create --name=terminal bsterminal:latest
docker cp terminal:/app/build_terminal/RelWithDebInfo/bin/blocksettle bsterminal
docker rm terminal
```
