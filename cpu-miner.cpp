/*
 * Copyright 2010 Jeff Garzik
 * Copyright 2012-2014 pooler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.  See COPYING for more details.
 */

#include "cpuminer-config.h"
#define _GNU_SOURCE

#include "core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#ifdef WIN32

#include <windows.h>
#else
#include <errno.h>
#include <signal.h>
#include <sys/resource.h>
#if HAVE_SYS_SYSCTL_H
#include <sys/types.h>
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <sys/sysctl.h>
#endif
#endif
#include <jansson.h>
#include <curl/curl.h>
#include <openssl/sha.h>
#include "compat.h"
#include "miner.h"

#ifdef WIN32
#include <Mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

#define PROGRAM_NAME		"skminer"

// from heavy.cu
#ifdef __cplusplus
extern "C"
{
#endif
int cuda_num_devices();
void cuda_devicenames();
int cuda_finddevice(char *name);
#ifdef __cplusplus
}
#endif


static bool submit_old = false;
bool use_syslog = false;
static bool opt_background = false;
static bool opt_quiet = false;
static int opt_retries = -1;
static int opt_fail_pause = 30;
int opt_timeout = 270;
static int opt_scantime = 5;
static json_t *opt_config;
static const bool opt_time = true;
//static sha256_algos opt_algo = ALGO_HEAVY;
static int opt_n_threads = 0;
static double opt_difficulty = 1; // CH
int device_map[8] = { 0, 1, 2, 3, 4, 5, 6, 7 }; // CB
static int num_processors;
int tp_coef[8];


#ifndef WIN32
static void signal_handler(int sig)
{
	switch (sig) {
	case SIGHUP:
		applog(LOG_INFO, "SIGHUP received");
		break;
	case SIGINT:
		applog(LOG_INFO, "SIGINT received, exiting");
		exit(0);
		break;
	case SIGTERM:
		applog(LOG_INFO, "SIGTERM received, exiting");
		exit(0);
		break;
	}
}
#endif

#define PROGRAM_VERSION "v0.1"
int main(int argc, char *argv[])
{
	struct thr_info *thr;
	long flags;
//	int i;

#ifdef WIN32
	SYSTEM_INFO sysinfo;
#endif

	 printf("        ***** skMiner for nVidia GPUs by djm34  *****\n");
	 printf("\t             This is version "PROGRAM_VERSION" \n");
	 printf("	based on ccMiner by Christian Buchner and Christian H. 2014 ***\n");
	 printf("                   and on primeminer by Videlicet\n");
	 printf("\t Copyright 2014 djm34\n");
	 printf("\t  BTC donation address: 1NENYmxwZGHsKFmyjTc5WferTn5VTFb7Ze\n");
	

	 if (argc < 3)
	 {
		 printf("Too Few Arguments. The Required Arguments are Ip and Port\n");
		 printf("Default Arguments are Total Threads = CPU Cores and Connection Timeout = 10 Seconds\n");
		 printf("Format for Arguments is 'IP PORT THREADS TIMEOUT'\n");

		 Sleep(10000);

		 return 0;
	 }

	num_processors = cuda_num_devices();
	std::string IP = argv[1];
	std::string PORT = argv[2];
	int nThreads = num_processors;
	bool bBenchmark = false;

	if (argc > 3)
		nThreads = boost::lexical_cast<int>(argv[3]);

	int nTimeout = 10;
	if (argc > 4)
		nTimeout = boost::lexical_cast<int>(argv[4]);
	int nThroughput = 512 * 512;
	int nThroughputMultiplier = 8;
	if (argc > 5)
		nThroughputMultiplier = boost::lexical_cast<int>(argv[5]);

	if (argc > 6)
		bBenchmark = boost::lexical_cast<bool>(argv[6]);

	nThroughput *= nThroughputMultiplier;
	cuda_devicenames();

#ifdef _DEBUG
	nThreads = 1;
#endif

	printf("Initializing Miner %s:%s Threads = %i Timeout = %i | Throughput = %i\n", IP.c_str(), PORT.c_str(), nThreads, nTimeout, nThroughput);
	Core::ServerConnection MINERS(IP, PORT, nThreads, nTimeout, nThroughput, bBenchmark);
	loop{ Sleep(100); }


}
