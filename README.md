# WasmEdge x Anna

## Build

Assuming WasmEdge is installed to `../_installed`, you can build this project with:

```sh
cmake -B build -DCMAKE_PREFIX_PATH=../_install
cmake --build build --parallel $(nproc)
```

If WasmEdge is installed to system library path, the `-DCMAKE_PREFIX_PATH=../_install` can be omitted.

## Usage

Write an application using [wasmedge-anna-rs](https://github.com/second-state/wasmedge-anna-rs), then build and run it with:

```sh
# in the app dir, say `/path/to/wasmedge-anna-rs/examples/hello`
cargo build --target wasm32-wasi
/path/to/wasmedge-anna/build/wasmedge_anna /path/to/anna/conf/anna-local.yml target/wasm32-wasi/debug/hello.wasm
```
