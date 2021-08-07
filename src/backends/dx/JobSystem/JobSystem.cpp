#include <JobSystem/JobSystem.h>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <JobSystem/JobBucket.h>
#include <JobSystem/JobNode.h>
#include <util/Memory.h>
void JobSystem::UpdateNewBucket() {
	JobBucket* bucket;
	while (true) {
		if (currentBucketPos >= buckets.size()) {
			{
				lockGuard lck(mainThreadWaitMutex);
				mainThreadFinished = true;
				mainThreadWaitCV.notify_all();
			}
			return;
		}
		bucket = buckets[currentBucketPos];
		if (bucket->allJobNodes.empty() || bucket->sys != this) {
			currentBucketPos++;
			continue;
		}
		break;
	}
	bucketMissionCount = bucket->allJobNodes.size();
	for (auto& node : bucket->executeJobs) {
		executingNode.Push(node);
	}
	bucket->executeJobs.clear();
	bucket->allJobNodes.clear();
	currentBucketPos++;
	size_t size = executingNode.Length();
	if (size < mThreadCount) {
		lockGuard lck(threadMtx);
		for (int64_t i = 0; i < size; ++i) {
			cv.notify_one();
		}
	} else {
		lockGuard lck(threadMtx);
		cv.notify_all();
	}
}
class JobThreadRunnable {
public:
	JobSystem* sys;
	/*bool* JobSystemInitialized;
	std::condition_variable* cv;
	ConcurrentQueue<JobNode*>* executingNode;
	std::atomic<int64>* bucketMissionCount;*/
	void operator()() {
		{
			std::unique_lock<std::mutex> lck(sys->threadMtx);
			while (!sys->jobSystemStart && sys->JobSystemInitialized) {
				sys->cv.wait(lck);
			}
		}
		int64 value = (int64)-1;
		while ([&]() {
			JobNode* node = nullptr;
			while (sys->executingNode.Pop(&node)) {
				/////////// Get Next Node
				while ([&]() {
					JobNode* nextNode = node->Execute(sys->executingNode, sys->cv);
					sys->jobNodePool.Delete(node);
					value = --sys->bucketMissionCount;
					if (nextNode != nullptr) {
						node = nextNode;
						return true;
					}
					/////////// Finish Get Next Node
					return false;
				}()) {}
				if (value == 0) {
					sys->UpdateNewBucket();
				}
			}
			std::unique_lock<std::mutex> lck(sys->threadMtx);
			while (sys->JobSystemInitialized) {
				sys->cv.wait(lck);
				return true;
			}
			return false;
		}()) {}
	}
};
JobBucket* JobSystem::GetJobBucket() {
	if (releasedBuckets.empty()) {
		JobBucket* bucket = new JobBucket(this);
		usedBuckets.push_back(bucket);
		return bucket;
	} else {
		auto ite = releasedBuckets.end() - 1;
		JobBucket* cur = *ite;
		cur->executeJobs.clear();
		cur->allJobNodes.clear();
		releasedBuckets.erase(ite);
		return cur;
	}
}
void JobSystem::ReleaseJobBucket(JobBucket* node) {
	node->executeJobs.clear();
	node->allJobNodes.clear();
	releasedBuckets.push_back(node);
}
JobSystem::JobSystem(size_t threadCount) noexcept
	: executingNode(100),
	  mainThreadFinished(true),
	  jobNodePool(50) {
	/*allocatedMemory[0].reserve(50);
	allocatedMemory[1].reserve(50);*/
	mThreadCount = threadCount;
	usedBuckets.reserve(20);
	releasedBuckets.reserve(20);
	allThreads.resize(threadCount);
	for (size_t i = 0; i < threadCount; ++i) {
		JobThreadRunnable j;
		j.sys = this;
		allThreads[i] = new std::thread(j);
	}
}
JobSystem::~JobSystem() noexcept {
	JobSystemInitialized = false;
	{
		lockGuard lck(threadMtx);
		cv.notify_all();
	}
	for (size_t i = 0; i < allThreads.size(); ++i) {
		allThreads[i]->join();
		delete allThreads[i];
	}
	for (auto ite = usedBuckets.begin(); ite != usedBuckets.end(); ++ite) {
		delete *ite;
	}
}
/*
void* JobSystem::AllocFuncMemory(uint64_t size)
{
	void* ptr = vengine_malloc(size);
	allocatedMemory[allocatorSwitcher].push_back(ptr);
	return ptr;
}
void JobSystem::FreeAllMemory()
{
	for (size_t i = 0; i < allocatedMemory[allocatorSwitcher].size(); ++i)
	{
		vengine_free(allocatedMemory[allocatorSwitcher][i]);
	}
	allocatedMemory[allocatorSwitcher].clear();
}
*/
void JobSystem::ExecuteBucket(JobBucket** bucket, size_t bucketCount) {
	jobNodePool.UpdateSwitcher();
	currentBucketPos = 0;
	buckets.resize(bucketCount);
	memcpy(buckets.data(), bucket, sizeof(JobBucket*) * bucketCount);
	mainThreadFinished = false;
	jobSystemStart = true;
	UpdateNewBucket();
	//FreeAllMemory();
}
void JobSystem::ExecuteBucket(JobBucket* bucket, size_t bucketCount) {
	jobNodePool.UpdateSwitcher();
	currentBucketPos = 0;
	buckets.resize(bucketCount);
	for (size_t i = 0; i < bucketCount; ++i) {
		buckets[i] = bucket + i;
	}
	mainThreadFinished = false;
	jobSystemStart = true;
	UpdateNewBucket();
	//FreeAllMemory();
}
void JobSystem::Wait() {
	std::unique_lock<std::mutex> lck(mainThreadWaitMutex);
	while (!mainThreadFinished) {
		mainThreadWaitCV.wait(lck);
	}
}
