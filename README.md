# CRYENGINE DXC Shader Compiler

This code integrates Microsoft’s DXC shader compiler into CRYENGINE, replacing the default FXC path. It enables compiling modern HLSL shaders — including those used for DirectX Raytracing — directly within the engine. The system can compile from source using the DXC COM API, fall back to an external dxc.exe if needed, validate the resulting DXIL bytecode, and generate placeholder shaders when source or compiled files aren’t available. While it was developed with ray tracing in mind, it can be adapted to compile any .hlsl shader for CRYENGINE.

## Requirements
------------

*   **CRYENGINE 5.7** source code.
    
*   File layout matches the engine’s structure — you can drag and drop the provided files directly into place.
    
    *   In your project, files should be added under:
        
        *   CryRenderD3D11 and CryRenderD3D12 in **Source Files** (.cpp)
            
        *   CryRenderD3D11 and CryRenderD3D12 in **Header Files** (.h)
            
*   In addition to the SDKs listed in the CRYENGINE source readme, you’ll need the **DirectX SDK**.
    
*   Ship dxcompiler.dll with your executable and link against dxcompiler.lib.
    
*   (Optional) Install the Windows SDK with dxc.exe for external fallback.
    

## What it does
------------

*   **Modern HLSL compilation:** Uses the DXC COM API to compile HLSL 6.x shaders (including DXR libraries with target lib\_6\_3).
    
*   **Engine‑aware file loading:** Resolves the engine’s shader directory and loads via CryPak or standard I/O.
    
*   **External fallback:** If in‑process compilation fails, automatically tries an external dxc.exe at common SDK install paths.
    
*   **DXIL validation:** Parses the DXIL container and ensures a valid DXIL part exists.
    
*   **Placeholder generation:** Creates minimal DXIL containers so pipelines can still be wired when sources or compiled shaders are missing.
    
*   **Pipeline‑ready metadata:** Stores bytecode and size for direct use in pipeline/state object creation.
    

## Usage
-----

**Place your shaders:**

*   Examples for ray tracing:
    

*   RayGen.hlsl — entry: RayGenMain
    
*   Miss.hlsl — entry: MissMain
    
*   ClosestHit.hlsl — entry: ClosestHitMain
    
*   Typical location: Engine/Shaders/HWScripts/CryFX/
    

**Compile at init:**_CCompiler compiler;if (!compiler.CompileRayTracingShaders()){    CryLogAlways("\[Ray Tracing\] Compilation failed; check logs for DXC errors.");}// Use compiler.m\_rayGenShaderBytecode, etc. for pipeline creation_**Create DXR state objects:**

*   Feed D3D12\_SHADER\_BYTECODE structures with .data() and .size() from the compiler’s bytecode vectors.
    
*   Build D3D12\_DXIL\_LIBRARY\_DESC and exports for your shader entry points.
    
*   Repeat for all shader stages.
    

**Alternate path:**

*   Place precompiled .cso files in the same directory for automatic loading if DXC compilation fails.
    

## Extending beyond ray tracing
----------------------------

You can call CompileShaderWithDXCAPI directly for any shader type and profile:

_std::vector bytecode;_

_if (!compiler.CompileShaderWithDXCAPI("MyShader.hlsl", "main", "ps\_6\_8", bytecode))_

_{_

    _compiler.CompileShaderWithExternalDXC("MyShader.hlsl", "main", "ps\_6\_8", bytecode);_

_}_

## Notes and tips
--------------

*   Add -I arguments if your shaders use relative includes:
    

_arguments.push\_back(L"-I");arguments.push\_back(wShaderDir);_

*   External compilation uses CreateProcessA and deletes the temp .cso after loading.
    
*   Placeholder shaders log warnings so you can distinguish them from working bytecode.
    
*   Windows‑only implementation — designed for D3D12 targets.
    

## Disclaimer
----------

This code was built as part of hardware‑accelerated ray tracing for CRYENGINE. Using it for other purposes has not been extensively tested.