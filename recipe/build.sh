mkdir -p sparta/release
pushd sparta/release
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX:PATH="$PREFIX" ..
cmake --build . --target

popd
