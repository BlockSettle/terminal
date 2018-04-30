# Building BlockSettle terminal

* Run the following command:
`python generate.py [debug]`
(last optional argument is used to enable debugging symbols)

* Go to `terminal.debug` or `terminal.release` dir (depending on the 'debug' argument on the previous step) and type your favourite make command (basically `make -j4`). Windows users should open the BlockSettle.sln file in one of these dirs.

* The binary can then be found in build_terminal dir

Currently supported platforms: Linux x64 (Ubuntu), MacOS X, Windows x64 with VS2015
