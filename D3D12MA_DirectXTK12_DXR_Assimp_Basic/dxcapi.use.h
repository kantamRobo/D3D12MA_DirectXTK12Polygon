//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
//////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcapi.use.h                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides support for DXC API users.                                       //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef __DXCAPI_USE_H__
#define __DXCAPI_USE_H__

#include <dxcapi.h>
#include <stdexcept>
#include <wrl/client.h>

namespace dxc {

// Helper class to dynamically load the dxcompiler or a compatible libraries.
class DxcDllSupport {
protected:
  HMODULE m_dll;
  DxcCreateInstanceProc m_createFn;
  DxcCreateInstance2Proc m_createFn2;

  HRESULT InitializeInternal(LPCWSTR dllName, LPCSTR fnName) {
    if (m_dll != nullptr) return S_OK;
    m_dll = LoadLibraryW(dllName);

    if (m_dll == nullptr) return HRESULT_FROM_WIN32(GetLastError());
    m_createFn = (DxcCreateInstanceProc)GetProcAddress(m_dll, fnName);

    if (m_createFn == nullptr) {
      HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
      FreeLibrary(m_dll);
      m_dll = nullptr;
      return hr;
    }

    // Only basic functions used to avoid requiring additional headers.
    m_createFn2 = nullptr;
    char fnName2[128];
    size_t s = strlen(fnName);
    if (s < sizeof(fnName2) - 2) {
      memcpy(fnName2, fnName, s);
      fnName2[s] = '2';
      fnName2[s + 1] = '\0';
      m_createFn2 = (DxcCreateInstance2Proc)GetProcAddress(m_dll, fnName2);
    }

    return S_OK;
  }

public:
  DxcDllSupport() : m_dll(nullptr), m_createFn(nullptr), m_createFn2(nullptr) {
  }

  DxcDllSupport(DxcDllSupport&& other) noexcept {
    m_dll = other.m_dll; other.m_dll = nullptr;
    m_createFn = other.m_createFn; other.m_createFn = nullptr;
    m_createFn2 = other.m_createFn2; other.m_createFn2 = nullptr;
  }

  ~DxcDllSupport() {
    Cleanup();
  }

  HRESULT Initialize() {
    return InitializeInternal(L"dxcompiler.dll", "DxcCreateInstance");
  }

  HRESULT InitializeForDll(_In_z_ const wchar_t* dll, _In_z_ const char* entryPoint) {
    return InitializeInternal(dll, entryPoint);
  }

  template <typename TInterface>
  HRESULT CreateInstance(REFCLSID clsid, _Outptr_ TInterface** pResult) {
    return CreateInstance(clsid, __uuidof(TInterface), (IUnknown**)pResult);
  }

  template <typename TInterface>
  HRESULT CreateInstance(REFCLSID clsid, Microsoft::WRL::ComPtr<TInterface>& pResult) {
    return CreateInstance(clsid, __uuidof(TInterface), (IUnknown**)pResult.ReleaseAndGetAddressOf());
  }

  template <typename TInterface>
  HRESULT CreateInstance(REFCLSID clsid, Microsoft::WRL::Details::ComPtrRef<Microsoft::WRL::ComPtr<TInterface>> pResult) {
    return CreateInstance(clsid, __uuidof(TInterface), (IUnknown**)static_cast<TInterface**>(pResult));
  }

  HRESULT CreateInstance(REFCLSID clsid, REFIID riid, _Outptr_ IUnknown **pResult) {
    if (pResult == nullptr) return E_POINTER;
    if (m_dll == nullptr) return E_FAIL;
    HRESULT hr = m_createFn(clsid, riid, (LPVOID*)pResult);
    return hr;
  }

  template <typename TInterface>
  HRESULT CreateInstance2(IMalloc *pMalloc, REFCLSID clsid, _Outptr_ TInterface** pResult) {
    return CreateInstance2(pMalloc, clsid, __uuidof(TInterface), (IUnknown**)pResult);
  }

  template <typename TInterface>
  HRESULT CreateInstance2(IMalloc *pMalloc, REFCLSID clsid, Microsoft::WRL::ComPtr<TInterface>& pResult) {
    return CreateInstance2(pMalloc, clsid, __uuidof(TInterface), (IUnknown**)pResult.ReleaseAndGetAddressOf());
  }

  template <typename TInterface>
  HRESULT CreateInstance2(IMalloc *pMalloc, REFCLSID clsid, Microsoft::WRL::Details::ComPtrRef<Microsoft::WRL::ComPtr<TInterface>> pResult) {
    return CreateInstance2(pMalloc, clsid, __uuidof(TInterface), (IUnknown**)static_cast<TInterface**>(pResult));
  }

  HRESULT CreateInstance2(IMalloc *pMalloc, REFCLSID clsid, REFIID riid, _Outptr_ IUnknown **pResult) {
    if (pResult == nullptr) return E_POINTER;
    if (m_dll == nullptr) return E_FAIL;
    if (m_createFn2 == nullptr) return E_FAIL;
    HRESULT hr = m_createFn2(pMalloc, clsid, riid, (LPVOID*)pResult);
    return hr;
  }

  bool HasCreateWithMalloc() const {
    return m_createFn2 != nullptr;
  }

  bool IsEnabled() const {
    return m_dll != nullptr;
  }

  void Cleanup() {
    if (m_dll != nullptr) {
      m_createFn = nullptr;
      m_createFn2 = nullptr;
      FreeLibrary(m_dll);
      m_dll = nullptr;
    }
  }

  HMODULE Detach() {
    HMODULE module = m_dll;
    m_dll = nullptr;
    return module;
  }
};

inline DxcDefine GetDefine(_In_ LPCWSTR name, LPCWSTR value) {
  DxcDefine result;
  result.Name = name;
  result.Value = value;
  return result;
}

inline void EnsureEnabled(DxcDllSupport &dxcSupport) {
  if (!dxcSupport.IsEnabled()) {
    HRESULT hr = dxcSupport.Initialize();
    if (FAILED(hr)) {
      throw std::runtime_error("Failed to initialize DXC support.");
    }
  }
}

inline HRESULT ReadFileIntoBlob(DxcDllSupport &dxcSupport, _In_ LPCWSTR pFileName,
                                _Outptr_ IDxcBlobEncoding **ppBlobEncoding) {
  Microsoft::WRL::ComPtr<IDxcLibrary> library;
  HRESULT hr = dxcSupport.CreateInstance(CLSID_DxcLibrary, library.GetAddressOf());
  if (FAILED(hr)) return hr;
  return library->CreateBlobFromFile(pFileName, nullptr, ppBlobEncoding);
}

inline HRESULT ReadFileIntoBlob(DxcDllSupport &dxcSupport, _In_ LPCWSTR pFileName,
                                Microsoft::WRL::ComPtr<IDxcBlobEncoding>& pBlobEncoding) {
  return ReadFileIntoBlob(dxcSupport, pFileName, pBlobEncoding.ReleaseAndGetAddressOf());
}

template <typename T>
inline HRESULT ReadFileIntoBlob(DxcDllSupport &dxcSupport, _In_ LPCWSTR pFileName,
                                Microsoft::WRL::Details::ComPtrRef<T> pBlobEncoding) {
  return ReadFileIntoBlob(dxcSupport, pFileName, static_cast<IDxcBlobEncoding**>(pBlobEncoding));
}

} // namespace dxc

#endif // __DXCAPI_USE_H__
