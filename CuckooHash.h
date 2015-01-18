// Copyright 1999-2015 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

Cuckoo hash map. Used for mapping offsets to edges in the LZ parser.

*/

#pragma once

#include <utility>
#include <algorithm>
#include <new>

using std::pair;

template <typename V> class CuckooHash;

template <typename V>
class CuckooHashIterator {
	CuckooHash<V>* table;
	int index;
	CuckooHashIterator(CuckooHash<V>* table, int index) : table(table), index(index)
	{}

	void find() {
		while (table->array[index].first == CuckooHash<V>::UNUSED) index++;
	}

	friend class CuckooHash<V>;

public:
	pair<int, V>& operator*() {
		find();
		return table->array[index];
	}

	pair<int, V>* operator->() {
		find();
		return &table->array[index];
	}

	CuckooHashIterator<V> operator++(int) {
		find();
		return CuckooHashIterator<V>(table, index++);
	}

	bool operator!=(const CuckooHashIterator<V>& other) {
		return index != other.index;
	}
};

template <typename V>
class CuckooHash {
public:
	typedef int key_type;
	typedef pair<key_type, V> value_type;
	typedef CuckooHashIterator<V> iterator;
private:
	friend class CuckooHashIterator<V>;

	typedef unsigned hash_type;

	static const key_type UNUSED = 0x80000000;
	static const hash_type HASH1_MUL = 0xF230D3A1;
	static const hash_type HASH2_MUL = 0x8084027F;
	static const int INITIAL_SIZE_LOG = 4;

	value_type* array;
	int n_elements;
	int hash_shift;

	int array_size() {
		return 1 << (sizeof(hash_type) * 8 - hash_shift);
	}

	void init_array() {
		int size = array_size();
		array = new value_type[size];
		for (int i = 0 ; i < size ; i++) {
			array[i].first = UNUSED;
			array[i].second = V();
		}
	}

	void init() {
		n_elements = 0;
		hash_shift = sizeof(hash_type) * 8 - INITIAL_SIZE_LOG;
		init_array();
	}

	void hashes(key_type key, hash_type& hash1, hash_type& hash2) const {
		hash_type f = (key << 1) + 1;
		hash1 = (f * HASH1_MUL) >> hash_shift;
		hash2 = (f * HASH2_MUL) >> hash_shift;
	}

	void rehash() {
		int old_size = array_size();
		value_type* old_array = array;
		hash_shift--;
		init_array();
		for (int i = 0 ; i < old_size ; i++) {
			if (old_array[i].first != UNUSED) {
				(*this)[old_array[i].first] = old_array[i].second;
			}
		}
		delete[] old_array;
	}

	void insert(hash_type hash, int key, V value, int n) {
		while (array[hash].first != UNUSED) {
			if (--n < 0) {
				rehash();
				(*this)[key] = value;
				return;
			}
			std::swap(key, array[hash].first);
			std::swap(value, array[hash].second);
			hash_type hash1;
			hash_type hash2;
			hashes(key, hash1, hash2);
			hash ^= hash1 ^ hash2;
		}
		array[hash].first = key;
		array[hash].second = value;
		n_elements++;
	}

public:
	CuckooHash() {
		init();
	}

	CuckooHash(const CuckooHash& source) {
		// We only use copy for array initialization, so just create an empty map
		init();
	}

	~CuckooHash() {
		delete[] array;
	}

	void clear() {
		delete[] array;
		init();
	}

	iterator begin() {
		return CuckooHashIterator<V>(this, 0);
	}

	iterator end() {
		int index = array_size();
		while (index > 0 && array[index - 1].first == UNUSED) index--;
		return CuckooHashIterator<V>(this, index);
	}

	int size() const {
		return n_elements;
	}

	bool empty() const {
		return size() == 0;
	}

	int count(int key) const {
		hash_type hash1;
		hash_type hash2;
		hashes(key, hash1, hash2);

		if (array[hash1].first == key || array[hash2].first == key) return 1;
		return 0;
	}

	void erase(int key) {
		hash_type hash1;
		hash_type hash2;
		hashes(key, hash1, hash2);

		hash_type hash;
		if (array[hash1].first == key) {
			hash = hash1;
		} else if (array[hash2].first == key) {
			hash = hash2;
		} else {
			return;
		}
		array[hash].first = UNUSED;
		array[hash].second = V();
		n_elements--;
	}

	V& operator[](int key) {
		hash_type hash1;
		hash_type hash2;
		hashes(key, hash1, hash2);

		if (array[hash1].first == key) return array[hash1].second;
		if (array[hash2].first == key) return array[hash2].second;
		if (array[hash1].first == UNUSED) {
			array[hash1].first = key;
			array[hash1].second = V();
			n_elements++;
			return array[hash1].second;
		}
		if (array[hash2].first == UNUSED) {
			array[hash2].first = key;
			array[hash2].second = V();
			n_elements++;
			return array[hash2].second;
		}
		insert(hash1, key, V(), 42);
		return (*this)[key];
	}
};

