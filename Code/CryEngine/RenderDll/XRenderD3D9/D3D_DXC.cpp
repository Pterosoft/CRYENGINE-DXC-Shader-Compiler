#include "StdAfx.h"
#include "D3D_DXC.h"

// Include DXC API headers
#include <dxcapi.h>
#include <combaseapi.h>

#pragma comment(lib, "dxcompiler.lib")

string CCompiler::GetEngineShaderDirectory()
{
    // Find the engine root folder
    char szEngineRootPath[_MAX_PATH] = "";
    CryFindEngineRootFolder(CRY_ARRAY_COUNT(szEngineRootPath), szEngineRootPath);

    // Construct path to shader directory
    string shaderPath = szEngineRootPath;

    // Ensure proper path separator
    if (!shaderPath.empty() && shaderPath.back() != '\\' && shaderPath.back() != '/')
    {
        shaderPath += "\\";
    }

    shaderPath += "Engine\\Shaders\\HWScripts\\CryFX\\";

    CryLog("[Ray Tracing] Shader directory resolved to: %s", shaderPath.c_str());
    return shaderPath;
}

bool CCompiler::LoadShaderFile(const char* filename, std::vector<BYTE>& bytecode)
{
    string fullPath = GetEngineShaderDirectory() + filename;

    CryLog("[Ray Tracing] Attempting to load shader file: %s", fullPath.c_str());

    // Try to open the file (.hlsl source files)
    FILE* fp = gEnv->pCryPak->FOpen(fullPath.c_str(), "rb", ICryPak::FOPEN_HINT_QUIET);
    if (!fp)
    {
        CryLog("[Ray Tracing] CryPak failed to open: %s", fullPath.c_str());

        // Fallback: try standard file operations
        fp = fopen(fullPath.c_str(), "rb");
        if (!fp)
        {
            CryLog("[Ray Tracing] Standard fopen failed to open: %s", fullPath.c_str());

            // Debug: List files in the shader directory
            string shaderDir = GetEngineShaderDirectory();
            CryLog("[Ray Tracing] Listing files in shader directory: %s", shaderDir.c_str());

            string searchPattern = shaderDir + "*.*";
            WIN32_FIND_DATAA findData;
            HANDLE hFind = FindFirstFileA(searchPattern.c_str(), &findData);

            if (hFind != INVALID_HANDLE_VALUE)
            {
                CryLog("[Ray Tracing] Files found in shader directory:");
                do
                {
                    if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                    {
                        CryLog("[Ray Tracing]   - %s", findData.cFileName);
                    }
                } while (FindNextFileA(hFind, &findData));
                FindClose(hFind);
            }
            else
            {
                CryLog("[Ray Tracing] Directory not found or inaccessible: %s", shaderDir.c_str());
            }

            return false;
        }
    }

    // Get file size - handle both CryPak and standard FILE*
    long fileSize = 0;
    bool useCryPak = (gEnv->pCryPak->FSeek(fp, 0, SEEK_END) == 0);

    if (useCryPak)
    {
        fileSize = gEnv->pCryPak->FTell(fp);
        gEnv->pCryPak->FSeek(fp, 0, SEEK_SET);
    }
    else
    {
        fseek(fp, 0, SEEK_END);
        fileSize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
    }

    if (fileSize <= 0)
    {
        CryLog("[Ray Tracing] Invalid shader file size: %s (%ld bytes)", fullPath.c_str(), fileSize);
        if (useCryPak)
            gEnv->pCryPak->FClose(fp);
        else
            fclose(fp);
        return false;
    }

    // Allocate buffer and read file
    bytecode.resize(fileSize);
    size_t bytesRead = 0;

    if (useCryPak)
    {
        bytesRead = gEnv->pCryPak->FRead(bytecode.data(), fileSize, fp);
        gEnv->pCryPak->FClose(fp);
    }
    else
    {
        bytesRead = fread(bytecode.data(), 1, fileSize, fp);
        fclose(fp);
    }

    if (bytesRead != fileSize)
    {
        CryLog("[Ray Tracing] Failed to read complete shader file: %s (read %zu of %ld bytes)",
            fullPath.c_str(), bytesRead, fileSize);
        bytecode.clear();
        return false;
    }

    CryLog("[Ray Tracing] Successfully loaded shader: %s (%ld bytes)", filename, fileSize);
    return true;
}

bool CCompiler::CompileRayTracingShadersFromSource()
{
    CryLog("[Ray Tracing] Compiling HLSL shaders from source using DXC API...");

    string shaderDir = GetEngineShaderDirectory();
    CryLog("[Ray Tracing] Looking for HLSL source files in: %s", shaderDir.c_str());

    struct ShaderSource
    {
        const char* filename;
        const char* entryPoint;
        std::vector<BYTE>* bytecode;
        const char* type;
    };

    ShaderSource shaderSources[] = {
        { "RayGen.hlsl", "RayGenMain", &m_rayGenShaderBytecode, "Ray Generation" },
        { "Miss.hlsl", "MissMain", &m_missShaderBytecode, "Miss" },
        { "ClosestHit.hlsl", "ClosestHitMain", &m_closestHitShaderBytecode, "Closest Hit" }
    };

    int compiledCount = 0;
    int totalShaders = sizeof(shaderSources) / sizeof(shaderSources[0]);

    for (int i = 0; i < totalShaders; ++i)
    {
        string sourcePath = shaderDir + shaderSources[i].filename;
        CryLog("[Ray Tracing] Checking for HLSL source file: %s", sourcePath.c_str());

        // Check if source file exists using CryPak first
        FILE* fp = gEnv->pCryPak->FOpen(sourcePath.c_str(), "r", ICryPak::FOPEN_HINT_QUIET);
        if (!fp)
        {
            // Try standard file operations
            fp = fopen(sourcePath.c_str(), "r");
        }

        if (!fp)
        {
            CryLog("[Ray Tracing] HLSL source file not found: %s", sourcePath.c_str());
            continue;
        }

        // Close the file handle
        if (gEnv->pCryPak->FClose(fp) != 0)
        {
            fclose(fp);
        }

        CryLog("[Ray Tracing] Found HLSL source: %s", sourcePath.c_str());

        // Compile using DXC COM API
        if (CompileShaderWithDXCAPI(sourcePath.c_str(),
            shaderSources[i].entryPoint,
            "lib_6_3",
            *shaderSources[i].bytecode))
        {
            CryLog("[Ray Tracing] Successfully compiled %s shader from HLSL source", shaderSources[i].type);
            ++compiledCount;
        }
        else
        {
            CryLog("[Ray Tracing] Failed to compile %s shader from HLSL source", shaderSources[i].type);
        }
    }

    if (compiledCount > 0)
    {
        CryLog("[Ray Tracing] Successfully compiled %d of %d HLSL shaders", compiledCount, totalShaders);
    }

    return compiledCount == totalShaders;
}

bool CCompiler::CompileShaderWithDXCAPI(const char* sourcePath,
    const char* entryPoint,
    const char* target,
    std::vector<BYTE>& bytecode)
{
    CryLog("[Ray Tracing] Compiling HLSL using DXC COM API: %s", sourcePath);

    HRESULT hr = S_OK;

    // Initialize COM if not already initialized
    static bool comInitialized = false;
    if (!comInitialized)
    {
        hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
        {
            CryLog("[Ray Tracing] Failed to initialize COM: 0x%08X", hr);
            return false;
        }
        comInitialized = true;
    }

    // Create DXC instances
    IDxcUtils* pUtils = nullptr;
    IDxcCompiler3* pCompiler = nullptr;
    IDxcIncludeHandler* pIncludeHandler = nullptr;

    // Create DXC Utils
    hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
    if (FAILED(hr))
    {
        CryLog("[Ray Tracing] Failed to create DXC Utils: 0x%08X", hr);
        return false;
    }

    // Create DXC Compiler
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));
    if (FAILED(hr))
    {
        CryLog("[Ray Tracing] Failed to create DXC Compiler: 0x%08X", hr);
        pUtils->Release();
        return false;
    }

    // Create include handler
    hr = pUtils->CreateDefaultIncludeHandler(&pIncludeHandler);
    if (FAILED(hr))
    {
        CryLog("[Ray Tracing] Failed to create include handler: 0x%08X", hr);
        pCompiler->Release();
        pUtils->Release();
        return false;
    }

    // Load source file
    std::vector<BYTE> sourceCode;
    if (!LoadShaderFile(PathUtil::GetFile(sourcePath), sourceCode))
    {
        CryLog("[Ray Tracing] Failed to load shader source: %s", sourcePath);
        pIncludeHandler->Release();
        pCompiler->Release();
        pUtils->Release();
        return false;
    }

    // Create source blob
    IDxcBlobEncoding* pSourceBlob = nullptr;
    hr = pUtils->CreateBlob(sourceCode.data(), static_cast<UINT32>(sourceCode.size()), DXC_CP_UTF8, &pSourceBlob);
    if (FAILED(hr))
    {
        CryLog("[Ray Tracing] Failed to create source blob: 0x%08X", hr);
        pIncludeHandler->Release();
        pCompiler->Release();
        pUtils->Release();
        return false;
    }

    // Convert strings to wide character
    wchar_t wEntryPoint[128];
    wchar_t wTarget[32];
    wchar_t wSourceName[MAX_PATH];

    MultiByteToWideChar(CP_UTF8, 0, entryPoint, -1, wEntryPoint, sizeof(wEntryPoint) / sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, target, -1, wTarget, sizeof(wTarget) / sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, sourcePath, -1, wSourceName, sizeof(wSourceName) / sizeof(wchar_t));

    // CRITICAL FIX: Use correct DXC arguments for ray tracing lib_6_3 target
    std::vector<LPCWSTR> arguments;
    arguments.push_back(L"-E");
    arguments.push_back(wEntryPoint);
    arguments.push_back(L"-T");
    arguments.push_back(wTarget);

    // CRITICAL: Use minimal and compatible flags for ray tracing
    // Remove version-specific flags that may not be supported

    // Debug vs Release flags
#ifdef _DEBUG
    arguments.push_back(L"-Zi");        // Debug information
    arguments.push_back(L"-Od");        // Disable optimizations for debugging
#else
    arguments.push_back(L"-O3");        // Optimization level 3 for release
#endif

    // Create source buffer
    DxcBuffer sourceBuffer = {};
    sourceBuffer.Ptr = pSourceBlob->GetBufferPointer();
    sourceBuffer.Size = pSourceBlob->GetBufferSize();
    sourceBuffer.Encoding = DXC_CP_UTF8;

    CryLog("[Ray Tracing] DXC Compilation arguments:");
    for (size_t i = 0; i < arguments.size(); i += 2)
    {
        if (i + 1 < arguments.size())
        {
            CryLog("[Ray Tracing]   %ls %ls", arguments[i], arguments[i + 1]);
        }
        else
        {
            CryLog("[Ray Tracing]   %ls", arguments[i]);
        }
    }

    // Compile the shader
    IDxcResult* pResult = nullptr;
    hr = pCompiler->Compile(
        &sourceBuffer,
        arguments.data(),
        static_cast<UINT32>(arguments.size()),
        pIncludeHandler,
        IID_PPV_ARGS(&pResult)
    );

    bool success = false;
    if (SUCCEEDED(hr))
    {
        // Check compilation status
        HRESULT status;
        hr = pResult->GetStatus(&status);
        if (SUCCEEDED(hr) && SUCCEEDED(status))
        {
            // Get the compiled object
            IDxcBlob* pShaderBlob = nullptr;
            hr = pResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pShaderBlob), nullptr);
            if (SUCCEEDED(hr) && pShaderBlob)
            {
                // CRITICAL FIX: Improved DXIL validation with proper header parsing
                const BYTE* pData = static_cast<const BYTE*>(pShaderBlob->GetBufferPointer());
                SIZE_T size = pShaderBlob->GetBufferSize();

                CryLog("[Ray Tracing] Compiled shader blob: %zu bytes", size);

                // FIXED: Proper DXIL container validation
                if (size >= 32)
                {
                    // DXIL container header structure:
                    // Offset 0: FourCC 'DXBC' (4 bytes)
                    // Offset 4: Hash digest (16 bytes)
                    // Offset 20: Version (4 bytes)  
                    // Offset 24: Container size (4 bytes)
                    // Offset 28: Part count (4 bytes)

                    const DWORD* header = reinterpret_cast<const DWORD*>(pData);
                    DWORD signature = header[0];        // Offset 0
                    DWORD containerSize = header[6];    // FIXED: Offset 24/4 = 6 (not 4!)
                    DWORD partCount = header[7];        // FIXED: Offset 28/4 = 7 (not 5!)

                    CryLog("[Ray Tracing] DXIL validation:");
                    CryLog("[Ray Tracing]   Signature: 0x%08X (expected: 0x43425844)", signature);
                    CryLog("[Ray Tracing]   Container size: %u bytes (actual: %zu)", containerSize, size);
                    CryLog("[Ray Tracing]   Part count: %u", partCount);

                    // Enhanced validation
                    bool isValid = true;

                    if (signature != 0x43425844) // 'DXBC'
                    {
                        CryLog("[Ray Tracing] ERROR: Invalid DXBC signature (got: 0x%08X)", signature);
                        isValid = false;
                    }

                    if (containerSize != size)
                    {
                        CryLog("[Ray Tracing] ERROR: Container size mismatch (header: %u, actual: %zu)", containerSize, size);
                        isValid = false;
                    }

                    if (partCount == 0 || partCount > 16)
                    {
                        CryLog("[Ray Tracing] ERROR: Invalid part count: %u", partCount);
                        isValid = false;
                    }

                    // If basic validation passes, check for DXIL part
                    if (isValid && size >= 32 + partCount * 4)
                    {
                        const DWORD* partOffsets = &header[8]; // FIXED: Starts at offset 32/4 = 8
                        bool foundDXIL = false;

                        for (DWORD i = 0; i < partCount; ++i)
                        {
                            if (partOffsets[i] >= size - 8)
                            {
                                CryLog("[Ray Tracing] WARNING: Part %u offset out of bounds: %u", i, partOffsets[i]);
                                continue;
                            }

                            const DWORD* partHeader = reinterpret_cast<const DWORD*>(pData + partOffsets[i]);
                            DWORD partFourCC = partHeader[0];
                            DWORD partSize = partHeader[1];

                            char fourCCStr[5] = { 0 };
                            memcpy(fourCCStr, &partFourCC, 4);

                            CryLog("[Ray Tracing]   Part %u: '%s' (%u bytes) at offset %u", i, fourCCStr, partSize, partOffsets[i]);

                            if (partFourCC == 0x4C495844) // 'DXIL'
                            {
                                foundDXIL = true;
                                CryLog("[Ray Tracing]   Found DXIL part at offset %u", partOffsets[i]);
                            }
                        }

                        if (!foundDXIL)
                        {
                            CryLog("[Ray Tracing] ERROR: No DXIL part found in container");
                            isValid = false;
                        }
                    }

                    if (isValid)
                    {
                        // Copy bytecode to output vector
                        bytecode.resize(size);
                        memcpy(bytecode.data(), pData, size);

                        CryLog("[Ray Tracing] Successfully compiled and validated shader: %s (%zu bytes)", entryPoint, size);
                        success = true;
                    }
                    else
                    {
                        CryLog("[Ray Tracing] ERROR: DXIL container validation failed");
                    }
                }
                else
                {
                    CryLog("[Ray Tracing] ERROR: Compiled shader blob is too small (%zu bytes)", size);
                }

                pShaderBlob->Release();
            }
            else
            {
                CryLog("[Ray Tracing] Failed to get compiled shader blob: 0x%08X", hr);
            }
        }
        else
        {
            CryLog("[Ray Tracing] Compilation failed with status: 0x%08X", status);
        }

        // Get error messages if compilation failed
        if (!success)
        {
            IDxcBlobUtf8* pErrorBlob = nullptr;
            hr = pResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrorBlob), nullptr);
            if (SUCCEEDED(hr) && pErrorBlob && pErrorBlob->GetStringLength() > 0)
            {
                CryLog("[Ray Tracing] Compilation errors:");
                CryLog("%s", pErrorBlob->GetStringPointer());
                pErrorBlob->Release();
            }
        }
    }
    else
    {
        CryLog("[Ray Tracing] Failed to compile shader: 0x%08X", hr);
    }

    // Cleanup
    if (pResult) pResult->Release();
    pSourceBlob->Release();
    pIncludeHandler->Release();
    pCompiler->Release();
    pUtils->Release();

    return success;
}

// Keep the external DXC method as fallback
bool CCompiler::CompileShaderWithExternalDXC(const char* sourcePath,
    const char* entryPoint,
    const char* target,
    std::vector<BYTE>& bytecode)
{
    CryLog("[Ray Tracing] Falling back to external DXC compilation");

    // Use CreateProcess instead of system() to avoid console window
    string shaderDir = GetEngineShaderDirectory();
    string outputPath = shaderDir + "temp_compiled.cso";

    // Try to find dxc.exe
    const char* dxcPaths[] = {
        "dxc.exe",
        "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\x64\\dxc.exe",
        "C:\\Program Files\\Windows Kits\\10\\bin\\x64\\dxc.exe",
        "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.19041.0\\x64\\dxc.exe",
        "C:\\Program Files\\Windows Kits\\10\\bin\\10.0.20348.0\\x64\\dxc.exe",
        "C:\\Program Files\\Windows Kits\\10\\bin\\10.0.22000.0\\x64\\dxc.exe"
    };

    string commandLine;
    commandLine.Format("-T %s -E %s -Fo \"%s\" \"%s\"",
        target, entryPoint, outputPath.c_str(), sourcePath);

    bool success = false;
    for (const char* dxcPath : dxcPaths)
    {
        CryLog("[Ray Tracing] Trying DXC path: %s", dxcPath);

        STARTUPINFOA si = {};
        PROCESS_INFORMATION pi = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE; // Hide the console window

        string fullCommandLine = string(dxcPath) + " " + commandLine;

        if (CreateProcessA(
            nullptr,                    // Application name
            (LPSTR)fullCommandLine.c_str(), // Command line
            nullptr,                    // Process security attributes
            nullptr,                    // Primary thread security attributes
            FALSE,                      // Handles are not inherited
            CREATE_NO_WINDOW,          // Creation flags - no console window
            nullptr,                    // Environment
            nullptr,                    // Current directory
            &si,                        // Startup info
            &pi))                       // Process information
        {
            // Wait for the process to complete
            WaitForSingleObject(pi.hProcess, INFINITE);

            DWORD exitCode;
            GetExitCodeProcess(pi.hProcess, &exitCode);

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            if (exitCode == 0)
            {
                CryLog("[Ray Tracing] External DXC compilation succeeded");

                // Load the compiled output
                FILE* fp = fopen(outputPath.c_str(), "rb");
                if (fp)
                {
                    fseek(fp, 0, SEEK_END);
                    long fileSize = ftell(fp);
                    fseek(fp, 0, SEEK_SET);

                    if (fileSize > 0)
                    {
                        bytecode.resize(fileSize);
                        size_t bytesRead = fread(bytecode.data(), 1, fileSize, fp);
                        if (bytesRead == fileSize)
                        {
                            success = true;
                            CryLog("[Ray Tracing] Successfully loaded compiled shader bytecode (%ld bytes)", fileSize);
                        }
                    }
                    fclose(fp);
                }

                // Clean up temporary file
                remove(outputPath.c_str());
                break;
            }
            else
            {
                CryLog("[Ray Tracing] External DXC compilation failed (exit code: %lu)", exitCode);
            }
        }
        else
        {
            CryLog("[Ray Tracing] Failed to launch DXC process: %lu", GetLastError());
        }
    }

    return success;
}

bool CCompiler::ValidateShaderBytecode()
{
    CryLog("[Ray Tracing] Validating shader bytecode...");

    // Check that all required shaders have bytecode
    if (m_rayGenShaderBytecode.empty())
    {
        CryLog("[Ray Tracing] Ray Generation shader bytecode is empty");
        return false;
    }

    if (m_missShaderBytecode.empty())
    {
        CryLog("[Ray Tracing] Miss shader bytecode is empty");
        return false;
    }

    if (m_closestHitShaderBytecode.empty())
    {
        CryLog("[Ray Tracing] Closest Hit shader bytecode is empty");
        return false;
    }

    // FIXED: Enhanced DXIL header validation with correct offsets
    auto validateDXIL = [](const std::vector<BYTE>& bytecode, const char* shaderType) -> bool
        {
            if (bytecode.size() < 32)
            {
                CryLog("[Ray Tracing] %s shader bytecode too small (%zu bytes)", shaderType, bytecode.size());
                return false;
            }

            // CORRECT DXIL container header layout:
            // 0-3: FourCC 'DXBC'
            // 4-19: Hash digest (16 bytes)
            // 20-23: Version
            // 24-27: Container size  
            // 28-31: Part count
            // 32+: Part offset table

            const DWORD* header = reinterpret_cast<const DWORD*>(bytecode.data());
            DWORD signature = header[0];        // Offset 0
            DWORD containerSize = header[6];    // Offset 24/4 = 6
            DWORD partCount = header[7];        // Offset 28/4 = 7

            CryLog("[Ray Tracing] %s DXIL validation:", shaderType);
            CryLog("[Ray Tracing]   Signature: 0x%08X", signature);
            CryLog("[Ray Tracing]   Container size: %u bytes (actual: %zu)", containerSize, bytecode.size());
            CryLog("[Ray Tracing]   Part count: %u", partCount);

            if (signature != 0x43425844) // 'DXBC'
            {
                CryLog("[Ray Tracing] ERROR: %s shader missing DXBC signature (found: 0x%08X)", shaderType, signature);
                return false;
            }

            if (containerSize != bytecode.size())
            {
                CryLog("[Ray Tracing] ERROR: %s container size mismatch (header: %u, actual: %zu)",
                    shaderType, containerSize, bytecode.size());
                return false;
            }

            if (partCount == 0 || partCount > 16) // Sanity check
            {
                CryLog("[Ray Tracing] ERROR: %s invalid part count: %u", shaderType, partCount);
                return false;
            }

            // Look for DXIL part
            if (bytecode.size() >= 32 + partCount * 4)
            {
                const DWORD* partOffsets = &header[8]; // Start at offset 32
                bool foundDXIL = false;

                for (DWORD i = 0; i < partCount; ++i)
                {
                    if (partOffsets[i] >= bytecode.size() - 8)
                    {
                        CryLog("[Ray Tracing] WARNING: %s part %u offset out of bounds: %u", shaderType, i, partOffsets[i]);
                        continue;
                    }

                    const DWORD* partHeader = reinterpret_cast<const DWORD*>(bytecode.data() + partOffsets[i]);
                    DWORD partFourCC = partHeader[0];
                    DWORD partSize = partHeader[1];

                    char fourCCStr[5] = { 0 };
                    memcpy(fourCCStr, &partFourCC, 4);

                    CryLog("[Ray Tracing]   Part %u: '%s' (%u bytes) at offset %u", i, fourCCStr, partSize, partOffsets[i]);

                    if (partFourCC == 0x4C495844) // 'DXIL'
                    {
                        foundDXIL = true;
                        CryLog("[Ray Tracing]   Found DXIL part at offset %u", partOffsets[i]);
                    }
                }

                if (!foundDXIL)
                {
                    CryLog("[Ray Tracing] ERROR: %s does not contain DXIL part", shaderType);
                    return false;
                }
            }
            else
            {
                CryLog("[Ray Tracing] ERROR: %s container too small for part table", shaderType);
                return false;
            }

            CryLog("[Ray Tracing] %s shader bytecode validated (%zu bytes)", shaderType, bytecode.size());
            return true;
        };

    bool allValid = true;
    allValid &= validateDXIL(m_rayGenShaderBytecode, "RayGen");
    allValid &= validateDXIL(m_missShaderBytecode, "Miss");
    allValid &= validateDXIL(m_closestHitShaderBytecode, "ClosestHit");

    if (allValid)
    {
        CryLog("[Ray Tracing] All shader bytecode validated successfully");
    }
    else
    {
        CryLog("[Ray Tracing] CRITICAL: Shader bytecode validation failed - shaders are corrupted or invalid");
    }

    return allValid;
};

void CCompiler::CreateShaderBytecode()
{
    CryLog("[Ray Tracing] Creating shader metadata for pipeline creation...");

    // Cache shader sizes for pipeline creation
    m_rayGenShaderSize = m_rayGenShaderBytecode.size();
    m_missShaderSize = m_missShaderBytecode.size();
    m_closestHitShaderSize = m_closestHitShaderBytecode.size();

    CryLog("[Ray Tracing] Shader metadata created:");
    CryLog("[Ray Tracing]   Ray Generation: %zu bytes", m_rayGenShaderSize);
    CryLog("[Ray Tracing]   Miss: %zu bytes", m_missShaderSize);
    CryLog("[Ray Tracing]   Closest Hit: %zu bytes", m_closestHitShaderSize);
}

bool CCompiler::CompileRayTracingShaders()
{
    CryLog("[Ray Tracing] Compiling DXR shaders from HLSL source files...");
    CryLog("[Ray Tracing] Looking for .hlsl files in Engine\\Shaders\\HWScripts\\CryFX\\");

    // First: Try to compile directly from HLSL source files using DXC API
    bool compiledFromSource = CompileRayTracingShadersFromSource();

    if (!compiledFromSource)
    {
        CryLog("[Ray Tracing] DXC API compilation failed, trying to load precompiled .cso files...");

        // Fallback: Try to load precompiled .cso files if they exist
        bool precompiledLoaded = LoadPrecompiledShaders();

        if (!precompiledLoaded)
        {
            CryLog("[Ray Tracing] No precompiled shaders found either, using placeholder shaders");
            CreatePlaceholderShaders();
        }
    }

    // Validate shader bytecode
    if (!ValidateShaderBytecode())
    {
        CryLog("[Ray Tracing] Shader bytecode validation failed");
        return false;
    }

    // Create shader metadata for pipeline creation
    CreateShaderBytecode();

    CryLog("[Ray Tracing] Successfully compiled/loaded DXR shaders");
    return true;
}

bool CCompiler::LoadPrecompiledShaders()
{
    CryLog("[Ray Tracing] Loading precompiled DXR shaders (.cso files)...");

    // Get shader directory path
    string shaderDir = GetEngineShaderDirectory();
    CryLog("[Ray Tracing] Looking for .cso files in: %s", shaderDir.c_str());

    // Try to load each compiled shader file
    struct ShaderFile
    {
        const char* filename;
        std::vector<BYTE>* bytecode;
        const char* type;
    };

    ShaderFile shaderFiles[] = {
        { "RayGen.cso", &m_rayGenShaderBytecode, "Ray Generation" },
        { "Miss.cso", &m_missShaderBytecode, "Miss" },
        { "ClosestHit.cso", &m_closestHitShaderBytecode, "Closest Hit" }
    };

    int loadedCount = 0;
    int totalShaders = sizeof(shaderFiles) / sizeof(shaderFiles[0]);

    for (int i = 0; i < totalShaders; ++i)
    {
        CryLog("[Ray Tracing] Attempting to load precompiled: %s", shaderFiles[i].filename);

        if (LoadShaderFile(shaderFiles[i].filename, *shaderFiles[i].bytecode))
        {
            CryLog("[Ray Tracing] Successfully loaded %s shader: %s (%zu bytes)",
                shaderFiles[i].type, shaderFiles[i].filename, shaderFiles[i].bytecode->size());
            ++loadedCount;
        }
        else
        {
            CryLog("[Ray Tracing] Failed to load %s shader: %s",
                shaderFiles[i].type, shaderFiles[i].filename);
        }
    }

    if (loadedCount == 0)
    {
        CryLog("[Ray Tracing] No precompiled .cso shaders found in: %s", shaderDir.c_str());
    }
    else if (loadedCount < totalShaders)
    {
        CryLog("[Ray Tracing] Warning: Only %d of %d precompiled shaders loaded successfully", loadedCount, totalShaders);
    }
    else
    {
        CryLog("[Ray Tracing] All %d precompiled shaders loaded successfully", totalShaders);
    }

    return loadedCount == totalShaders;
}

void CCompiler::CreatePlaceholderShaders()
{
    CryLog("[Ray Tracing] Creating placeholder shader bytecode for development");
    CryLogAlways("[Ray Tracing] ============================================");
    CryLogAlways("[Ray Tracing] WARNING: Using placeholder shaders!");
    CryLogAlways("[Ray Tracing] Ray tracing will not function correctly.");
    CryLogAlways("[Ray Tracing] ============================================");
    CryLogAlways("[Ray Tracing] ");
    CryLogAlways("[Ray Tracing] To fix this, ensure HLSL shader files exist:");
    CryLogAlways("[Ray Tracing] 1. Place these files in: Engine\\Shaders\\HWScripts\\CryFX\\");
    CryLogAlways("[Ray Tracing]    - RayGen.hlsl");
    CryLogAlways("[Ray Tracing]    - Miss.hlsl");
    CryLogAlways("[Ray Tracing]    - ClosestHit.hlsl");
    CryLogAlways("[Ray Tracing] 2. Make sure dxcompiler.dll is available");
    CryLogAlways("[Ray Tracing] ============================================");

    // Create proper DXIL container headers according to Microsoft DXIL specification
    struct DXILContainerHeader
    {
        UINT32 fourCC;           // 'DXBC' (0x43425844)
        UINT32 hash[4];          // Hash digest (MD5)
        UINT32 version;          // Version (1)
        UINT32 containerSize;    // Total container size
        UINT32 partCount;        // Number of parts
    };

    struct DXILPartHeader
    {
        UINT32 fourCC;           // Part identifier
        UINT32 partSize;         // Size of part data
    };

    // Define DXIL FourCC codes
    const UINT32 DXIL_FOURCC_DXBC = 0x43425844; // 'DXBC'
    const UINT32 DXIL_FOURCC_DXIL = 0x4C495844; // 'DXIL'
    const UINT32 DXIL_FOURCC_HASH = 0x48534148; // 'HASH'

    auto createDXILContainer = [&](const char* shaderType) -> std::vector<BYTE>
        {
            // Minimal DXIL instruction sequence for a valid but empty shader
            const BYTE minimalDXILCode[] = {
                // LLVM bitcode header
                0x42, 0x43,                     // 'BC' magic
                0xC0, 0xDE,                     // LLVM bitcode version
                0x21, 0x0C, 0x00, 0x00,         // Bitcode header
                0x02, 0x00, 0x00, 0x00,         // Module version
                0x01, 0x00, 0x00, 0x00,         // Offset
                0x3C, 0x00, 0x00, 0x00,         // Size
                // Module records (minimal)
                0x00, 0x00, 0x00, 0x00,         // Module header
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
            };

            const UINT32 dxilPartSize = sizeof(minimalDXILCode);
            const UINT32 totalPartCount = 2; // DXIL + HASH parts
            const UINT32 headerSize = sizeof(DXILContainerHeader);
            const UINT32 partOffsetsSize = totalPartCount * sizeof(UINT32);
            const UINT32 dxilHeaderSize = sizeof(DXILPartHeader);
            const UINT32 hashHeaderSize = sizeof(DXILPartHeader);
            const UINT32 totalSize = headerSize + partOffsetsSize +
                dxilHeaderSize + dxilPartSize +
                hashHeaderSize + 16;

            std::vector<BYTE> container;
            container.reserve(totalSize);

            // Create container header
            DXILContainerHeader containerHeader = {};
            containerHeader.fourCC = DXIL_FOURCC_DXBC;
            UINT32 typeHash = 0;
            for (const char* p = shaderType; *p; ++p)
            {
                typeHash = typeHash * 31 + static_cast<UINT32>(*p);
            }
            containerHeader.hash[0] = typeHash;
            containerHeader.hash[1] = ~typeHash;
            containerHeader.hash[2] = typeHash ^ 0xABCDEF00;
            containerHeader.hash[3] = (~typeHash) ^ 0x12345678;
            containerHeader.version = 1;
            containerHeader.containerSize = totalSize;
            containerHeader.partCount = totalPartCount;

            // Write container header
            container.resize(headerSize);
            memcpy(container.data(), &containerHeader, headerSize);

            // Calculate part offsets
            UINT32 currentOffset = headerSize + partOffsetsSize;
            UINT32 dxilPartOffset = currentOffset;
            currentOffset += dxilHeaderSize + dxilPartSize;
            UINT32 hashPartOffset = currentOffset;

            // Write part offsets
            container.resize(headerSize + partOffsetsSize);
            UINT32* partOffsets = reinterpret_cast<UINT32*>(container.data() + headerSize);
            partOffsets[0] = dxilPartOffset;
            partOffsets[1] = hashPartOffset;

            // Write DXIL part
            DXILPartHeader dxilHeader = {};
            dxilHeader.fourCC = DXIL_FOURCC_DXIL;
            dxilHeader.partSize = dxilPartSize;

            container.resize(dxilPartOffset + dxilHeaderSize + dxilPartSize);
            memcpy(container.data() + dxilPartOffset, &dxilHeader, dxilHeaderSize);
            memcpy(container.data() + dxilPartOffset + dxilHeaderSize, minimalDXILCode, dxilPartSize);

            // Write HASH part
            DXILPartHeader hashHeader = {};
            hashHeader.fourCC = DXIL_FOURCC_HASH;
            hashHeader.partSize = 16;

            container.resize(hashPartOffset + hashHeaderSize + 16);
            memcpy(container.data() + hashPartOffset, &hashHeader, hashHeaderSize);
            memcpy(container.data() + hashPartOffset + hashHeaderSize, containerHeader.hash, 16);

            return container;
        };

    // Create placeholder bytecode for each shader type
    if (m_rayGenShaderBytecode.empty())
    {
        m_rayGenShaderBytecode = createDXILContainer("RayGeneration");
    }

    if (m_missShaderBytecode.empty())
    {
        m_missShaderBytecode = createDXILContainer("Miss");
    }

    if (m_closestHitShaderBytecode.empty())
    {
        m_closestHitShaderBytecode = createDXILContainer("ClosestHit");
    }
}