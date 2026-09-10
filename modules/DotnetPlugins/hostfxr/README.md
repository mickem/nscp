# .NET native hosting headers

`hostfxr.h` and `coreclr_delegates.h` are verbatim copies of the public native
hosting headers shipped with the .NET runtime (dotnet/runtime,
`src/native/corehost/`, taken from the `Microsoft.NETCore.App.Host` 8.0.30
pack). They are MIT licensed by the .NET Foundation and Contributors; see
`LICENSES/MIT.txt` and `THIRD-PARTY-NOTICES.md`.

They are vendored so the module builds without a .NET SDK installed and so the
calling conventions of every hostfxr entry point and runtime delegate
(`HOSTFXR_CALLTYPE` is `__cdecl`, `CORECLR_DELEGATE_CALLTYPE` is `__stdcall` on
Windows) come from the authoritative source rather than local typedefs: getting
one of them wrong only shows up as a crash on 32-bit Windows.

To update: copy the two files from a newer `Microsoft.NETCore.App.Host.<rid>`
pack (`runtimes/<rid>/native/`). Newer runtimes only append to these headers.
