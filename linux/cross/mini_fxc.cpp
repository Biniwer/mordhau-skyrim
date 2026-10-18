// The part of Microsoft's fxc.exe that DirectXTK's Src/Shaders/CompileShaders.cmd uses, for building DirectXTK on Linux
// (directxtk-shaders.sh runs it under Wine with Microsoft's d3dcompiler_47.dll: the same compiler fxc is a front end to).
// Reads lines "source|target|entry|output.inc|variable" and compiles each as fxc would with CompileShaders.cmd's
// /WX /Ges /Zi /Zpc /Qstrip_reflect /Qstrip_debug, writing the /Fh header fxc writes.
#include <windows.h>
#include <d3dcompiler.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static std::wstring Wide(const std::string& a_text)
{
	std::wstring out(a_text.size(), L'\0');
	out.resize(MultiByteToWideChar(CP_UTF8, 0, a_text.data(), static_cast<int>(a_text.size()), out.data(), static_cast<int>(out.size())));
	return out;
}

int main(int argc, char** argv)
{
	if (argc != 2) {
		std::fprintf(stderr, "usage: mini_fxc <list>\n");
		return 2;
	}
	std::ifstream list(argv[1]);
	std::string   line;
	int           failed = 0, done = 0;
	while (std::getline(list, line)) {
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}
		std::vector<std::string> f;
		std::stringstream        split(line);
		for (std::string part; std::getline(split, part, '|');) {
			f.push_back(part);
		}
		if (f.size() != 5) {
			continue;
		}
		const UINT flags = D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG | D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR;
		ID3DBlob*  code = nullptr;
		ID3DBlob*  errors = nullptr;
		HRESULT    hr = D3DCompileFromFile(Wide(f[0]).c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, f[2].c_str(), f[1].c_str(), flags, 0, &code, &errors);
		if (errors) {
			std::fprintf(stderr, "%s", static_cast<const char*>(errors->GetBufferPointer()));
			errors->Release();
		}
		ID3DBlob* stripped = nullptr;
		if (SUCCEEDED(hr)) {
			hr = D3DStripShader(code->GetBufferPointer(), code->GetBufferSize(), D3DCOMPILER_STRIP_REFLECTION_DATA | D3DCOMPILER_STRIP_DEBUG_INFO, &stripped);
		}
		if (FAILED(hr)) {
			std::fprintf(stderr, "failed: %s %s (0x%08lx)\n", f[0].c_str(), f[2].c_str(), static_cast<unsigned long>(hr));
			++failed;
		} else {
			std::ofstream out(f[3], std::ios::binary);
			const auto*   bytes = static_cast<const unsigned char*>(stripped->GetBufferPointer());
			const size_t  size = stripped->GetBufferSize();
			out << "#if 0\n// " << f[0] << " " << f[1] << " " << f[2] << " (mini_fxc)\n#endif\n\nconst BYTE " << f[4] << "[] =\n{\n";
			char hex[8];
			for (size_t i = 0; i < size; ++i) {
				std::snprintf(hex, sizeof(hex), "%4u", bytes[i]);
				out << (i % 12 == 0 ? "\n   " : "") << hex << (i + 1 < size ? "," : "");
			}
			out << "\n};\n";
			++done;
		}
		if (stripped) {
			stripped->Release();
		}
		if (code) {
			code->Release();
		}
	}
	std::printf("mini_fxc: %d compiled, %d failed\n", done, failed);
	return failed ? 1 : 0;
}
