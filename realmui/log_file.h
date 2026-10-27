#pragma once
#include <share.h>

namespace Win
{
	class WideFileSink final : public spdlog::sinks::base_sink<std::mutex>
	{
	public:
		explicit WideFileSink(const std::wstring& a_path) noexcept :
			file(_wfsopen(a_path.c_str(), L"wb", _SH_DENYNO))
		{}

		~WideFileSink() override
		{
			if (file) {
				std::fclose(file);
			}
		}

		[[nodiscard]] bool IsOpen() const noexcept { return file != nullptr; }

	protected:
		void sink_it_(const spdlog::details::log_msg& a_msg) override
		{
			if (!file) {
				return;
			}
			spdlog::memory_buf_t line;
			formatter_->format(a_msg, line);
			std::fwrite(line.data(), 1, line.size(), file);
		}

		void flush_() override
		{
			if (file) {
				std::fflush(file);
			}
		}

	private:
		std::FILE* file = nullptr;
	};

	inline int OpenLog(const wchar_t* a_name) noexcept
	{
		const auto use = [](const std::wstring& a_path) noexcept {
			try {
				auto sink = std::make_shared<WideFileSink>(a_path);
				if (!sink->IsOpen()) {
					return false;
				}
				auto log = std::make_shared<spdlog::logger>("RealmUI", std::move(sink));
				log->set_level(spdlog::level::info);
				log->flush_on(spdlog::level::info);
				log->set_pattern("[%H:%M:%S.%e] [%t] [%L] %v");
				spdlog::set_default_logger(std::move(log));
				return true;
			} catch (...) {
				return false;
			}
		};
		try {
			if (const auto folder = SKSE::log::log_directory(); folder && use((*folder / a_name).wstring())) {
				return 1;
			}
		} catch (...) {}
		try {
			wchar_t     temp[MAX_PATH + 2] = {};
			const DWORD n = GetTempPathW(MAX_PATH + 1, temp);
			if (n > 0 && n <= MAX_PATH && use(std::wstring(temp, n) + a_name)) {
				return 2;
			}
		} catch (...) {}
		return 0;
	}
}
