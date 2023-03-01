#pragma once

#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <vector>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap
{
private:
	struct Bucket
	{
		std::mutex bucket_mutex;
		std::map<Key, Value> bucket_map;
	};

public:
	static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

	struct Access
	{
		std::lock_guard<std::mutex> g;
		Value& ref_to_value;

		Access(const Key& key, Bucket& bucket) : g(bucket.bucket_mutex), ref_to_value(bucket.bucket_map[key]) {}
	};

	explicit ConcurrentMap(size_t bucket_count) : main_map_(bucket_count) {}

	Access operator[](const Key& key)
	{
		auto& bucket = main_map_[static_cast<uint64_t>(key) % main_map_.size()];
		return { key, bucket };
	}

	std::map<Key, Value> BuildOrdinaryMap()
	{
		std::map<Key, Value> result;
		for (auto& [mutex, map] : main_map_)
		{
			std::lock_guard g(mutex);
			result.insert(map.begin(), map.end());
		}
		return result;
	}

private:
	std::vector<Bucket> main_map_;

};