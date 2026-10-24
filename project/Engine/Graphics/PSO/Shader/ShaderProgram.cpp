#include "Engine/Graphics/PSO/Shader/ShaderProgram.h"
#include "Engine/Graphics/PSO/Shader/ShaderCompiler.h"
#include <iostream>

bool ShaderProgram::CompileVS(
    const std::wstring& filepath,
    IDxcUtils* utils,
    IDxcCompiler3* compiler,
    IDxcIncludeHandler* includeHandler) {
    vs_ = ShaderCompiler::Compile(std::cout, filepath, L"vs_6_0", utils, compiler, includeHandler);
    return (vs_ != nullptr);
}

bool ShaderProgram::CompilePS(
    const std::wstring& filepath,
    IDxcUtils* utils,
    IDxcCompiler3* compiler,
    IDxcIncludeHandler* includeHandler) {
    ps_ = ShaderCompiler::Compile(std::cout, filepath, L"ps_6_0", utils, compiler, includeHandler);
    return (ps_ != nullptr);
}

bool ShaderProgram::CompileCS(
    const std::wstring& filepath,
    IDxcUtils* utils,
    IDxcCompiler3* compiler,
    IDxcIncludeHandler* includeHandler) {
    cs_ = ShaderCompiler::Compile(std::cout, filepath, L"cs_6_0", utils, compiler, includeHandler);
    return (cs_ != nullptr);
}

D3D12_SHADER_BYTECODE ShaderProgram::GetVS() const {
    if (!vs_) return { nullptr, 0 };
    return { vs_->GetBufferPointer(), vs_->GetBufferSize() };
}

D3D12_SHADER_BYTECODE ShaderProgram::GetPS() const {
    if (!ps_) return { nullptr, 0 };
    return { ps_->GetBufferPointer(), ps_->GetBufferSize() };
}

D3D12_SHADER_BYTECODE ShaderProgram::GetCS() const {
    if (!cs_) return { nullptr, 0 };
    return { cs_->GetBufferPointer(), cs_->GetBufferSize() };
}
