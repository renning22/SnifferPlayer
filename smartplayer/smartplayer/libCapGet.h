#pragma once

#include <string>
#include <memory>

class CapGetDelegate
{
public:
	virtual void GetCaptured(std::string url) = 0;

	virtual void GetCaptured(std::string url, std::string header) = 0;
};

int Initialize(int& error_code, std::string& error_info); //返回网卡个数。返回-1表示有错误发生。

int StartCapture(std::shared_ptr<CapGetDelegate> d); //返回成功开始Capture的网卡个数。

void StopCapture();