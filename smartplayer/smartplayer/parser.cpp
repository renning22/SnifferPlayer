#include "log.h"
#include "parser.h"
#include "http.h"
#include "url.h"
#include "website.h"

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/regex.hpp>
#include <sstream>

using namespace std;

static std::shared_ptr<boost::asio::io_service>		working_ioservice;

static std::vector<std::shared_ptr<boost::thread>>	working_threads;

static boost::recursive_mutex						local_variables_lock;

static std::set<std::string>						current_parsing_urls;

static bool											abort_request = false;



static vector<string> parse_by_nnkk(string url)
{
	vector<string> file_paths;

	INFO << "parse_by_nnkk : " << url << " , by thread " << boost::this_thread::get_id();

	try
	{
		string content;
		try
		{
			content = http_get_content(website_parser_url(url));
		}
		catch(string e)
		{
			throw "\n"+e;
		}

		if( content.compare(0,5,"Sorry") == 0 )
		{
			throw content;
		}

		stringstream ss(content);
		string file_path;
		while( std::getline(ss,file_path) )
		{
			if( !url_validate(file_path) )
				throw string("Invalid url : "+file_path);
			file_paths.push_back(file_path);
		}

		if( file_paths.empty() )
		{
			DEBUG << content;
			throw string("Empty results");
		}
	}
	catch(string e)
	{
		throw string("parse_by_nnkk : failed , ") + e;
	}

	return file_paths;
}

static vector<string> parse_by_flvcd(string url)
{
	vector<string> file_paths;

	INFO << "parse_by_flvcd : " << url << " , by thread " << boost::this_thread::get_id();

	try
	{
		string content;
		try
		{
			content = http_get_content(website_flvcd_parser_url(url)+"&format=high&flag=one");
		}
		catch(string e)
		{
			throw "\n"+e;
		}

		if( content.find("请输入密码") != string::npos )
		{
			throw string("Need a password");
		}
		else if( content.find("提示：对不起，FLVCD暂不支持此地址的解析") != string::npos )
		{
			throw string("Not support");
		}
		else if( content.find("解析失败，请确认视频是否被删除、地址是否正确，或稍后重试......") != string::npos )
		{
			throw string("Deleted");
		}

		// first way
		{
			stringstream ss(content);
			string aline;
			while( std::getline(ss,aline) )
			{
				if( aline.compare(0,3,"<U>") == 0 )
				{
					string file_path = aline.substr(3);
					file_paths.push_back(file_path);
				}
			}

			if( !file_paths.empty() )
				DEBUG << "parse_by_flvcd : by first way";
		}

		// second way
		if( file_paths.empty() )
		{
			stringstream ss(content);
			string aline;
			while( std::getline(ss,aline) )
			{
				const char * anchor_string = "    <br>下载地址：";
				if( aline.compare(0,strlen(anchor_string),anchor_string) == 0 )
					break;
			}
			while( std::getline(ss,aline) )
			{
				const char * end_string = "    </td>";
				if( aline.compare(0,strlen(end_string),end_string) == 0 )
					break;

				const char * feature_string = "<a href=\"";
				if( aline.find(feature_string) != string::npos )
				{
					string first_portion = aline.substr( aline.find(feature_string) + strlen(feature_string) );
					string file_path = first_portion.substr(0,first_portion.find_first_of('"'));
					file_paths.push_back(file_path);
				}
			}

			if( !file_paths.empty() )
				DEBUG << "parse_by_flvcd : by second way";
		}
		
		if( file_paths.empty() )
		{
			// no way...
			DEBUG << content;
			throw string("Empty results");
		}

		for(auto i=file_paths.begin();i!=file_paths.end();i++)
		if( !url_validate(*i) )
			throw string("Invalid url : "+*i);
	}
	catch(string e)
	{
		throw string("parse_by_flvcd : failed , ") + e;
	}
	return file_paths;
}

static boost::function<vector<string>(string)> parser_select(string const & url)
{
	string host = url_host(url);
	//if( host.find("youku.com") != string::npos )
	{
	//	return parse_by_nnkk;
	}
	//else
	{
		return parse_by_flvcd;
	}
}

bool parser_init()
{
	working_ioservice = std::make_shared<boost::asio::io_service>();

	for(int i=0;i!=5;i++)
	{
		working_threads.push_back( std::make_shared<boost::thread>(
		[=]()
		{
			DEBUG << "parser : Working thread started , " << boost::this_thread::get_id();
			while( !abort_request )
			{
				//DEBUG << "parser : loop , " << boost::this_thread::get_id();
				if( !working_ioservice->poll() )
					boost::this_thread::sleep( boost::posix_time::milliseconds(100) );
			}
			DEBUG << "parser : Working thread ended , " << boost::this_thread::get_id();
		}) );
	}
	return true;
}

void parser_uninit()
{
	abort_request = true;
	for(auto it=working_threads.begin();it!=working_threads.end();it++)
		(*it)->join();
}

string parser_url_filter(string host,string uri,string referer)
{
	string ret;
	string const & address = uri;

	if (host == "v.youku.com")
	{
		if (address.compare(0, 11, "/v_show/id_") == 0 && (address.compare(address.size() - 5, 5, ".html") == 0 || (address.find(".html?f=") != std::string::npos) || (address.find(".html?s=") != std::string::npos) ))
		{
			ret = "http://"+host+address;
		}
	}
	else if (referer.compare(0, 29, "http://v.youku.com/v_show/id_") == 0 && (referer.compare(referer.size() - 5, 5, ".html") == 0 || (referer.find(".html?f=") != std::string::npos)))
	{
		ret = referer;
	}
	/*
	else if (referer.compare(0, 63, "http://imgcache.qq.com/tencentvideo_v1/player/TencentPlayer.swf") == 0)
	{
		//通过referrer可能稍慢，会稍放一下广告(1-2s)，但别的途径特征不明显
		boost::smatch what;
		if ( boost::regex_search(address, what, boost::regex("^/[^/]+/\\w{11}\\.")) && what.size() && what[0].matched)
		{
			ret = what[0].str(); //故意不把前面的host加上去，因为这里host意义不大，而且加上去不利判重
		}
	}
	*/
	else if (host == "video.sina.com.cn" )
	{
		ret = "http://" + host + address;
	}
	else if (host == "www.tudou.com")
	{
		if (address.compare(0, 11, "/albumplay/") == 0 && address.size() > 11 
			|| address.compare(0, 15, "/programs/view/") == 0 && address.size() > 15 
			|| address.compare(0, 10, "/playlist/") == 0 && address.size() > 10
			|| address.compare(0, 11, "/playindex/") == 0 && address.size() > 11
			|| address.compare(0, 10, "/listplay/") == 0 && address.size() > 10)
		{
			ret = "http://" + host + address;
		}
	}
	else if (referer.compare(0, 20, "http://www.tudou.com") == 0)
	{
		if (referer.compare(20, 10, "/albumplay") == 0 || referer.compare(20, 14, "/programs/view") == 0 || referer.compare(20, 9, "/playlist") == 0
			|| referer.compare(20, 10, "/playindex") == 0 || referer.compare(20, 9, "/listplay") == 0)
		{
			ret = referer;
		}
	}
	else if (host == "www.letv.com")
	{
		if( boost::regex_match(address, boost::regex("/ptv/(vplay/\\d+|pplay/\\d+/\\d+)\\.html")) )
		{
			ret = "http://" + host + address;
		}
	}
	else if (referer.compare(0, 24, "http://www.letv.com/ptv/") == 0)
	{
		if (boost::regex_match(referer.substr(24), boost::regex("(vplay/\\d+|pplay/\\d+/\\d+)\\.html")) )
		{
			ret = referer;
		}
	}
	/*
	else if (host == "cache.video.qiyi.com")
	{
		if ( boost::regex_match(address, boost::regex("/vd/\\d{1,8}/\\w{32}/")) ) //也回发出"/vi/\\d{1,8}/\\w{32}/"，一样的id。暂时只sniffer vd这种
		{
			ret = "http://" + host + address; //把前面的host加上去只是为了方便服务器区分是哪个视频网站的，其实这里host意义不大，主要是要后面的id有用
		}
	}
	*/
	/* todo
	else if (host == "www.iqiyi.com")
	{
		ret = "http://" + host + address;
	}
	*/
	/* todo
	else if (host == "tv.sohu.com")
	{
		ret = "http://" + host + address;
	}
	*/
	else if (host == "v.ku6.com") //not test ok
	{
		//if ( boost::regex_match(address, boost::regex("(|/film|/special)/show/\\w*\\.html")) )
		{
			ret = "http://" + host + address;
		}
	}
	//pptv
	//163 (fail)
	//joy.cn (fail)
	else if (host == "6.cn") //not test ok
	{
		//if ( boost::regex_match(address, boost::regex("/watch/\\d*\\.html")) )
		{
			ret = "http://" + host + address;
		}
	}
	/*
	else if (host == "tieba.baidu.com")
	{
		if ( boost::regex_match(address, boost::regex("/shipin/bw/video/play.*")) || boost::regex_match(address, boost::regex("/f.*")) )
		{
			ret = "http://" + host + address;
		}
	}
	*/
	else if (host == "v.pps.tv")
	{
		ret = "http://" + host + address;
	}
	else if (host == "video.kankan.com" || host == "vod.kankan.com")
	{
		ret = "http://" + host + address;
	}
	else if (host == "news.cntv.cn" || host == "dianying.cntv.cn")
	{
		ret = "http://" + host + address;
	}
	else if (host == "www.56.com")
	{
		//这种sniffer原始地址的方法还是不好，浏览器有缓存的话就可能直接不发生相应的GET了。得sniffer后面的地址
		//if (boost::regex_match(address, boost::regex("/\\w\\d{2}/(((v_\\w+|play_album-aid-\\d+_vid-\\w+|album-aid-\\d+)\\.html)|(play_album\\.phtml\\?aid=\\d+&vid=\\w+))")))
		{
			ret = "http://" + host + address;
		}
	}
	else if (host == "www.yinyuetai.com")
	{
		if( boost::regex_match(address, boost::regex("/video/\\d+")) )
		{
			ret = "http://" + host + address;
		}
	}
	else if (host == "you.joy.cn" || host == "auto.joy.cn" || host == "v.joy.cn" || host == "news.joy.cn" )
	{
		ret = "http://" + host + address;
	}
	else if (host == "space.tv.cctv.com"
		|| host == "www.cctv.com"
		|| host == "v.cctv.com"
		|| host == "sports.cctv.com"
		|| host == "space.tv.cctv.com"
		|| host == "www.m1905.com"
		|| host == "v.ifeng.com"
		|| host == "www.jsbc.com"
		|| host == "www.jstv.com"
		|| host == "tv.btv.com.cn"
		|| host == "space.btv.com.cn"
		|| host == "www.gztv.com"
		|| host == "ent.hunantv.com"
		|| host == "www.hunantv.com"
		|| host == "www.imgo.tv"
		|| host == "tv.cztv.com"
		|| host == "www.cztv.com"
		|| host == "podcast.tvscn.com"
		|| host == "www.taihaitv.cn"
		|| host == "v.iqilu.com"
		|| host == "news.xinhuanet.com"
		|| host == "www.umiwi.com"
		|| host == "i.mtime.com"
		|| host == "movie.mtime.com"
		|| host == "tv.v1.cn"
		|| host == "v.v1.cn"
		|| host == "shehui.v1.cn"
		|| host == "miniv.v1.cn"
		|| host == "v.v1.cn"
		|| host == "v.zol.com.cn"
		|| host == "game.zol.com.cn"
		|| host == "tv.tom.com"
		|| host == "youku.soufun.com"
		|| host == "www.hualu5.com"
		|| host == "www.boosj.com"
		|| host == "video.baomihua.com"
		|| host == "play.hupu.tv"
		|| host == "v.163.com"
		|| host == "v.qq.com" 
		|| host == "www.acfun.tv"
		|| host == "www.bilibili.tv" ) //琥珀网 后面还有一山小网站 (额外加入了www.yinyuetai.com和v.163.com)
	{
		ret = "http://" + host + address;
	}
	return ret;
}

void parser_parse(std::string url,boost::function<void(std::vector<std::string>)> async)
{
	{
		boost::lock_guard<boost::recursive_mutex> guard(local_variables_lock);
		current_parsing_urls.insert(url);
	}
	working_ioservice->post( [=]()
	{
		DEBUG << "parser_parse : " << boost::this_thread::get_id();

		vector<string> urls;

		auto parser = parser_select(url);
		try
		{
			urls = parser(url);
		}
		catch(string e)
		{
			// parse failed, todo
			WARN << "parser_parse : failed , " << url << "\n" << e;
		}

		{
			boost::lock_guard<boost::recursive_mutex> guard(local_variables_lock);
			current_parsing_urls.erase(url);
		}
		async(urls);
	});
}

bool parser_is_parsing_url(std::string url)
{
	boost::lock_guard<boost::recursive_mutex> guard(local_variables_lock);
	return current_parsing_urls.count(url)>0;
}