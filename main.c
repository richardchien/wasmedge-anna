#include <stdio.h>

#include <wasmedge/wasmedge.h>

WasmEdge_Result _host_func_add(void *Data,
                               WasmEdge_MemoryInstanceContext *MemCxt,
                               const WasmEdge_Value *Params,
                               WasmEdge_Value *Returns) {
  /*
   * Params: {i32, i32}
   * Returns: {i32}
   * Developers should take care about the function type.
   */
  int32_t Val1 = WasmEdge_ValueGetI32(Params[0]);
  int32_t Val2 = WasmEdge_ValueGetI32(Params[1]);
  Returns[0] = WasmEdge_ValueGenI32(Val1 + Val2);
  return WasmEdge_Result_Success;
}

WasmEdge_ImportObjectContext *create_env_import_obj() {
  WasmEdge_String ExportName = WasmEdge_StringCreateByCString("env");
  WasmEdge_ImportObjectContext *ImpObj =
      WasmEdge_ImportObjectCreate(ExportName);
  WasmEdge_StringDelete(ExportName);

  enum WasmEdge_ValType ParamList[2] = {WasmEdge_ValType_I32,
                                        WasmEdge_ValType_I32};
  enum WasmEdge_ValType ReturnList[1] = {WasmEdge_ValType_I32};
  WasmEdge_FunctionTypeContext *HostFType =
      WasmEdge_FunctionTypeCreate(ParamList, 2, ReturnList, 1);
  WasmEdge_FunctionInstanceContext *HostFunc =
      WasmEdge_FunctionInstanceCreate(HostFType, _host_func_add, NULL, 0);
  WasmEdge_FunctionTypeDelete(HostFType);

  WasmEdge_String FuncName = WasmEdge_StringCreateByCString("add");
  WasmEdge_ImportObjectAddFunction(ImpObj, FuncName, HostFunc);
  WasmEdge_StringDelete(FuncName);

  WasmEdge_Limit TableLimit = {.HasMax = true, .Min = 10, .Max = 20};
  WasmEdge_TableTypeContext *HostTType =
      WasmEdge_TableTypeCreate(WasmEdge_RefType_FuncRef, TableLimit);
  WasmEdge_TableInstanceContext *HostTable =
      WasmEdge_TableInstanceCreate(HostTType);
  WasmEdge_TableTypeDelete(HostTType);
  WasmEdge_String TableName = WasmEdge_StringCreateByCString("table");
  WasmEdge_ImportObjectAddTable(ImpObj, TableName, HostTable);
  WasmEdge_StringDelete(TableName);

  WasmEdge_Limit MemoryLimit = {.HasMax = true, .Min = 1, .Max = 2};
  WasmEdge_MemoryTypeContext *HostMType =
      WasmEdge_MemoryTypeCreate(MemoryLimit);
  WasmEdge_MemoryInstanceContext *HostMemory =
      WasmEdge_MemoryInstanceCreate(HostMType);
  WasmEdge_MemoryTypeDelete(HostMType);
  WasmEdge_String MemoryName = WasmEdge_StringCreateByCString("memory");
  WasmEdge_ImportObjectAddMemory(ImpObj, MemoryName, HostMemory);
  WasmEdge_StringDelete(MemoryName);

  WasmEdge_GlobalTypeContext *HostGType =
      WasmEdge_GlobalTypeCreate(WasmEdge_ValType_I32, WasmEdge_Mutability_Var);
  WasmEdge_GlobalInstanceContext *HostGlobal =
      WasmEdge_GlobalInstanceCreate(HostGType, WasmEdge_ValueGenI32(666));
  WasmEdge_GlobalTypeDelete(HostGType);
  WasmEdge_String GlobalName = WasmEdge_StringCreateByCString("global");
  WasmEdge_ImportObjectAddGlobal(ImpObj, GlobalName, HostGlobal);
  WasmEdge_StringDelete(GlobalName);

  return ImpObj;
}

int main(int argc, const char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s <wasm file>\n", argv[0]);
    return 1;
  }

  const char *wasm_file = argv[1];

  WasmEdge_ConfigureContext *conf_ctx = WasmEdge_ConfigureCreate();
  WasmEdge_ConfigureAddHostRegistration(conf_ctx,
                                        WasmEdge_HostRegistration_Wasi);
  WasmEdge_VMContext *vm_ctx = WasmEdge_VMCreate(conf_ctx, NULL);

  WasmEdge_ImportObjectContext *env_import = create_env_import_obj();
  WasmEdge_VMRegisterModuleFromImport(vm_ctx, env_import);

  WasmEdge_Value params[1] = {WasmEdge_ValueGenI32(32)};
  WasmEdge_Value returns[1];
  WasmEdge_String func_name = WasmEdge_StringCreateByCString("add2");

  WasmEdge_Result res = WasmEdge_VMRunWasmFromFile(vm_ctx, wasm_file, func_name,
                                                   params, 1, returns, 1);

  if (WasmEdge_ResultOK(res)) {
    printf("Get result: %d\n", WasmEdge_ValueGetI32(returns[0]));
  } else {
    printf("Error message: %s\n", WasmEdge_ResultGetMessage(res));
  }

  /* Resources deallocations. */
  WasmEdge_VMDelete(vm_ctx);
  WasmEdge_ConfigureDelete(conf_ctx);
  WasmEdge_StringDelete(func_name);
  return 0;
}
