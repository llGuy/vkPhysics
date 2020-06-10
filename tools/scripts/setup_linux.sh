mkdir ../../build
(cd ../../build && cmake -DCMAKE_BUILD_TYPE=DEBUG -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ..)
sh build_linux.sh
