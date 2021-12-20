# a telegram bot script

base on [TDLib](https://github.com/lccxz/td)

get & build & test & run
```bash
git clone --recurse-submodules https://github.com/lccxz/tdscript.git

cd tdscript

cmake -S . -B build
# cmake -GNinja -DCMAKE_BUILD_TYPE=Release \
#   -DWITH_TESTS=OFF -DWITH_STATIC=ON -DWITH_IPV6=ON -S . -B build-release

cmake --build build

ctest --test-dir build

build/tdscript
```
