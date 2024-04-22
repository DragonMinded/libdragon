// Copyright 1999-2014 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

Heap-based priority queue with removal support.

The element type must have an accessible _heap_index integer field.

*/

#pragma once

#include <vector>
#include <functional>

using std::vector;
using std::less;

template <class T>
class Heap {
	vector<T> elements;
	less<T> compare;

	void swap(int i1, int i2) {
		T t1 = elements[i1];
		T t2 = elements[i2];
		elements[i1] = t2;
		elements[i2] = t1;
		t2->_heap_index = i1;
		t1->_heap_index = i2;
	}

	void up(int i) {
		while (i > 0) {
			int pi = (i-1)/2;
			if (!compare(elements[pi], elements[i])) return;
			swap(i, pi);
			i = pi;
		}
	}

	void down(int i) {
		while (i*2+1 < elements.size()) {
			int ci1 = i*2+1;
			int ci2 = i*2+2;
			int ci = ci2 < elements.size() && compare(elements[ci1], elements[ci2]) ? ci2 : ci1;
			if (!compare(elements[i], elements[ci])) return;
			swap(i, ci);
			i = ci;
		}		
	}

	T remove_index(int i) {
		T removed = elements[i];
		T last = elements[elements.size()-1];
		elements[i] = last;
		elements.pop_back();
		last->_heap_index = i;
		down(i);
		return removed;
	}

public:
	Heap() {}

	void insert(T t) {
		elements.push_back(t);
		t->_heap_index = elements.size()-1;
		up(elements.size()-1);
	}

	void remove(T t) {
		if (contains(t)) {
			remove_index(t->_heap_index);
		}
	}

	T remove_largest() {
		return remove_index(0);
	}

	bool contains(T t) {
		return t->_heap_index < elements.size() && elements[t->_heap_index] == t;
	}

	int size() {
		return elements.size();
	}

	void clear() {
		elements.clear();
	}

};
