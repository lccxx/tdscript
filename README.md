# a telegram bot script

base on [TDLib](https://github.com/tdlib/td)

get & build & test & run
```bash
git clone --recurse-submodules https://github.com/lccxz/tdscript.git

cd tdscript

cmake -S . -B build
# cmake -DWITH_STATIC=ON -DWITH_TESTS=OFF -DCMAKE_BUILD_TYPE=Release -S . -B build

cmake --build build

build/tdscript_test_client_test

# cmake -S . -B build && cmake --build build && build/tdscript_test_client_test

build/tdscript
```
