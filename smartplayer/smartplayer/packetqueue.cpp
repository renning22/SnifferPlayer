#include "packetqueue.h"

extern "C"
{
#include "libavformat/avformat.h"
}

#include <boost/thread/locks.hpp>
#include <numeric>

bool AVPacketLess::operator()(int64_t dts, AVPacket* pkt)
{
	return dts < pkt->dts;
}

bool AVPacketLess::operator()(AVPacket* pkt, int64_t dts)
{
	return pkt->dts < dts;
}

bool PacketBlockLess::operator() (int64_t dts, PacketBlock* pb) //dts和block的比较：定义为dts和block最后一个元素的比较。dts要比最后一个元素小，才是比整个block小。
{
	return dts < pb->back()->dts;
}

bool PacketBlockLess::operator() (PacketBlock* pb, int64_t dts)
{
	return pb->back()->dts < dts;
}

static AVPacket eof_pkt = {0,std::numeric_limits<int64_t>::max(),(uint8_t*)"eof",0};

AVPacket& PacketQueue::get_eof_packet()
{
	return eof_pkt;
}

bool PacketQueue::is_eof_packet(const AVPacket& pkt)
{
	return pkt.data == eof_pkt.data;
}

int64_t PacketQueue::size() const
{
	return size_;
}

void PacketQueue::free_pkt(AVPacket *pkt) //内部方法，因此不用加锁
{
	size_ -= pkt->size;

	if (!PacketQueue::is_eof_packet(*pkt))
	{
		if (pkt->data)
		{
			av_free_packet(pkt);
		}
		delete pkt;
	}
}

bool PacketQueue::insert(const AVPacket& pkt_)
{
	AVPacket* pkt = new AVPacket(pkt_);

	boost::lock_guard<boost::mutex> lock(mutex);

	size_ += pkt->size;

	//找第一个大于等于dts的block，如果能找到，说明将插在中间；通常是找不到的，即插在末尾
	auto it_block = std::lower_bound(blocks.begin(), blocks.end(), pkt->dts, PacketBlockLess());
	
	if (it_block == blocks.end())
	{
		if (blocks.empty() || blocks.back()->size() == m_block_size)
		{
			//全空，或者末尾的block满了，开个新的block
			blocks.push_back(new PacketBlock());
		}
		blocks.back()->push_back(pkt);
	}
	else
	{
		//如果pkt小于该block的首个pkt，说明pkt其实应该插在上个block的尾部；
		if (pkt->dts < (*it_block)->front()->dts)
		{
			if (it_block == blocks.begin()) //本身就是第一个，则在这之前插入一个block
			{
				it_block = blocks.insert(it_block, new PacketBlock());
			}
			else
			{
				it_block --; //往前挪一个

				if ((*it_block)->size() == m_block_size)
				{
					//如果满了，则在此之后插入个新的block
					it_block ++;
					it_block = blocks.insert(it_block, new PacketBlock());
				}
			}
			(*it_block)->push_back(pkt);
		}
		else
		{
			//否则，说明pkt是在该block的中间。除非有dts相等的，否则需要将该block拆成两半，将pkt放在前面的block里
			auto it_pkt = std::lower_bound((*it_block)->begin(), (*it_block)->end(), pkt->dts, AVPacketLess());
			
			//dts相等，覆盖并返回false；
			if (pkt->dts == (*it_pkt)->dts)
			{
				free_pkt(*it_pkt);
				*it_pkt = pkt;

				return false;
			}
			else
			{
				//此时，it_pkt > dts，将从it_pkt开始的放到后面的半个block里，然后把pkt放在前面的半个block里
				PacketBlock* old_pb = *it_block;
				PacketBlock* new_pb = new PacketBlock(it_pkt, old_pb->end());

				it_block ++;
				blocks.insert(it_block, new_pb);

				old_pb->erase(it_pkt, old_pb->end());
				old_pb->push_back(pkt);
			}
		}			
	}

	return true;
}

bool PacketQueue::pop_front()
{
	boost::lock_guard<boost::mutex> lock(mutex);

	if (blocks.empty())
	{
		return false;
	}
	else
	{
		free_pkt(blocks.front()->front());
		blocks.front()->pop_front();
		if (blocks.front()->empty())
		{
			delete blocks.front();
			blocks.pop_front();
		}

		return true;
	}
}

void PacketQueue::clear()
{	
	boost::lock_guard<boost::mutex> lock(mutex);

	for (auto it = blocks.begin(); it != blocks.end(); ++it)
	{
		for (auto it2 = (*it)->begin(); it2 != (*it)->end(); ++it2)
		{
			free_pkt(*it2);
		}
		delete *it;
	}
	blocks.clear();
}

bool PacketQueue::upper_bound(int64_t dts, AVPacket& pkt, int k, int64_t& next_k_pkt_dts)
{		
	boost::lock_guard<boost::mutex> lock(mutex);

	auto it_block = std::upper_bound(blocks.begin(), blocks.end(), dts, PacketBlockLess());

	if (it_block == blocks.end())
	{
		return false;
	}
	else
	{
		auto it_pkt = std::upper_bound((*it_block)->begin(), (*it_block)->end(), dts, AVPacketLess());
		pkt = *(*it_pkt);

		if( k <= 0 )
			return true;

		while (k >= (*it_block)->end() - it_pkt)
		{
			k -= (*it_block)->end() - it_pkt;
			it_block ++;
			if (it_block == blocks.end())
			{
				next_k_pkt_dts = -1;
				return true;
			}
			it_pkt = (*it_block)->begin();
		}
		it_pkt += k;
		next_k_pkt_dts = (*it_pkt)->dts;
		return true;
	}
}
/*
//std::map 版本
bool PacketQueue::insert(const AVPacket& pkt_)
{
	AVPacket* pkt = new AVPacket(pkt_);

	boost::lock_guard<boost::mutex> lock(mutex);

	size_ += pkt->size;

	auto insert_result = data.insert(std::make_pair(pkt->dts, pkt));
	if (insert_result.second)
	{
		return true;
	}
	else
	{
		free_pkt(insert_result.first->second);
		insert_result.first->second = pkt;

		return false;
	}
}

bool PacketQueue::pop_front()
{
	boost::lock_guard<boost::mutex> lock(mutex);

	if (data.empty())
	{
		return false;
	}
	else
	{
		free_pkt(data.begin()->second);
		data.erase(data.begin());
		return true;
	}
}

void PacketQueue::clear()
{	
	boost::lock_guard<boost::mutex> lock(mutex);

	for (auto it = data.begin(); it != data.end(); ++it)
	{
		free_pkt(it->second);
	}

	data.clear();
}

bool PacketQueue::upper_bound(int64_t dts, AVPacket& pkt, int k, int64_t& next_k_pkt_dts)
{		
	boost::lock_guard<boost::mutex> lock(mutex);

	auto it = data.upper_bound(dts);
	if (it == data.end())
	{
		return false;
	}
	else
	{
		pkt = *(it->second);
		return true;
	}
}*/

/*
#include <SDL.h>
#include <SDL_thread.h>

static AVPacket eof_pkt = {0,0,(uint8_t*)"eof",0};

static AVPacket * get_eof_packet_local()
{
	return &eof_pkt;
}

static bool packet_is_eof_local(AVPacket *pkt)
{
	return pkt->data == eof_pkt.data;
}

static int packet_queue_put_front(PacketQueue *q, AVPacket *pkt)
{
    AVPacketList *pkt1;

    /* duplicate the packet * /
    if (!packet_is_eof_local(pkt) && av_dup_packet(pkt) < 0)
        return -1;

    pkt1 = (AVPacketList*)av_malloc(sizeof(AVPacketList));
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;

    SDL_LockMutex(q->mutex);
    if (!q->last_pkt)
	{
        q->first_pkt = pkt1;
		q->last_pkt = pkt1;
	}
    else
	{
		pkt1->next = q->first_pkt;
		q->first_pkt = pkt1;
	}
    q->nb_packets++;
    q->size_ += pkt1->pkt.size + sizeof(*pkt1);
    SDL_UnlockMutex(q->mutex);
}

static int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
    AVPacketList *pkt1;

    /* duplicate the packet * /
    if (!packet_is_eof_local(pkt) && av_dup_packet(pkt) < 0)
        return -1;

    pkt1 = (AVPacketList*)av_malloc(sizeof(AVPacketList));
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;

    SDL_LockMutex(q->mutex);
    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size_ += pkt1->pkt.size + sizeof(*pkt1);
    /* XXX: should duplicate packet data in DV case * /
    SDL_UnlockMutex(q->mutex);
    return 0;
}

/* packet queue handling * /
static void packet_queue_init(PacketQueue *q)
{
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
}

static bool packet_queue_get(PacketQueue *q, AVPacket *pkt)
{
    AVPacketList *pkt1;
    int ret;
    SDL_LockMutex(q->mutex);
    pkt1 = q->first_pkt;
    if (pkt1)
	{
        q->first_pkt = pkt1->next;
        if (!q->first_pkt)
            q->last_pkt = NULL;
        q->nb_packets--;
        q->size_ -= pkt1->pkt.size + sizeof(*pkt1);
        *pkt = pkt1->pkt;
        av_free(pkt1);
        ret = true;
    }
	else
	{
		ret = false;
	}
    SDL_UnlockMutex(q->mutex);
    return ret;
}

static void packet_queue_flush(PacketQueue *q)
{
    AVPacketList *pkt, *pkt1;

    SDL_LockMutex(q->mutex);
    for (pkt = q->first_pkt; pkt != NULL; pkt = pkt1)
	{
        pkt1 = pkt->next;
        av_free_packet(&pkt->pkt);
        av_freep(&pkt);
    }
    q->last_pkt = NULL;
    q->first_pkt = NULL;
    q->nb_packets = 0;
    q->size_ = 0;
    SDL_UnlockMutex(q->mutex);
}

static int packet_queue_flush_until(PacketQueue *q,int64_t dts,int64_t & return_dts)
{
    AVPacket pkt;
    int ret;
    SDL_LockMutex(q->mutex);
	if( q->first_pkt )
	{
		if( q->first_pkt->pkt.dts >= dts )
		{
			packet_queue_flush(q);
		}
	}
	while(1)
	{
		ret = packet_queue_get(q,&pkt);
		if( ret == -1 )
		{
			ret = -1;
			break;
		}
		else if( ret == 0 )
		{
			packet_queue_flush(q);
			break;
		}
		else
		{
			int64_t this_dts = pkt.dts;
			bool is_key = pkt.flags & AV_PKT_FLAG_KEY;
			if( pkt.size > 0 && pkt.data && is_key && this_dts >= dts )
			{
				return_dts = this_dts;
				packet_queue_put_front(q,&pkt);
				break;
			}
			if( pkt.data )
				av_free_packet(&pkt);
		}
	}
	ret = q->nb_packets;
	// Dont conditional, cause this is a unnormal insertation.
	SDL_UnlockMutex(q->mutex);
	return ret;
}

static void packet_queue_end(PacketQueue *q)
{
    packet_queue_flush(q);
    SDL_DestroyMutex(q->mutex);
}
/////////////////



PacketQueue::PacketQueue()
{
	packet_queue_init(this);
}

PacketQueue::~PacketQueue()
{
	packet_queue_end(this);
}

void PacketQueue::lock()
{
	SDL_LockMutex(mutex);
}

void PacketQueue::unlock()
{
	SDL_UnlockMutex(mutex);
}

int PacketQueue::push_front(AVPacket *pkt)
{
	return packet_queue_put_front(this,pkt);
}

int PacketQueue::push_back(AVPacket *pkt)
{
	return packet_queue_put(this,pkt);
}

bool PacketQueue::pop_front(AVPacket *pkt)
{
	return packet_queue_get(this,pkt);
}

void PacketQueue::flush()
{
	packet_queue_flush(this);
}

int PacketQueue::flush_until(int64_t dts,int64_t & return_dts)
{
	return packet_queue_flush_until(this,dts,return_dts);
}

std::size_t PacketQueue::size() const
{
	return size_;
}

AVPacket * PacketQueue::get_eof_packet()
{
	return get_eof_packet_local();
}

bool PacketQueue::is_eof_packet(AVPacket *pkt)
{
	return packet_is_eof_local(pkt);
}
*/