#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <anna/client_wrapper.hpp>
#include <wasmedge/wasmedge.h>

// the global anna kvs client
static std::unique_ptr<anna::ClientWrapper> kvs_client;

/**
 * Host function to **put** a key-value pair into the KV store.
 *
 * Parameters:
 *   key_size: usize as i32,
 *   key_ptr: *const u8 as i32,
 *   val_size: usize as i32,
 *   val_ptr: *const u8 as i32
 *
 * Returns:
 *   ok: bool as i32
 */
static WasmEdge_Result __hfunc_put(void *data,
                                   WasmEdge_MemoryInstanceContext *mem_inst,
                                   const WasmEdge_Value *params,
                                   WasmEdge_Value *returns) {
  auto key_size = static_cast<uint32_t>(WasmEdge_ValueGetI32(params[0]));
  auto key_ptr = static_cast<uint32_t>(WasmEdge_ValueGetI32(params[1]));
  auto val_size = static_cast<uint32_t>(WasmEdge_ValueGetI32(params[2]));
  auto val_ptr = static_cast<uint32_t>(WasmEdge_ValueGetI32(params[3]));

  // copy the key from module memory
  std::string key;
  key.resize(key_size);
  if (auto res = WasmEdge_MemoryInstanceGetData(
          mem_inst, reinterpret_cast<uint8_t *>(key.data()), key_ptr, key_size);
      !WasmEdge_ResultOK(res)) {
    return res;
  }

  // copy the value from module memory
  std::string val;
  val.resize(val_size);
  if (auto res = WasmEdge_MemoryInstanceGetData(
          mem_inst, reinterpret_cast<uint8_t *>(val.data()), val_ptr, val_size);
      !WasmEdge_ResultOK(res)) {
    return res;
  }

  auto ret = kvs_client->put(key, val);
  returns[0] = WasmEdge_ValueGenI32(ret);

  return WasmEdge_Result_Success;
}

/**
 * Host function to **get** value from the KV store by key.
 *
 * Parameters:
 *   key_size: usize as i32,
 *   key_ptr: *const u8 as i32,
 *   val_buf_size: usize as i32,
 *   [out] val_buf_ptr: *mut u8 as i32
 *
 * Returns:
 *   val_size: usize as i32
 *
 * Notes:
 *   - if val_size == 0, the key does not exist (or failed to get from server).
 *   - if val_size <= val_buf_size, the value is copied to the buffer.
 *   - if val_size > val_buf_size, no copy is performed.
 */
static WasmEdge_Result __hfunc_get(void *data,
                                   WasmEdge_MemoryInstanceContext *mem_inst,
                                   const WasmEdge_Value *params,
                                   WasmEdge_Value *returns) {
  auto key_size = static_cast<uint32_t>(WasmEdge_ValueGetI32(params[0]));
  auto key_ptr = static_cast<uint32_t>(WasmEdge_ValueGetI32(params[1]));
  auto val_buf_size = static_cast<uint32_t>(WasmEdge_ValueGetI32(params[2]));
  auto val_buf_ptr = static_cast<uint32_t>(WasmEdge_ValueGetI32(params[3]));

  // copy the key from module memory
  std::string key;
  key.resize(key_size);
  if (auto res = WasmEdge_MemoryInstanceGetData(
          mem_inst, reinterpret_cast<uint8_t *>(key.data()), key_ptr, key_size);
      !WasmEdge_ResultOK(res)) {
    return res;
  }

  auto val_opt = kvs_client->get(key);
  if (!val_opt) {
    returns[0] = WasmEdge_ValueGenI32(0);
  } else {
    auto val = std::move(val_opt).value();
    returns[0] = WasmEdge_ValueGenI32(static_cast<uint32_t>(val.size()));

    if (val.size() <= val_buf_size) {
      // the buffer provided by the module has enough space, so copy into it
      if (auto res = WasmEdge_MemoryInstanceSetData(
              mem_inst, reinterpret_cast<const uint8_t *>(val.data()),
              val_buf_ptr, val.size());
          !WasmEdge_ResultOK(res)) {
        return res;
      }
    }
  }

  return WasmEdge_Result_Success;
}

static auto create_anna_module() {
  auto mod_name = WasmEdge_StringCreateByCString("wasmedge_anna");
  auto mod = WasmEdge_ImportObjectCreate(mod_name);
  WasmEdge_StringDelete(mod_name);

  auto table_limit = WasmEdge_Limit{.HasMax = true, .Min = 10, .Max = 20};
  auto htable_type =
      WasmEdge_TableTypeCreate(WasmEdge_RefType_FuncRef, table_limit);
  auto htable = WasmEdge_TableInstanceCreate(htable_type);
  WasmEdge_TableTypeDelete(htable_type);
  auto htable_name = WasmEdge_StringCreateByCString("table");
  WasmEdge_ImportObjectAddTable(mod, htable_name, htable);
  WasmEdge_StringDelete(htable_name);

  auto mem_limit = WasmEdge_Limit{.HasMax = true, .Min = 1, .Max = 2};
  auto hmem_type = WasmEdge_MemoryTypeCreate(mem_limit);
  auto hmem = WasmEdge_MemoryInstanceCreate(hmem_type);
  WasmEdge_MemoryTypeDelete(hmem_type);
  auto hmem_name = WasmEdge_StringCreateByCString("memory");
  WasmEdge_ImportObjectAddMemory(mod, hmem_name, hmem);
  WasmEdge_StringDelete(hmem_name);

  auto hglobal_type =
      WasmEdge_GlobalTypeCreate(WasmEdge_ValType_I32, WasmEdge_Mutability_Var);
  auto hglobal =
      WasmEdge_GlobalInstanceCreate(hglobal_type, WasmEdge_ValueGenI32(666));
  WasmEdge_GlobalTypeDelete(hglobal_type);
  auto hglobal_name = WasmEdge_StringCreateByCString("global");
  WasmEdge_ImportObjectAddGlobal(mod, hglobal_name, hglobal);
  WasmEdge_StringDelete(hglobal_name);

  // register host functions
  {
    WasmEdge_ValType params[4] = {WasmEdge_ValType_I32, WasmEdge_ValType_I32,
                                  WasmEdge_ValType_I32, WasmEdge_ValType_I32};
    WasmEdge_ValType returns[1] = {WasmEdge_ValType_I32};
    auto hfunc_type = WasmEdge_FunctionTypeCreate(
        params, sizeof(params) / sizeof(params[0]), returns,
        sizeof(returns) / sizeof(returns[0]));
    auto hfunc =
        WasmEdge_FunctionInstanceCreate(hfunc_type, __hfunc_put, nullptr, 0);
    WasmEdge_FunctionTypeDelete(hfunc_type);
    auto hfunc_name = WasmEdge_StringCreateByCString("put");
    WasmEdge_ImportObjectAddFunction(mod, hfunc_name, hfunc);
    WasmEdge_StringDelete(hfunc_name);
  }
  {
    WasmEdge_ValType params[4] = {WasmEdge_ValType_I32, WasmEdge_ValType_I32,
                                  WasmEdge_ValType_I32, WasmEdge_ValType_I32};
    WasmEdge_ValType returns[1] = {WasmEdge_ValType_I32};
    auto hfunc_type = WasmEdge_FunctionTypeCreate(
        params, sizeof(params) / sizeof(params[0]), returns,
        sizeof(returns) / sizeof(returns[0]));
    auto hfunc =
        WasmEdge_FunctionInstanceCreate(hfunc_type, __hfunc_get, nullptr, 0);
    WasmEdge_FunctionTypeDelete(hfunc_type);
    auto hfunc_name = WasmEdge_StringCreateByCString("get");
    WasmEdge_ImportObjectAddFunction(mod, hfunc_name, hfunc);
    WasmEdge_StringDelete(hfunc_name);
  }

  return mod;
}

int main(int argc, const char *argv[]) {
  if (argc < 3) {
    std::cout << "Usage: " << argv[0]
              << " <anna config> <wasm file> [wasm arguments...]\n";
    return 1;
  }

  const char *anna_config = argv[1];
  const char *wasm_file = argv[2];
  const char **wasm_args = argv + 2;
  int wasm_argc = argc - 2;

  kvs_client = std::make_unique<anna::ClientWrapper>(anna_config);

  auto conf = WasmEdge_ConfigureCreate();
  WasmEdge_ConfigureAddHostRegistration(conf, WasmEdge_HostRegistration_Wasi);
  WasmEdge_ConfigureAddHostRegistration(
      conf, WasmEdge_HostRegistration_WasmEdge_Process);
  auto vm = WasmEdge_VMCreate(conf, NULL);

  auto wasi_mod =
      WasmEdge_VMGetImportModuleContext(vm, WasmEdge_HostRegistration_Wasi);
  const char *preopens[1] = {".:."};
  WasmEdge_ImportObjectInitWASI(wasi_mod, wasm_args, wasm_argc, nullptr, 0,
                                preopens,
                                sizeof(preopens) / sizeof(preopens[0]));

  auto env_mod = create_anna_module();
  WasmEdge_VMRegisterModuleFromImport(vm, env_mod);

  // run `_start` function of the loaded wasm module
  auto func_name = WasmEdge_StringCreateByCString("_start");
  auto res = WasmEdge_VMRunWasmFromFile(vm, wasm_file, func_name, nullptr, 0,
                                        nullptr, 0);

  if (!WasmEdge_ResultOK(res)) {
    std::cout << "Error: " << WasmEdge_ResultGetMessage(res) << "\n";
  }

  WasmEdge_VMDelete(vm);
  WasmEdge_ConfigureDelete(conf);
  WasmEdge_StringDelete(func_name);

  // manually release the client object, to avoid zeromq's bug
  kvs_client.reset();

  return 0;
}
