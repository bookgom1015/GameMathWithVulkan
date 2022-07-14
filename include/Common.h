#pragma once

#pragma comment(lib, "glfw3.lib")
#pragma comment(lib, "vulkan-1.lib")

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <glm/gtx/rotate_vector.hpp>

#include <algorithm>
#include <array>
#include <exception>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <vector>
#define NOMINMAX
#include <Windows.h>

#include "GameTimer.h"

#undef min
#undef max

#ifndef Log
	#define Log(x, ...)											\
	{															\
		std::vector<std::string> texts = { x, __VA_ARGS__ };	\
		std::stringstream _sstream;								\
																\
		for (const auto& text : texts)							\
			_sstream << text;									\
																\
		StringUtil::LogFunc(_sstream.str());					\
	}
#endif

#ifndef Logln
	#define Logln(x, ...)										\
	{															\
		std::vector<std::string> texts = { x, __VA_ARGS__ };	\
		std::stringstream _sstream;								\
																\
		for (const auto& text : texts)							\
			_sstream << text;									\
		_sstream << '\n';										\
																\
		StringUtil::LogFunc(_sstream.str());					\
	}
#endif

#ifndef WLog
	#define WLog(x, ...)										\
	{															\
		std::vector<std::wstring> texts = { x, __VA_ARGS__ };	\
		std::wstringstream _wsstream;							\
																\
		for (const auto& text : texts)							\
			_wsstream << text;									\
																\
		StringUtil::LogFunc(_wsstream.str());					\
	}
#endif

#ifndef WLogln
	#define WLogln(x, ...)										\
	{															\
		std::vector<std::wstring> texts = { x, __VA_ARGS__ };	\
		std::wstringstream _wsstream;							\
																\
		for (const auto& text : texts)							\
			_wsstream << text;									\
		_wsstream << L'\n';										\
																\
		StringUtil::LogFunc(_wsstream.str());					\
	}
#endif

#ifndef ReturnFalse
	#define ReturnFalse(__msg)														\
	{																				\
		std::wstringstream __wsstream_RIF;											\
		__wsstream_RIF << __FILE__ << L"; line: " << __LINE__ << L"; " << __msg;	\
		WLogln(__wsstream_RIF.str());												\
		return false;																\
	}
#endif

#ifndef CheckReturn
#define CheckReturn(__statement)															\
	{																						\
		try {																				\
			bool __result = __statement;													\
			if (!__result) {																\
				std::wstringstream __wsstream_CR;											\
				__wsstream_CR << __FILE__ << L"; line: " << __LINE__ << L"; ";				\
				WLogln(__wsstream_CR.str());												\
				return false;																\
			}																				\
		}																					\
		catch (const std::exception& e) {													\
			std::wstringstream __wsstream_CR;												\
				__wsstream_CR << __FILE__ << L"; line: " << __LINE__ << L"; " << e.what();	\
				WLogln(__wsstream_CR.str());												\
			return false;																	\
		}																					\
	}
#endif

static const glm::vec3 ZeroVector		= glm::vec3(0.0f);
static const glm::vec3 RightVector		= glm::vec3(1.0f, 0.0f, 0.0f);
static const glm::vec3 UpVector			= glm::vec3(0.0f, 1.0f, 0.0f);
static const glm::vec3 ForwardVector	= glm::vec3(0.0f, 0.0f, 1.0f);

namespace StringUtil {
	class StringUtilHelper {
	public:
		static HANDLE ghLogFile;

		static std::mutex gLogFileMutex;
	};

	inline void LogFunc(const std::string& text) {
		std::wstring wstr;
		wstr.assign(text.begin(), text.end());

		DWORD writtenBytes = 0;

		StringUtilHelper::gLogFileMutex.lock();

		WriteFile(
			StringUtilHelper::ghLogFile,
			wstr.c_str(),
			static_cast<DWORD>(wstr.length() * sizeof(wchar_t)),
			&writtenBytes,
			NULL
		);

		StringUtilHelper::gLogFileMutex.unlock();
	}

	inline void LogFunc(const std::wstring& text) {
		DWORD writtenBytes = 0;

		StringUtilHelper::gLogFileMutex.lock();

		WriteFile(
			StringUtilHelper::ghLogFile,
			text.c_str(),
			static_cast<DWORD>(text.length() * sizeof(wchar_t)),
			&writtenBytes,
			NULL
		);

		StringUtilHelper::gLogFileMutex.unlock();
	}

	inline void SetTextToWnd(HWND hWnd, LPCWSTR newText) {
		SetWindowText(hWnd, newText);
	}

	inline void AppendTextToWnd(HWND hWnd, LPCWSTR newText) {
		int finalLength = GetWindowTextLength(hWnd) + lstrlen(newText) + 1;
		wchar_t* buf = reinterpret_cast<wchar_t*>(std::malloc(finalLength * sizeof(wchar_t)));

		GetWindowText(hWnd, buf, finalLength);

		wcscat_s(buf, finalLength, newText);

		SetWindowText(hWnd, buf);

		std::free(buf);
	}
};
