#include "ti_native.h"
#include "ti_web_shell.h"

#include <string>
#include <fstream>

TiNative::TiNative (TIWebShell *ti_web_shell)
{
	this->ti_web_shell = ti_web_shell;

	BindMethod("debug", &TiNative::debug);
	BindMethod("getResourcePath", &TiNative::getResourcePath);
	BindMethod("include", &TiNative::include);
}

void TiNative::debug (const CppArgumentList &args, CppVariant *result)
{
	if (args.size() > 0)
		printf("[titanium:debug]: %s\n", args[0].ToString().c_str());
}

void TiNative::getResourcePath(const CppArgumentList &args, CppVariant *result)
{
	std::wstring resourcePath = ti_web_shell->getResourcesPath();

	result->Set(WideToUTF8(resourcePath));
}

void TiNative::include(const CppArgumentList &args, CppVariant *result)
{
	if (args.size() > 0) {
		std::string relativeName = args[0].ToString();
		std::string relativePath = WideToUTF8(ti_web_shell->getResourcesPath());
		relativePath += "\\";
		relativePath += relativeName;

		std::ifstream in(relativePath.c_str());
		std::string s, line;
		while(getline(in, line)) {
			s += line + "\n";
		}

		WebView *webview = ti_web_shell->getHost()->webview();
		webview->GetMainFrame()->ExecuteJavaScript(s, relativePath);
	}
}