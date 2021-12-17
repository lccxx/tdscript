# a telegram bot script

base on [TDLib](https://github.com/lccxz/td)

get & build & test & run
```bash
git clone --recurse-submodules https://github.com/lccxz/tdscript.git

cd tdscript

cmake -S . -B build
# cmake -GNinja -DWITH_STATIC=ON -DWITH_TESTS=OFF -DCMAKE_BUILD_TYPE=Release -S . -B build-release

cmake --build build
# cmake --build build-release

build/tdscript_test_client_test

build/tdscript
```
