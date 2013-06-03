#include "../smartplayer/packetqueue.h"

extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}

#include <algorithm>
#include <iostream>
#include <stdint.h>
#include <math.h>

#define TESTCASE (100)
#define MAXN (30000)
#define BSIZE (4)

int a[MAXN];

int my_rand(int max_r)
{
	return ((rand() << 16) | rand()) % max_r;
}

void test1()
{	
	for (int t = 0; t < TESTCASE; ++t)
	{
		int i;
		for (i = 0; i < MAXN; ++i)
		{
			a[i] = i * 10;
		}

		for (i = MAXN-1; i > 0; --i)
		{
			if (rand() % 6 == 0)
			{
				std::swap(a[i], a[my_rand(i + 1)]);
			}
		}

		std::cout << "data generated" << std::endl;
		
		PacketQueue pq(BSIZE);
		//PacketQueue pq;

		for (i = 0; i < MAXN; ++i)
		{
			AVPacket pkt;
			av_init_packet(&pkt);
			pkt.dts = a[i];
			pkt.data = (uint8_t*)(a[i]);
			if (! pq.insert(pkt))
			{
				std::cout << "unexpected" << std::endl;
			}
		}

		std::cout << "inserted" << std::endl;

		int64_t last_dts = -1;
		for (i = 0; i < MAXN - 1; ++i)
		{
			AVPacket current_pkt;
			int64_t next_k_pkt_dts;
			
			if (rand() % 50 == 0)
			{
				if (pq.upper_bound(last_dts, current_pkt, MAXN - i, next_k_pkt_dts))
				{
					if (next_k_pkt_dts != -1)
					{
						std::cout << "wrong. " << next_k_pkt_dts << " " << current_pkt.dts << " " << MAXN - i << std::endl;
					}
				}
				else
				{
					std::cout << "unexpected upper_bound fail" << std::endl;
				}
			}

			int k = my_rand((MAXN - i + 9) / 10);
			if (k == 0) k = 1;
			if (pq.upper_bound(last_dts, current_pkt, k, next_k_pkt_dts))
			{
				if (current_pkt.dts != i * 10 && current_pkt.data != (uint8_t*)(i * 10))
				{
					std::cout << "wrong. " << current_pkt.dts << " " << i << std::endl;
				}
				
				if (next_k_pkt_dts != current_pkt.dts + k * 10)
				{
					std::cout << "wrong. " << next_k_pkt_dts << " " << current_pkt.dts << " " << k << std::endl;
				}
				last_dts = current_pkt.dts;
			}
			else
			{
				std::cout << "unexpected upper_bound fail" << std::endl;
			}

			if (rand() % 3 != 0)
			{
				pq.pop_front();
			}
		}

		std::cout << "testcase " << t << " success" << std::endl;
	}
}

std::string test2()
{
	try
	{
		throw "123";

		return "123";
	}
	catch(...)
	{
		std::cout << "nice return" << std::endl;
	}
	return "321";
}

int main()
{
	srand(7331);

	//test1();

	auto ss = test2();

	return 0;
}