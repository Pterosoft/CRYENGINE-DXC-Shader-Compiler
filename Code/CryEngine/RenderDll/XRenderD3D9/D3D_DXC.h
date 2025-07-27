#pragma once

#include "StdAfx.h"

class CCompiler
{
public:
	void CreatePlaceholderShaders();
	string GetEngineShaderDirectory();
	bool LoadShaderFile(const char* filename, std::vector<BYTE>& bytecode);
	bool CompileRayTracingShadersFromSource();
	bool CompileShaderWithDXCAPI(const char* sourcePath,
		const char* entryPoint,
		const char* target,
		std::vector<BYTE>& bytecode);
	bool CompileShaderWithExternalDXC(const char* sourcePath,
		const char* entryPoint,
		const char* target,
		std::vector<BYTE>& bytecode);
	bool ValidateShaderBytecode();
	void CreateShaderBytecode();
	bool CompileRayTracingShaders();
	bool LoadPrecompiledShaders();

	// Getters for shader bytecode
	const std::vector<BYTE>& GetRayGenShaderBytecode() const { return m_rayGenShaderBytecode; }
	const std::vector<BYTE>& GetMissShaderBytecode() const { return m_missShaderBytecode; }
	const std::vector<BYTE>& GetClosestHitShaderBytecode() const { return m_closestHitShaderBytecode; }

	// Getters for shader sizes
	size_t GetRayGenShaderSize() const { return m_rayGenShaderSize; }
	size_t GetMissShaderSize() const { return m_missShaderSize; }
	size_t GetClosestHitShaderSize() const { return m_closestHitShaderSize; }

private:
	// Shader bytecode (precompiled since CryEngine doesn't support dxc)
	std::vector<BYTE>           m_rayGenShaderBytecode;
	std::vector<BYTE>           m_missShaderBytecode;
	std::vector<BYTE>           m_closestHitShaderBytecode;

	// Shader metadata
	size_t                      m_rayGenShaderSize;
	size_t                      m_missShaderSize;
	size_t                      m_closestHitShaderSize;
};