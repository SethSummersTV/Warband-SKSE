#pragma once

#include "utf8_path.h"

namespace PortraitPick
{
	inline std::atomic<bool> busy{ false };
	inline constexpr const char* kFolder = "Data/PrismaUI/views/Warband/portraits/custom/";

	inline bool SafeKey(const std::string& a_key)
	{
		return !a_key.empty() && a_key.size() < 160 && a_key.find("..") == std::string::npos &&
		       a_key.find_first_of("/\\:*?\"<>|") == std::string::npos;
	}

	inline std::wstring Wide(const std::string& a_utf8)
	{
		return Utf8Path::Wide(a_utf8);
	}

	inline std::wstring AskForFile()
	{
		SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
		HANDLE rd = nullptr, wr = nullptr;
		if (!CreatePipe(&rd, &wr, &sa, 0)) {
			return {};
		}
		SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
		std::wstring cmd =
			L"powershell.exe -NoProfile -NonInteractive -STA -WindowStyle Hidden -Command \""
			L"Add-Type -AssemblyName System.Windows.Forms;"
			L"$d = New-Object System.Windows.Forms.OpenFileDialog;"
			L"$d.Title = 'Warband: choose a picture';"
			L"$d.Filter = 'Pictures|*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.webp;*.tif;*.tiff|All files|*.*';"
			L"$f = New-Object System.Windows.Forms.Form; $f.TopMost = $true;"
			L"if ($d.ShowDialog($f) -eq 'OK') { [Console]::OutputEncoding = [Text.Encoding]::UTF8; [Console]::Out.Write($d.FileName) }\"";
		STARTUPINFOW si{};
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdOutput = wr;
		si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
		PROCESS_INFORMATION pi{};
		std::string bytes;
		if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
			CloseHandle(wr);
			wr = nullptr;
			char buf[1024];
			DWORD got = 0;
			while (ReadFile(rd, buf, sizeof(buf), &got, nullptr) && got > 0 && bytes.size() < 8192) {
				bytes.append(buf, got);
			}
			WaitForSingleObject(pi.hProcess, 2000);
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
		}
		if (wr) {
			CloseHandle(wr);
		}
		CloseHandle(rd);
		if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF && static_cast<unsigned char>(bytes[1]) == 0xBB) {
			bytes.erase(0, 3);
		}
		while (!bytes.empty() && (bytes.back() == '\r' || bytes.back() == '\n' || bytes.back() == ' ')) {
			bytes.pop_back();
		}
		return Wide(bytes);
	}

	inline bool SaveSquareJpeg(const std::wstring& a_from, const std::wstring& a_to)
	{
		using Microsoft::WRL::ComPtr;
		ComPtr<IWICImagingFactory> wic;
		if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic)))) {
			return false;
		}
		ComPtr<IWICBitmapDecoder> decoder;
		ComPtr<IWICBitmapFrameDecode> frame;
		ComPtr<IWICFormatConverter> rgb;
		ComPtr<IWICBitmapClipper> clip;
		ComPtr<IWICBitmapScaler> scale;
		UINT w = 0, h = 0;
		if (FAILED(wic->CreateDecoderFromFilename(a_from.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder)) ||
			FAILED(decoder->GetFrame(0, &frame)) || FAILED(frame->GetSize(&w, &h)) || w == 0 || h == 0 || w > 20000 || h > 20000) {
			return false;
		}
		if (FAILED(wic->CreateFormatConverter(&rgb)) ||
			FAILED(rgb->Initialize(frame.Get(), GUID_WICPixelFormat24bppBGR, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
			return false;
		}
		const UINT side = (std::min)(w, h);
		WICRect rc{ static_cast<INT>((w - side) / 2), static_cast<INT>(h > w ? (h - side) / 4 : 0), static_cast<INT>(side), static_cast<INT>(side) };
		if (FAILED(wic->CreateBitmapClipper(&clip)) || FAILED(clip->Initialize(rgb.Get(), &rc)) ||
			FAILED(wic->CreateBitmapScaler(&scale)) || FAILED(scale->Initialize(clip.Get(), 256, 256, WICBitmapInterpolationModeFant))) {
			return false;
		}
		ComPtr<IWICStream> stream;
		ComPtr<IWICBitmapEncoder> encoder;
		ComPtr<IWICBitmapFrameEncode> out;
		ComPtr<IPropertyBag2> props;
		if (FAILED(wic->CreateStream(&stream)) || FAILED(stream->InitializeFromFilename(a_to.c_str(), GENERIC_WRITE)) ||
			FAILED(wic->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &encoder)) ||
			FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)) || FAILED(encoder->CreateNewFrame(&out, &props))) {
			return false;
		}
		PROPBAG2 option{};
		wchar_t quality[] = L"ImageQuality";
		option.pstrName = quality;
		VARIANT value{};
		value.vt = VT_R4;
		value.fltVal = 0.92f;
		props->Write(1, &option, &value);
		WICPixelFormatGUID format = GUID_WICPixelFormat24bppBGR;
		return SUCCEEDED(out->Initialize(props.Get())) && SUCCEEDED(out->SetSize(256, 256)) && SUCCEEDED(out->SetPixelFormat(&format)) &&
		       SUCCEEDED(out->WriteSource(scale.Get(), nullptr)) && SUCCEEDED(out->Commit()) && SUCCEEDED(encoder->Commit());
	}

	inline void Pick(std::string a_key, std::function<void(bool, std::string)> a_done)
	{
		if (!SafeKey(a_key) || busy.exchange(true)) {
			return;
		}
		std::error_code ec;
		const auto folder = std::filesystem::absolute(kFolder, ec);
		if (ec) {
			busy.store(false);
			return;
		}
		std::thread([key = std::move(a_key), folder, done = std::move(a_done)]() {
			bool saved = false;
			const auto from = AskForFile();
			if (!from.empty()) {
				std::error_code ec2;
				std::filesystem::create_directories(folder, ec2);
				const auto to = (folder / (Wide(key) + L".jpg")).wstring();
				if (SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
					saved = SaveSquareJpeg(from, to);
					CoUninitialize();
				}
				if (!saved) {
					std::filesystem::remove(to, ec2);
				}
			}
			busy.store(false);
			done(saved, key);
		}).detach();
	}

	inline bool Clear(const std::string& a_key)
	{
		if (!SafeKey(a_key)) {
			return false;
		}
		std::error_code ec;
		bool any = false;
		for (const char* ext : { ".jpg", ".png" }) {
			any = std::filesystem::remove(std::filesystem::path(kFolder) / (Wide(a_key) + Wide(ext)), ec) || any;
		}
		return any;
	}
}
