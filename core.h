#ifndef COINSHIELD_LLP_CORE_H
#define COINSHIELD_LLP_CORE_H


#include "types.h"
#include "bignum.h"
#include "hash/templates.h"
#include "hash/CBlock.h"
#include "hash/Miner.h"
#include "hash/MinerThread.h"


namespace LLP
{
	
	
}

namespace Core
{
	

	class ServerConnection
	{
	public:
		LLP::Miner  *CLIENT;
		int nThreads, nTimeout, nThroughput;
		std::vector<MinerThread*> THREADS;
		LLP::Thread_t THREAD;
		LLP::Timer    TIMER;
		LLP::Timer	StartTimer;
		std::string   IP, PORT;
		bool bBenchmark = false;

		ServerConnection(std::string ip, std::string port, int nMaxThreads, int nMaxTimeout, int throughput = 0, bool benchmark = false);
		void ResetThreads();
		unsigned long long Hashes();
		void ServerThread();
		

	};
}

#endif