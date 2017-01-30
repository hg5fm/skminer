#include "cpuminer-config.h"
#define _GNU_SOURCE

#include "core.h"
#include "hash/templates.h"
#include "hash/CBlock.h"
#include "hash/Miner.h"
#include "hash/MinerThread.h"

unsigned int nBlocksFoundCounter = 0;
unsigned int nBlocksAccepted = 0;
unsigned int nBlocksRejected = 0;
time_t nStartTimer;

char *device_name[8]; // CB
namespace Core
{
	ServerConnection::ServerConnection(std::string ip, std::string port, int nMaxThreads, int nMaxTimeout, int throughput, bool benchmark) 
		: IP(ip), PORT(port), TIMER(), nThreads(nMaxThreads), nTimeout(nMaxTimeout), nThroughput(throughput), bBenchmark(benchmark), THREAD(boost::bind(&ServerConnection::ServerThread, this))
	{
		printf("GPU:\n");
		for (size_t i = 0; i < 8; i++)
		{
			if (device_name[i])
				printf("%i: %s\n", i+1, device_name[i]);
		}
		time(&nStartTimer);
		StartTimer.Start();
		//nStartTimer = (unsigned int)time(0);

		for (int nIndex = 0; nIndex < nThreads; nIndex++)
		{
			MinerThread * minerThread = new MinerThread(nIndex);
			minerThread->SetThroughput(throughput);
			minerThread->SetIsBenchmark(benchmark);			
			THREADS.push_back(minerThread);
		}
	}

	/** Reset the block on each of the Threads. **/
	void ServerConnection::ResetThreads()
	{

		/** Reset each individual flag to tell threads to stop mining. **/
		for (unsigned int nIndex = 0; nIndex < THREADS.size(); nIndex++)
		{
			THREADS[nIndex]->SetIsBlockFound(false);
			THREADS[nIndex]->SetIsNewBlock(true);
		}

	}

	/** Get the total Primes Found from Each Mining Thread.
	Then reset their counter. **/
	unsigned long long ServerConnection::Hashes()
	{
		unsigned long long nHashes = 0;
		for (unsigned int nIndex = 0; nIndex < THREADS.size(); nIndex++)
		{
			nHashes += THREADS[nIndex]->GetHashes();
			THREADS[nIndex]->SetHashes(0);
		}

		return nHashes;
	}


	double ServerConnection::GetDifficulty(unsigned int nBits, int nChannel)
	{
		/** Prime Channel is just Decimal Held in Integer
		Multiplied and Divided by Significant Digits. **/
		if (nChannel == 1)
			return nBits / 10000000.0;

		/** Get the Proportion of the Bits First. **/
		auto dDiff =
			static_cast<double>(0x0000ffff) / static_cast<double>(nBits & 0x00ffffff);

		/** Calculate where on Compact Scale Difficulty is. **/
		int nShift = nBits >> 24;

		/** Shift down if Position on Compact Scale is above 124. **/
		while (nShift > 124)
		{
			dDiff = dDiff / 256.0;
			nShift--;
		}

		/** Shift up if Position on Compact Scale is below 124. **/
		while (nShift < 124)
		{
			dDiff = dDiff * 256.0;
			nShift++;
		}

		/** Offset the number by 64 to give larger starting reference. **/
		return dDiff * ((nChannel == 2) ? 64 : 1024 * 1024 * 256);
	}



	/** Main Connection Thread. Handles all the networking to allow
	Mining threads the most performance. **/
	void ServerConnection::ServerThread()
	{
		CBigNum diff1;
		CBigNum tempTarget;
		tempTarget.SetCompact(0x7c09cf79);
		unsigned int tempDiff = 652356296;
		diff1 = tempTarget * tempDiff;


		char buffer[128];

		/** Don't begin until all mining threads are Created. **/
		while (THREADS.size() != nThreads)
			Sleep(1000);


		/** Initialize the Server Connection. **/
		CLIENT = new LLP::Miner(IP, PORT);


		/** Initialize a Timer for the Hash Meter. **/
		TIMER.Start();
		unsigned int nTimerWait = 2;

		unsigned int nBestHeight = 0;
		loop
		{
			try
			{
				/** Run this thread at 1 Cycle per Second. **/
				Sleep(10);


				/** Attempt with best efforts to keep the Connection Alive. **/
				if (!CLIENT->Connected() || CLIENT->Errors())
				{
					ResetThreads();
					if (CLIENT != nullptr)
					{
						delete CLIENT;
						CLIENT = new LLP::Miner(IP, PORT);
					}
					if (!CLIENT->Connect())
						continue;
					else
						CLIENT->SetChannel(2);
				}


				/** Check the Block Height. **/
				unsigned int nHeight = CLIENT->GetHeight(nTimeout);
				if (nHeight == 0)
				{
					printf("[MASTER] Failed to Update Height\n");
					CLIENT->Disconnect();
					continue;
				}

				/** If there is a new block, Flag the Threads to Stop Mining. **/
				if (nHeight != nBestHeight)
				{
					nBestHeight = nHeight;
					printf("[MASTER] Coinshield Network: New Block %u\n", nHeight);

					ResetThreads();
				}

				/** Rudimentary Meter **/
				if (TIMER.Elapsed() > nTimerWait)
				{
					unsigned int elapsedFromStart =  StartTimer.Elapsed();
					double  Elapsed = (double)TIMER.ElapsedMilliseconds();
					unsigned long long nHashes = Hashes();
					double nMHashps = ((double) nHashes/1000.0) / (double)TIMER.ElapsedMilliseconds();
					if (THREADS[0]->GetBlock() == NULL)
						continue;
					unsigned int nBits = THREADS[0]->GetBlock()->GetBits();

					double diff = GetDifficulty(nBits, 2);
															
					struct tm t;
					__time32_t aclock = elapsedFromStart;
					
					_gmtime32_s(&t, &aclock);
					
					printf("[METERS] %.04f MHash/s | Block/24h %.04f | Blks ACC=%u REJ=%u | Diff=%.08f |  %02d:%02d:%02d:%02d\n", 
						nMHashps, (double)nBlocksFoundCounter / (double)elapsedFromStart * 86400.0, nBlocksAccepted, nBlocksRejected, diff, t.tm_yday, t.tm_hour, t.tm_min, t.tm_sec);

					//printf("[METERS] %.04f MHash/s  |  %.04f Block/h  |  Blks ACC=%u REJ=%u | Diff= %.08f / %lu  \n", 
					//	nMHashps, (double)nBlocksFoundCounter / (double)elapsedFromStart *3600.0, nBlocksAccepted, nBlocksRejected, 1.0 / (double)nDifficulty, diff.getulong());
					if (nMHashps > 5000)
						exit(1);
					TIMER.Reset();
					if (nTimerWait == 2)
						nTimerWait = 10;
				}


				/** Check if there is work to do for each Miner Thread. **/
				for (unsigned int nIndex = 0; nIndex < THREADS.size(); nIndex++)
				{

					/** Attempt to get a new block from the Server if Thread needs One. **/

					if (THREADS[nIndex]->GetIsNewBlock())
					{
						/** Retrieve new block from Server. **/
						/** Delete the Block Pointer if it Exists. **/
						CBlock* pBlock = THREADS[nIndex]->GetBlock();
						if (pBlock == NULL)
						{
							pBlock = new CBlock();
						}

						/** Retrieve new block from Server. **/
						bool getBRes = CLIENT->GetBlock(pBlock, nTimeout);
						if (pBlock == nullptr || getBRes == false)
						{
							CLIENT->Disconnect();
							printf("[MASTER] Failed to get new Block. Reconnecting \r\n");
							break;
						}

						/** If the block is good, tell the Mining Thread its okay to Mine. **/
						if (pBlock)
						{
							THREADS[nIndex]->SetIsBlockFound(false);
							THREADS[nIndex]->SetIsNewBlock(false);
							THREADS[nIndex]->SetBlock(pBlock);
							THREADS[nIndex]->SetHashes(0); // reset hash count

						}

						/** If the Block didn't come in properly, Reconnect to the Server. **/
						else
						{
							CLIENT->Disconnect();

							break;
						}

					}

					/** Submit a block from Mining Thread if Flagged. **/
					else if (THREADS[nIndex]->GetIsBlockFound())
					{

						double  nHashes = THREADS[nIndex]->GetHashes();
						double nMHashps = ((double)nHashes / 1000.0) / (double)TIMER.ElapsedMilliseconds();
						time_t currentTime;
						time(&currentTime);
						nBlocksFoundCounter++;
						if (bBenchmark)
						{
							printf("\n[MASTER] TEST Block Found by \"%s\"		%.1f MHash/s!!!  | %s\n", device_name[THREADS[nIndex]->GetGpuId()], nMHashps, ctime(&currentTime));							
							THREADS[nIndex]->SetIsNewBlock(true);
							THREADS[nIndex]->SetIsBlockFound(false);

							break;
						}
						//							
						/** Attempt to Submit the Block to Network. **/
						unsigned char RESPONSE = CLIENT->SubmitBlock(THREADS[nIndex]->GetBlock()->GetMerkleRoot(), THREADS[nIndex]->GetBlock()->GetNonce(), nTimeout);
						//Hashes();
						//TIMER.Reset();
						if (RESPONSE == 200)
						{
							printf("\n[MASTER] Block Found by \"%s\"		%.1f MHash/s (accepted) Yay !!!  | %s\n", device_name[THREADS[nIndex]->GetGpuId()], nMHashps, ctime(&currentTime));
							nBlocksAccepted++;
							THREADS[nIndex]->SetIsNewBlock(true);
							THREADS[nIndex]->SetIsBlockFound(false);
						}
						else if (RESPONSE == 201)
						{
							nBlocksRejected++;
							printf("\n[MASTER] Block Found by \"%s\"		%.1f MHash/s (rejected) Booo !!!  | %s\n", device_name[THREADS[nIndex]->GetGpuId()],  nMHashps, ctime(&currentTime));
							THREADS[nIndex]->SetIsNewBlock(true);
							THREADS[nIndex]->SetIsBlockFound(false);
						}
						else
						{
							nBlocksRejected++;
							printf("[MASTER] Failure to Submit Block. Reconnecting... %s", ctime(&currentTime));
							CLIENT->Disconnect();
						}

						break;


					}
				}
			}

			catch (...)
			{
				printf("Some Unexpected Exception occured");
			}
		}

	};
}