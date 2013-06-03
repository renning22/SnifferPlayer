#include "libCapGet.h"
#include "utility.hpp"
#include "parser.h"
#include "log.h"

#include <pcap.h>
#include <string>
#include <vector>
#include <boost/thread.hpp>

static std::vector< std::pair<std::string,std::string> > alldevs_name;
static char errbuf[PCAP_ERRBUF_SIZE];

static bool abort_request = false;
static std::vector<boost::shared_ptr<boost::thread>> threads;

int Initialize(int& error_code, std::string& error_info)
{
	pcap_if_t *alldevs;
	
	/* Retrieve the device list */
	if(pcap_findalldevs(&alldevs, errbuf) == -1)
	{
		error_code = 1;
		error_info = std::string("Error in pcap_findalldevs: ") + errbuf;
		return -1;
	}
	
	alldevs_name.clear();
	for(pcap_if_t *d = alldevs; d; d=d->next)
	{
		//TODO: 加一些筛选，去掉无关网卡
		alldevs_name.push_back( std::make_pair(d->name,d->description));
	}

	/* At this point, we don't need any more the device list. Free it */
	pcap_freealldevs(alldevs);
}

typedef struct ip_address
{
	u_char byte1;
	u_char byte2;
	u_char byte3;
	u_char byte4;
}ip_address;

/* IPv4 header */
typedef struct ip_header
{
	u_char	ver_ihl;		// Version (4 bits) + Internet header length (4 bits)
	u_char	tos;			// Type of service 
	u_short tlen;			// Total length 
	u_short identification; // Identification
	u_short flags_fo;		// Flags (3 bits) + Fragment offset (13 bits)
	u_char	ttl;			// Time to live
	u_char	proto;			// Protocol
	u_short crc;			// Header checksum
	ip_address	saddr;		// Source address
	ip_address	daddr;		// Destination address
	u_int	op_pad;			// Option + Padding
}ip_header;

typedef struct tcp_header{
	u_short sport; //源端口
	u_short dport; //目的端口
	u_int seq; //序列号
	u_int checkno; //确认号
	u_short hlen_sv_control;//HLEN(4 bit)+保留(6 bit)+控制字段(6 bit,URG+ACK+PSH+PST+SYN+FIN)
	u_short winsize;//窗口大小
	u_short checksum;//效验和
	u_short urgentp;//紧急指针
	u_int option; //选项和填充
}tcp_header;

void capture(pcap_t *adhandle, std::shared_ptr<CapGetDelegate> d,std::string description)
{
	DEBUG << "libCapGet : capture thread started , " << boost::this_thread::get_id() << " , " << description;
	int res;
	struct pcap_pkthdr *header;
	const u_char *pkt_data;

	while((res = pcap_next_ex( adhandle, &header, &pkt_data)) >= 0)
	{
		if (abort_request)
			return;

		if(res == 0)
		{
			/* Timeout elapsed */
			continue;
		}

		//DEBUG << "capturead " << header->caplen;

		u_int ethernet_header_len = 14;
		if( header->caplen < ethernet_header_len )
			continue;

		u_short ethernet_type = *(u_short*)(pkt_data+12);
		u_int ip_header_offset = 0;
		//DEBUG << "Ethernet : " << std::hex << ethernet_type;
		if( ethernet_type == 0x6488 )
		{
			// PPPoE session
			u_int PPPoE_header_len = 6;
			u_int PtoP_header_len = 2;
			ip_header_offset = ethernet_header_len + PPPoE_header_len + PtoP_header_len;
			//DEBUG << "Found a PPPoE session";
		}
		else
		{
			ip_header_offset = ethernet_header_len; // this is default lenght of ethernet
		}
		/* retireve the position of the ip header */
		ip_header *ih = (ip_header *) (pkt_data + ip_header_offset);
		u_int ip_len = (ih->ver_ihl & 0xf) * 4;

		// if HTTP
		if( ih->proto != 0x06 )
			continue;

		tcp_header *th = (tcp_header *) ((u_char*)ih + ip_len);
		u_int tcp_len = ((th->hlen_sv_control & 0xf0) >> 4) * 4;

		u_char* hh = (u_char*)th + tcp_len;

		int tcp_content_length = header->caplen - ip_header_offset - ip_len - tcp_len;

		unsigned int line_start = 0;
		bool first_line = true;
		std::string address, host, referer;

	
		// if GET
		if( tcp_content_length < 4 || !( hh[0] == 'G' && hh[1] == 'E' && hh[2] == 'T' || hh[3] == ' ' ) )
			continue;

		for (int i = 0; i < tcp_content_length - 1; ++i) //故意-1，下面有个地方可以少个判断
		{
			if (hh[i] == 0x0d && hh[i+1] == 0x0a)
			{
				if (i == line_start + 2) // 连续两个\r\n的情况,end of HTTP header
				{
					break;
				}
				if (first_line)
				{
					if (i >= 4) //其实这个条件肯定满足，保险起见还是多判下
					{
						unsigned int space_pos = 4;
						for (;space_pos < i; ++space_pos)
						{
							if (hh[space_pos] == 0x20)
							{
								break;
							}
						}
						address.assign((char*)hh + 4, space_pos - 4);
					}
					first_line = false;
				}
				else
				{
					int line_length = i - line_start;
					//std::string debug((char*)hh + line_start, line_length);
					//printf("%s\n", debug.c_str());
					if (line_length > 6 && hh[line_start] == 0x48 && hh[line_start + 1] == 0x6f && hh[line_start + 2] == 0x73 && hh[line_start + 3] == 0x74 
						&& hh[line_start + 4] == 0x3a && hh[line_start + 5] == 0x20)
					{
						host.assign((char*)hh + line_start + 6, line_length - 6);
						if (! referer.empty())
						{
							break;
						}
					}
					//"Referer: "
					else if (line_length > 8 && hh[line_start] == 'R' && hh[line_start + 1] == 'e' && hh[line_start + 2] == 'f'
						&& hh[line_start + 3] == 'e' && hh[line_start + 4] == 'r' && hh[line_start + 5] == 'e' && hh[line_start + 6] == 'r'
						&& hh[line_start + 7] == ':' && hh[line_start + 8] == ' ')
					{
						referer.assign((char*)hh + line_start + 9, line_length - 9);
						if (! host.empty())
						{
							break;
						}
					}
				}
				line_start = i + 2;
			}
		}

		auto url = parser_url_filter(host,address,referer);
		if( !url.empty() )
			d->GetCaptured(url);
	}

	DEBUG << "libCapGet : capture thread ended , " << boost::this_thread::get_id();
}

int StartCapture(std::shared_ptr<CapGetDelegate> d)
{
	for (auto it = alldevs_name.begin(); it != alldevs_name.end(); ++it)
	{		
		pcap_t *adhandle;

		if ((adhandle = pcap_open_live(it->first.c_str(),	// name of the device
								 65536,			// portion of the packet to capture. 
												// 65536 grants that the whole packet will be captured on all the MACs.
								 0,				// promiscuous mode (nonzero means promiscuous)
								 1000,			// read timeout
								 errbuf			// error buffer
								 )) == NULL)
		{
			continue;
		}

		bpf_u_int32 NetMask=0xffffff;
		struct bpf_program fcode;

		//compile the filter
		//const char * compiled_string = "tcp port http and tcp[20] = 0x47 && tcp[21] = 0x45 && tcp[22] = 0x54 && tcp[23] = 0x20";
		//const char * compiled_string = "tcp port http"; // for variant length
		const char * compiled_string = "";
		if(pcap_compile(adhandle, &fcode, compiled_string, 1, NetMask) < 0) //TODO: 不一定是20
		{
			pcap_close(adhandle);
			continue;
		}

		//set the filter
		if(pcap_setfilter(adhandle, &fcode)<0)
		{
			pcap_close(adhandle);
			continue;
		}


		boost::shared_ptr<boost::thread> thp = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&capture, adhandle, d, it->second)));
		threads.push_back(thp);
	}
	return threads.size();
}

void StopCapture()
{
	abort_request = true;

	for(auto it=threads.begin();it!=threads.end();it++)
		(*it)->join();
}
