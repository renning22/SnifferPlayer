#pragma once

#include <string>
#include <memory>

class CapGetDelegate
{
public:
	virtual void GetCaptured(std::string url) = 0;

	virtual void GetCaptured(std::string url, std::string header) = 0;
};

int Initialize(int& error_code, std::string& error_info); //������������������-1��ʾ�д�������

int StartCapture(std::shared_ptr<CapGetDelegate> d); //���سɹ���ʼCapture������������

void StopCapture();