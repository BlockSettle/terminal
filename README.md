# Building BlockSettle terminal

* Run the following command:
`python generate.py [debug] [test]`
(last 2 arguments are used to enable debugging symbols and enable the build of unit tests)

* Go to `terminal.debug` or `terminal.release` dir (depending on the 'debug' argument on the previous step) and type your favourite make command (basically `make -j4`)

* The binary can then be found in build_terminal dir

Currently supported platforms: Linux x64 (Ubuntu), MacOS X, Windows x64 with VS2015
