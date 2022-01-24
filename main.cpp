#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <anna/client_wrapper.hpp>
#include <wasmedge/wasmedge.h>

static std::unique_ptr<anna::ClientWrapper> kvs_client;

using HFunc = WasmEdge_Result (*)(void *data,
                                  WasmEdge_MemoryInstanceContext *mem_inst,
                                  const WasmEdge_Value *params,
                                  WasmEdge_Value *returns);

static WasmEdge_Result __hfunc_add(void *data,
                                   WasmEdge_MemoryInstanceContext *mem_inst,
                                   const WasmEdge_Value *params,
                                   WasmEdge_Value *returns) {
  /*
   * params: {i32, i32}
   * returns: {i32}
   */
  auto a = WasmEdge_ValueGetI32(params[0]);
  auto b = WasmEdge_ValueGetI32(params[1]);
  returns[0] = WasmEdge_ValueGenI32(a + b);
  return WasmEdge_Result_Success;
}

static WasmEdge_Result __hfunc_put(void *data,
                                   WasmEdge_MemoryInstanceContext *mem_inst,
                                   const WasmEdge_Value *params,
                                   WasmEdge_Value *returns) {
  /*
   * params: {
   *   key_size: usize as i32,
   *   key_ptr: *const u8 as i32,
   *   val_size: usize as i32,
   *   val_ptr: *const u8 as i32
   * }
   * returns: {ok: bool as i32}
   */
  auto key_size = static_cast<uint32_t>(WasmEdge_ValueGetI32(params[0]));
  auto key_ptr = static_cast<uint32_t>(WasmEdge_ValueGetI32(params[1]));
  auto val_size = static_cast<uint32_t>(WasmEdge_ValueGetI32(params[2]));
  auto val_ptr = static_cast<uint32_t>(WasmEdge_ValueGetI32(params[3]));

  std::string key_data;
  key_data.resize(key_size);
  if (auto res = WasmEdge_MemoryInstanceGetData(
          mem_inst, reinterpret_cast<uint8_t *>(key_data.data()), key_ptr,
          key_size);
      !WasmEdge_ResultOK(res)) {
    return res;
  }

  std::string val_data;
  val_data.resize(val_size);
  if (auto res = WasmEdge_MemoryInstanceGetData(
          mem_inst, reinterpret_cast<uint8_t *>(val_data.data()), val_ptr,
          val_size);
      !WasmEdge_ResultOK(res)) {
    return res;
  }

  std::cout << "key: " << key_data << "\n";
  auto ret = kvs_client->put(key_data, val_data);
  returns[0] = WasmEdge_ValueGenI32(ret);

  {
    auto val = kvs_client->get(key_data);
    std::cout << "value: " << val.value() << "\n";
  }

  return WasmEdge_Result_Success;
}

static WasmEdge_Result __hfunc_get(void *data,
                                   WasmEdge_MemoryInstanceContext *mem_inst,
                                   const WasmEdge_Value *params,
                                   WasmEdge_Value *returns) {
  /*
   * params: {
   *   key_size: usize as i32,
   *   key_ptr: *const u8 as i32,
   *   val_buf_size: usize as i32,
   *   [out] val_buf_ptr: *const u8 as i32
   * }
   * returns: {val_size: usize as i32}
   *
   * if val_size == 0, the key may not exist.
   * if val_size <= val_buf_size, the value is copied to val_buf.
   * if val_size > val_buf_size, no copy is performed.
   */
  auto key_size = static_cast<uint32_t>(WasmEdge_ValueGetI32(params[0]));
  auto key_ptr = static_cast<uint32_t>(WasmEdge_ValueGetI32(params[1]));
  auto val_buf_size = static_cast<uint32_t>(WasmEdge_ValueGetI32(params[2]));
  auto val_buf_ptr = static_cast<uint32_t>(WasmEdge_ValueGetI32(params[3]));

  std::string key_data;
  key_data.resize(key_size);
  if (auto res = WasmEdge_MemoryInstanceGetData(
          mem_inst, reinterpret_cast<uint8_t *>(key_data.data()), key_ptr,
          key_size);
      !WasmEdge_ResultOK(res)) {
    return res;
  }

  auto val_opt = kvs_client->get(key_data);
  if (!val_opt) {
    returns[0] = WasmEdge_ValueGenI32(0);
  } else {
    auto val = std::move(val_opt).value();
    std::cout << "get value: " << val << "\n";
    returns[0] = WasmEdge_ValueGenI32(static_cast<uint32_t>(val.size()));

    if (val.size() <= val_buf_size) {
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

static auto create_env_import_obj() {
  auto mod_name = WasmEdge_StringCreateByCString("env");
  auto import_obj = WasmEdge_ImportObjectCreate(mod_name);
  WasmEdge_StringDelete(mod_name);

  auto table_limit = WasmEdge_Limit{.HasMax = true, .Min = 10, .Max = 20};
  auto htable_type =
      WasmEdge_TableTypeCreate(WasmEdge_RefType_FuncRef, table_limit);
  auto htable = WasmEdge_TableInstanceCreate(htable_type);
  WasmEdge_TableTypeDelete(htable_type);
  auto htable_name = WasmEdge_StringCreateByCString("table");
  WasmEdge_ImportObjectAddTable(import_obj, htable_name, htable);
  WasmEdge_StringDelete(htable_name);

  auto mem_limit = WasmEdge_Limit{.HasMax = true, .Min = 1, .Max = 2};
  auto hmem_type = WasmEdge_MemoryTypeCreate(mem_limit);
  auto hmem = WasmEdge_MemoryInstanceCreate(hmem_type);
  WasmEdge_MemoryTypeDelete(hmem_type);
  auto hmem_name = WasmEdge_StringCreateByCString("memory");
  WasmEdge_ImportObjectAddMemory(import_obj, hmem_name, hmem);
  WasmEdge_StringDelete(hmem_name);

  auto hglobal_type =
      WasmEdge_GlobalTypeCreate(WasmEdge_ValType_I32, WasmEdge_Mutability_Var);
  auto hglobal =
      WasmEdge_GlobalInstanceCreate(hglobal_type, WasmEdge_ValueGenI32(666));
  WasmEdge_GlobalTypeDelete(hglobal_type);
  auto hglobal_name = WasmEdge_StringCreateByCString("global");
  WasmEdge_ImportObjectAddGlobal(import_obj, hglobal_name, hglobal);
  WasmEdge_StringDelete(hglobal_name);

  // register host functions
  {
    WasmEdge_ValType params[2] = {WasmEdge_ValType_I32, WasmEdge_ValType_I32};
    WasmEdge_ValType returns[1] = {WasmEdge_ValType_I32};
    auto hfunc_type = WasmEdge_FunctionTypeCreate(params, 2, returns, 1);
    auto hfunc =
        WasmEdge_FunctionInstanceCreate(hfunc_type, __hfunc_add, nullptr, 0);
    WasmEdge_FunctionTypeDelete(hfunc_type);
    auto hfunc_name = WasmEdge_StringCreateByCString("__wasmedge_anna_add");
    WasmEdge_ImportObjectAddFunction(import_obj, hfunc_name, hfunc);
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
        WasmEdge_FunctionInstanceCreate(hfunc_type, __hfunc_put, nullptr, 0);
    WasmEdge_FunctionTypeDelete(hfunc_type);
    auto hfunc_name = WasmEdge_StringCreateByCString("__wasmedge_anna_put");
    WasmEdge_ImportObjectAddFunction(import_obj, hfunc_name, hfunc);
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
    auto hfunc_name = WasmEdge_StringCreateByCString("__wasmedge_anna_get");
    WasmEdge_ImportObjectAddFunction(import_obj, hfunc_name, hfunc);
    WasmEdge_StringDelete(hfunc_name);
  }

  return import_obj;
}

int main(int argc, const char *argv[]) {
  if (argc != 3) {
    std::cout << "Usage: " << argv[0] << " <anna config> <wasm file>\n";
    return 1;
  }

  const char *anna_config = argv[1];
  const char *wasm_file = argv[2];

  kvs_client = std::make_unique<anna::ClientWrapper>(anna_config);

  auto conf = WasmEdge_ConfigureCreate();
  WasmEdge_ConfigureAddHostRegistration(conf, WasmEdge_HostRegistration_Wasi);
  WasmEdge_ConfigureAddHostRegistration(
      conf, WasmEdge_HostRegistration_WasmEdge_Process);
  auto vm = WasmEdge_VMCreate(conf, NULL);

  auto wasi_mod =
      WasmEdge_VMGetImportModuleContext(vm, WasmEdge_HostRegistration_Wasi);
  WasmEdge_ImportObjectInitWASI(wasi_mod, &wasm_file, 1, nullptr, 0, nullptr,
                                0);

  auto env_import = create_env_import_obj();
  WasmEdge_VMRegisterModuleFromImport(vm, env_import);

  auto func_name = WasmEdge_StringCreateByCString("_start");
  auto res = WasmEdge_VMRunWasmFromFile(vm, wasm_file, func_name, nullptr, 0,
                                        nullptr, 0);

  if (!WasmEdge_ResultOK(res)) {
    std::cout << "Error: " << WasmEdge_ResultGetMessage(res) << "\n";
  }

  /* Resources deallocations. */
  WasmEdge_VMDelete(vm);
  WasmEdge_ConfigureDelete(conf);
  WasmEdge_StringDelete(func_name);

  // anna::ClientWrapper &client = *kvs_client;
  // std::cout << "GET a : " << client.get("a").value_or("NULL") << "\n";
  // client.put("a", "foo");
  // std::cout << "GET a : " << client.get("a").value_or("NULL") << "\n";
  // client.put_set("set", {"1", "2", "3"});
  // auto set = client.get_set("set");
  // if (set) {
  //   for (auto &val : set.value()) {
  //     std::cout << val << ", ";
  //   }
  //   std::cout << "\n";
  // }
  // client.put_set("set", {"1", "2", "4"});
  // set = client.get_set("set");
  // if (set) {
  //   for (auto &val : set.value()) {
  //     std::cout << val << ", ";
  //   }
  //   std::cout << "\n";
  // }

  // manually release the client object, to avoid zeromq's bug
  kvs_client.reset();
  return 0;
}
