#pragma once

#include <deque>
#include <map>
#include <boost/thread/mutex.hpp>

class AVPacketLess
{
public:
	bool operator () (int64_t dts, struct AVPacket* pkt);
	bool operator () (struct AVPacket* pkt, int64_t dts);
};

typedef std::deque<struct AVPacket*> PacketBlock;

class PacketBlockLess
{
public:
	bool operator () (int64_t dts, PacketBlock* pb);
	bool operator () (PacketBlock* pb, int64_t dts);
};

#define DEFAULT_BLOCK_SIZE (5000)

class PacketQueue
{
protected:
	unsigned int m_block_size;

	std::deque<PacketBlock*> blocks;
	boost::mutex mutex;

	int64_t size_;

	inline void free_pkt(AVPacket *pkt);

public:
	static AVPacket& get_eof_packet();
	static bool is_eof_packet(const AVPacket& pkt);

	PacketQueue(unsigned int BlockSize = DEFAULT_BLOCK_SIZE) : m_block_size(BlockSize), size_(0)
	{
	}

	~PacketQueue()
	{
		clear();
	}

	bool insert(const AVPacket& pkt); //返回true表示插入成功，返回false表示有重复，但也覆盖了
	bool pop_front();
	void clear();
	bool upper_bound(int64_t dts, AVPacket& pkt, int k, int64_t& next_k_pkt_dts);
	int64_t size() const;
};

/*
//std::map版本
class PacketQueue
{
protected:
	std::map<int64_t, struct AVPacket*> data;
	boost::mutex mutex;

	int64_t size_;

	inline void free_pkt(AVPacket *pkt);

public:
	static AVPacket& get_eof_packet();
	static bool is_eof_packet(const AVPacket& pkt);

	PacketQueue() : size_(0)
	{
	}

	bool insert(const AVPacket& pkt); //返回true表示插入成功，返回false表示有重复，但也覆盖了
	bool pop_front();
	void clear();
	bool upper_bound(int64_t dts, AVPacket& pkt, int k, int64_t& next_k_pkt_dts);

	int64_t size() const;
};*/
/*
#include <inttypes.h>
#include <utility>

struct AVPacket;
struct AVPacketList;
struct SDL_mutex;

// thread-safe except ctor & deconstructor
class PacketQueue
{
public:
    AVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    std::size_t size_;
    SDL_mutex *mutex;

public:
	PacketQueue();
	~PacketQueue();

	void lock();

	void unlock();

	int push_front(AVPacket *pkt);

	int push_back(AVPacket *pkt);

	bool pop_front(AVPacket *pkt);

	void flush();

	int flush_until(int64_t dts,int64_t & return_dts);

	std::size_t size() const;

public:
	static AVPacket * get_eof_packet();

	static bool is_eof_packet(AVPacket *pkt);
};
*/