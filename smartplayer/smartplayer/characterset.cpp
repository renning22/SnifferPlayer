#include "characterset.h"

#include <windows.h>

using namespace std;

std::string UTF16toUTF8(std::wstring const & text)
{
	size_t length;
	length = WideCharToMultiByte(CP_UTF8,0,text.c_str(),-1,NULL,0,NULL,NULL);
	if( length != 0 )
	{
		string ret(length,0);
		length = WideCharToMultiByte(CP_UTF8,0,text.c_str(),-1,reinterpret_cast<LPSTR>(&*ret.begin()),ret.length(),NULL,NULL);
		if( length > 0 )
		{
			if( ret[ret.length()-1] == 0 )
				ret.pop_back();
			return ret;
		}
	}
	return string();
}

wstring ANSItoUTF16(string const & text)
{
	size_t length;
	length = MultiByteToWideChar(CP_ACP,0,text.c_str(),-1,NULL,0);
	if( length != 0 )
	{
		wstring ret(length,0);
		length = MultiByteToWideChar(CP_ACP,0,text.c_str(),-1,reinterpret_cast<LPWSTR>(&*ret.begin()),ret.length());
		if( length > 0 )
		{
			if( ret[ret.length()-1] == 0 )
				ret.pop_back();
			return ret;
		}
	}
	return wstring();
}

string ANSItoUTF8(string const & text)
{
	return UTF16toUTF8(ANSItoUTF16(text));
}