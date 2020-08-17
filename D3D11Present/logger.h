#pragma once

#define APP_NAME "D3D11"
#define LOG_STREAM stdout

class Log
{
private:
	enum MsgType
	{
		INFO_MSG = 0,
		WARNING_MSG = 1,
		ERROR_MSG = 2
	};

	template <typename ...Args>
	static void LogMsg(MsgType type, const std::string& format, Args&& ...args)
	{
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

		fprintf(LOG_STREAM, "[");
		SetConsoleTextAttribute(hConsole, 11);
		fprintf(LOG_STREAM, APP_NAME);
		SetConsoleTextAttribute(hConsole, 7);
		fprintf(LOG_STREAM, "][");

		switch (type)
		{
		case INFO_MSG:
			SetConsoleTextAttribute(hConsole, 10);
			fprintf(LOG_STREAM, "INFO");
			break;
		case WARNING_MSG:
			SetConsoleTextAttribute(hConsole, 14);
			fprintf(LOG_STREAM, "WARNING");
			break;
		case ERROR_MSG:
			SetConsoleTextAttribute(hConsole, 12);
			fprintf(LOG_STREAM, "ERROR");
			break;
		}

		auto size = std::snprintf(nullptr, 0, format.c_str(), std::forward<Args>(args)...);
		std::string output(size + 1, '\0');
		std::sprintf(&output[0], format.c_str(), std::forward<Args>(args)...);

		SetConsoleTextAttribute(hConsole, 7);
		fprintf(LOG_STREAM, "] %s\n", output.c_str());
	}

public:
	template <typename ...Args>
	static void Info(const std::string& format, Args&& ...args)
	{
		LogMsg(INFO_MSG, format, std::forward<Args>(args)...);
	}

	template <typename ...Args>
	static void Warn(const std::string& format, Args&& ...args)
	{
		LogMsg(WARNING_MSG, format, std::forward<Args>(args)...);
	}

	template <typename ...Args>
	static void Error(const std::string& format, Args&& ...args)
	{
		LogMsg(ERROR_MSG, format, std::forward<Args>(args)...);
	}
};