#include "shrinkler_compress.h"
#include <stdint.h>
#include <string.h>

#include "shrinkler/DataFile.h"

extern "C" 
uint8_t *shrinkler_compress(const uint8_t *input, int dec_size, int level, int *cmp_size, int* inplace_margin)
{
    int references = 100000;
	PackParams params;
	params.parity_context = 0x1;
	params.iterations = level;
	params.length_margin = level;
	params.skip_length = level*1000;
	params.match_patience = level*100;
	params.max_same_length = level*10;

	DataFile *orig = new DataFile;
	orig->data.resize(dec_size);
    memcpy(&orig->data[0], input, dec_size);

	RefEdgeFactory edge_factory(references);
	DataFile *crunched = orig->crunch(&params, &edge_factory, false);
	delete orig;

    *cmp_size = crunched->data.size();
	*inplace_margin = crunched->header.safety_margin;

    uint8_t *cmp = (uint8_t *)malloc(*cmp_size);
    memcpy(cmp, &crunched->data[0], *cmp_size);
    delete crunched;

    return cmp;
}
