# WasmEdge x Anna

## Build

Assuming WasmEdge is installed to `../_installed`, you can build this project with:

```sh
cmake -B build -DCMAKE_PREFIX_PATH=../_install
cmake --build build --parallel $(nproc)
```

If WasmEdge is installed to system library path, the `-DCMAKE_PREFIX_PATH=../_install` can be omitted.

## Usage

```sh
./build/wasmedge_anna ../anna/conf/anna-local.yml ../wasmedge-anna-rs/target/wasm32-wasi/debug/demo-app.wasm
```
