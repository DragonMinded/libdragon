// Copyright 1999-2019 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

Parse a data block into LZ symbols (literal bytes and references).

The parser uses a "local optimal parse" strategy, where all matches reported
by the match finder are considered. Potential parses are maintained for each
possible previous reference offset, in order to maximize the utilization of
the "repeated offset" feature of the LZ encoding.

Three parameters control the speed/precision tradeoff of the parser:

The length_margin parameter how many shorter matches the parser will consider
for each match reported by the match finder. If the match finder reports a
match of length l, the parser will consider all (valid) matches of length at
least l-length_margin.

The skip_length parameter controls a shortcutting mechanism for very long
matches. Whenever a match of length at least skip_length is reported, the
parser will use that match unconditionally and skip ahead to continue the
parsing at the end of the match.

The max_edges parameter controls the total number of reference edges the
parser will keep around for representing potential parses. Whenever the
limit is reached, the parser will delete the least favorable of the current
parses to free up space.

*/

#pragma once

#include <vector>
#include <map>
#include <set>
#include <functional>
#include <utility>
#include <list>
#include <algorithm>

using std::map;
using std::max;
using std::min;
using std::pair;
using std::sort;
using std::vector;

#include "LZEncoder.h"
#include "MatchFinder.h"
#include "Heap.h"
#include "CuckooHash.h"
#include "assert.h"

// For each offset:
//   Best total size with last ref having that offset

class RefEdge {
	int pos;
	int offset;
	int length;
	int total_size;
	int refcount;
	RefEdge *source;

	RefEdge(int pos, int offset, int length, int total_size, RefEdge *source)
		: pos(pos), offset(offset), length(length), total_size(total_size), source(source)
	{
		assert(source != this);
		refcount = 1;
		if (source != NULL) {
			source->refcount++;
		}
		_heap_index = 0;
	}

	int target() {
		return pos + length;
	}

	friend class RefEdgeFactory;
	friend class LZParser;
	friend struct LZResultEdge;
	friend class LZParseResult;
	friend struct std::less<RefEdge*>;

public:
	int _heap_index;
};

namespace std {
	template <> struct less<RefEdge*> {
		bool operator()(RefEdge* const & e1, RefEdge* const & e2) const {
			return e1->total_size < e2->total_size;
		}
	};
}

// Factory for RefEdge objects which recycles destroyed objects for efficiency
class RefEdgeFactory {
	int edge_capacity;
	int edge_count;
	int cleaned_edges;

	RefEdge* buffer;
public:
	int max_edge_count;
	int max_cleaned_edges;

	RefEdgeFactory(int edge_capacity) : edge_capacity(edge_capacity),
		edge_count(0), cleaned_edges(0), max_edge_count(0), max_cleaned_edges(0)
	{
		buffer = NULL;
	}

	~RefEdgeFactory() {
		while (buffer != NULL) {
			RefEdge *edge = buffer;
			buffer = buffer->source;
			delete edge;
		}
	}

	void reset() {
		assert(edge_count == 0);
		cleaned_edges = 0;
	}

	RefEdge* create(int pos, int offset, int length, int total_size, RefEdge *source) {
		max_edge_count = max(max_edge_count, ++edge_count);
		if (buffer == NULL) {
			return new RefEdge(pos, offset, length, total_size, source);
		} else {
			RefEdge* edge = buffer;
			buffer = edge->source;
			return new (edge) RefEdge(pos, offset, length, total_size, source);
		}
	}

	void destroy(RefEdge* edge, bool clean) {
		edge->source = buffer;
		buffer = edge;
		edge_count--;
		if (clean) {
			max_cleaned_edges = max(max_cleaned_edges, ++cleaned_edges);
		}
	}

	bool full() {
		return edge_count >= edge_capacity;
	}

};

class LZProgress {
public:
	virtual void begin(int size) = 0;
	virtual void update(int pos) = 0;
	virtual void end() = 0;

	virtual ~LZProgress() {}
};

struct LZResultEdge {
	int pos;
	int offset;
	int length;

	LZResultEdge(RefEdge *edge) : pos(edge->pos), offset(edge->offset), length(edge->length) {}

	friend class LZParseResult;
};

typedef unsigned long long result_size_t;

class LZParseResult {
	vector<LZResultEdge> edges;
	const unsigned char *data;
	int data_length;
	int zero_padding;
public:
	result_size_t encode(const LZEncoder& result_encoder) const {
		result_size_t size = 0;
		int pos = 0;
		LZState state;
		result_encoder.setInitialState(&state);
		for (int i = edges.size() - 1 ; i >= 0 ; i--) {
			const LZResultEdge *edge = &edges[i];
			while (pos < edge->pos) {
				size += result_encoder.encodeLiteral(data[pos++], &state, &state);
			}
			size += result_encoder.encodeReference(edge->offset, edge->length, &state, &state);
			pos += edge->length;
		}
		while (pos < data_length) {
			size += result_encoder.encodeLiteral(data[pos++], &state, &state);
		}
		if (zero_padding > 0) {
			size += result_encoder.encodeLiteral(0, &state, &state);
			if (zero_padding == 2) {
				size += result_encoder.encodeLiteral(0, &state, &state);
			} else if (zero_padding > 1) {
				size += result_encoder.encodeReference(1, zero_padding - 1, &state, &state);
			}
		}
		size += result_encoder.finish(&state);
		return size;
	}

	friend class LZParser;
};

class LZParser {
	const unsigned char *data;
	int data_length;
	int zero_padding;
	MatchFinder& finder;
	int length_margin;
	int skip_length;
	const LZEncoder* encoderp;
	RefEdgeFactory* edge_factory;

	vector<int> literal_size;
	vector<CuckooHash<RefEdge*> > edges_to_pos;
	RefEdge* best;
	CuckooHash<RefEdge*> best_for_offset;
	Heap<RefEdge*> root_edges;

	bool is_root(RefEdge *edge) {
		return root_edges.contains(edge);
	}

	void remove_root(RefEdge *edge) {
		root_edges.remove(edge);
	}

	void releaseEdge(RefEdge *edge, bool clean = false) {
		while (edge != NULL) {
			RefEdge *source = edge->source;
			if (--edge->refcount == 0) {
				assert(!is_root(edge));
				edge_factory->destroy(edge, clean);
			} else {
				return;
			}
			edge = source;
		}
	}

	// Return progress
	bool clean_worst_edge(int pos, RefEdge *exclude) {
		if (root_edges.size() == 0) return false;
		RefEdge *worst_edge = root_edges.remove_largest();
		if (worst_edge == best || worst_edge == exclude) return true;
		CuckooHash<RefEdge*>& container = worst_edge->target() > pos
			? edges_to_pos[worst_edge->target()]
			: best_for_offset;
		if (container.size() > 1 && container.count(worst_edge->offset) > 0) {
			container.erase(worst_edge->offset);
			releaseEdge(worst_edge, true);
		}
		return true;
	}

	void put_by_offset(CuckooHash<RefEdge*>& by_offset, RefEdge* edge) {
		assert(!is_root(edge));
		if (by_offset.count(edge->offset) == 0) {
			by_offset[edge->offset] = edge;
			root_edges.insert(edge);
		} else if (edge->total_size < by_offset[edge->offset]->total_size) {
			RefEdge* old_edge = by_offset[edge->offset];
			remove_root(old_edge);
			releaseEdge(old_edge);
			by_offset[edge->offset] = edge;
			root_edges.insert(edge);
		} else {
			releaseEdge(edge);
		}
	}

	void newEdge(RefEdge *source, int pos, int offset, int length) {
		if (source && offset == source->offset && pos == source->target()) return;
		int prev_target = source ? source->target() : 0;
		int new_target = pos + length;
		LZState state_before;
		LZState state_after;
		encoderp->constructState(&state_before, pos, pos == prev_target, source ? source->offset : 0);
		int size_before = (source ? source->total_size : literal_size[data_length]) - (literal_size[data_length] - literal_size[pos]);
		int edge_size = encoderp->encodeReference(offset, length, &state_before, &state_after);
		int size_after = literal_size[data_length] - literal_size[new_target];
		while (edge_factory->full()) {
			if (!clean_worst_edge(pos, source)) break;
		}
		RefEdge *new_edge = edge_factory->create(pos, offset, length, size_before + edge_size + size_after, source);
		put_by_offset(edges_to_pos[new_target], new_edge);
	}

public:
	LZParser(const unsigned char *data, int data_length, int zero_padding, MatchFinder& finder, int length_margin, int skip_length, RefEdgeFactory* edge_factory)
		: data(data), data_length(data_length), zero_padding(zero_padding), finder(finder), length_margin(length_margin), skip_length(skip_length), edge_factory(edge_factory)
	{
		// Initialize edges_to_pos array
		edges_to_pos.resize(data_length + 1);
		best = NULL;
	}

	LZParseResult parse(const LZEncoder& encoder, LZProgress *progress) {
		progress->begin(data_length);
		encoderp = &encoder;

		// Reset state
		best_for_offset.clear();
		root_edges.clear();
		edge_factory->reset();

		// Accumulate literal sizes
		literal_size.resize(data_length + 1, 0);
		int size = 0;
		LZState literal_state;
		encoder.setInitialState(&literal_state);
		for (int i = 0 ; i < data_length ; i++) {
			literal_size[i] = size;
			size += encoder.encodeLiteral(data[i], &literal_state, &literal_state);
		}
		literal_size[data_length] = size;

		// Parse
		RefEdge* initial_best = edge_factory->create(0, 0, 0, literal_size[data_length], NULL);
		best = initial_best;
		for (int pos = 1 ; pos <= data_length ; pos++) {
			// Assimilate edges ending here
			for (CuckooHash<RefEdge*>::iterator it = edges_to_pos[pos].begin() ; it != edges_to_pos[pos].end() ; it++) {
				RefEdge *edge = it->second;
				if (edge->total_size < best->total_size) {
					best = edge;
				}
				remove_root(edge);
				put_by_offset(best_for_offset, edge);
			}
			edges_to_pos[pos].clear();

			// Add new edges according to matches
			finder.beginMatching(pos);
			int match_pos;
			int match_length;
			int max_match_length = 0;
			while (finder.nextMatch(&match_pos, &match_length)) {
				int offset = pos - match_pos;
				if (match_length > data_length - pos) {
					match_length = data_length - pos;
				}
				int min_length = match_length - length_margin;
				if (min_length < 2) min_length = 2;
				for (int length = min_length ; length <= match_length ; length++) {
					newEdge(best, pos, offset, length);
					if (best->offset != offset && best_for_offset.count(offset)) {
						assert(best_for_offset[offset]->target() <= pos);
						newEdge(best_for_offset[offset], pos, offset, length);
					}
				}
				max_match_length = max(max_match_length, match_length);
			}

			// If we have a very long match, skip ahead
			if (max_match_length >= skip_length && !edges_to_pos[pos + max_match_length].empty()) {
				root_edges.clear();
				for (CuckooHash<RefEdge*>::iterator it = best_for_offset.begin() ; it != best_for_offset.end() ; it++) {
					releaseEdge(it->second);
				}
				best_for_offset.clear();
				int target_pos = pos + max_match_length;
				while (pos < target_pos - 1) {
					CuckooHash<RefEdge*>& edges = edges_to_pos[++pos];
					for (CuckooHash<RefEdge*>::iterator it = edges.begin() ; it != edges.end() ; it++) {
						releaseEdge(it->second);
					}
					edges.clear();
				}
				best = initial_best;
			}

			progress->update(pos);
		}

		// Clean unused paths
		root_edges.clear();
		for (CuckooHash<RefEdge*>::iterator it = best_for_offset.begin() ; it != best_for_offset.end() ; it++) {
			RefEdge *edge = it->second;
			if (edge != best) {
				releaseEdge(edge);
			}
		}

		// Find best path
		LZParseResult result;
		result.data = data;
		result.data_length = data_length;
		result.zero_padding = zero_padding;
		RefEdge *edge = best;
		while (edge->length > 0) {
			result.edges.push_back(LZResultEdge(edge));
			edge = edge->source;
		}
		releaseEdge(edge);
		releaseEdge(best);

		progress->end();

		return result;
	}

};
