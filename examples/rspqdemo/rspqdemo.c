#include <libdragon.h>
#include <string.h>

#include "vec.h"
#include "vector_helper.h"

#define NUM_VECTOR_SLOTS  16
#define NUM_VECTORS       (NUM_VECTOR_SLOTS*2)
#define NUM_MATRICES      4
#define MTX_SLOT          30

vec4_t vectors[NUM_VECTORS];
mtx4x4_t identity, scale, rotation, translation;

vec_slot_t *input_vectors;
vec_slot_t *output_vectors;
vec_mtx_t *matrices;

rspq_block_t *transform_vectors_block;

void print_vectors(vec4_t *v, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++)
    {
        printf("%11.4f  %11.4f  %11.4f  %11.4f\n", v[i].v[0], v[i].v[1], v[i].v[2], v[i].v[3]);
    }
    printf("\n");
}

void print_output(const char* header)
{
    rspq_wait();
    printf("%s\n", header);
    vectors_to_floats(vectors[0].v, output_vectors, NUM_VECTORS*4);
    print_vectors(vectors, NUM_VECTORS);
}

int main()
{
    // Initialize systems
    console_init();
    console_set_debug(true);
	debug_init_isviewer();
	debug_init_usblog();

    // Initialize the "vec" library that this example is based on (see vec.h)
    vec_init();

    // Allocate memory for DMA transfers
    input_vectors = malloc_uncached(sizeof(vec_slot_t)*NUM_VECTOR_SLOTS);
    output_vectors = malloc_uncached(sizeof(vec_slot_t)*NUM_VECTOR_SLOTS);
    matrices = malloc_uncached(sizeof(vec_mtx_t)*NUM_MATRICES);

    memset(input_vectors, 0, sizeof(vec_slot_t)*NUM_VECTOR_SLOTS);
    memset(output_vectors, 0, sizeof(vec_slot_t)*NUM_VECTOR_SLOTS);
    memset(matrices, 0, sizeof(vec_slot_t)*NUM_MATRICES);

    // Initialize input vectors
    for (uint32_t z = 0; z < 2; z++)
    {
        for (uint32_t y = 0; y < 4; y++)
        {
            for (uint32_t x = 0; x < 4; x++)
            {
                vectors[z*4*4 + y*4 + x] = (vec4_t){ .v={
                    x, y, z, 1.f
                }};
            }
        }
    }
    // Convert to fixed point format required by the overlay
    floats_to_vectors(input_vectors, vectors[0].v, NUM_VECTORS*4);

    // Initialize matrices
    matrix_identity(&identity);
    matrix_scale(&scale, 0.5f, 2.0f, 1.1f);
    matrix_rotate_y(&rotation, 4.f);
    matrix_translate(&translation, 0.f, -3.1f, 8.f);

    // Convert to fixed point format required by the overlay
    floats_to_vectors(matrices[0].c, identity.m[0],    16);
    floats_to_vectors(matrices[1].c, scale.m[0],       16);
    floats_to_vectors(matrices[2].c, rotation.m[0],    16);
    floats_to_vectors(matrices[3].c, translation.m[0], 16);

    // This block defines a reusable sequence of commands that could
    // be understood as a "function" that will transform the vectors
    // in slots 0-15 with the matrix in slots 30-31.
    // It is repeatedly called further down to transform an array of vectors with
    // different matrices.
    rspq_block_begin();
    vec_load(0, input_vectors, NUM_VECTOR_SLOTS);
    for (uint32_t i = 0; i < NUM_VECTOR_SLOTS; i++)
    {
        vec_transform(i, MTX_SLOT, i);
    }
    vec_store(output_vectors, 0, NUM_VECTOR_SLOTS);
    transform_vectors_block = rspq_block_end();

    // Print inputs first for reference
    printf("Input vectors:\n");
    print_vectors(vectors, NUM_VECTORS);

    // Scale
    vec_load(MTX_SLOT, matrices[1].c, 2);
    rspq_block_run(transform_vectors_block);
    print_output("Scaled:");

    // Rotate
    vec_load(MTX_SLOT, matrices[2].c, 2);
    rspq_block_run(transform_vectors_block);
    print_output("Rotated:");

    // Translate
    vec_load(MTX_SLOT, matrices[3].c, 2);
    rspq_block_run(transform_vectors_block);
    print_output("Translated:");

    // Typical affine matrix: first scale, then rotate, then translate
    // Load 3 matrices starting at slot 16
    vec_load(16, matrices[1].c, 6);
    // Perform matrix composition by multiplying them together (transforming column vectors)
    // The resulting matrix is written to MTX_SLOT
    vec_transform(22, 18, 16);         // Rotation * scale (first two columns)
    vec_transform(23, 18, 17);         // Rotation * scale (last two columns)
    vec_transform(MTX_SLOT+0, 20, 22); // Translation * rotation * scale (first two columns)
    vec_transform(MTX_SLOT+1, 20, 23); // Translation * rotation * scale (last two columns)
    rspq_block_run(transform_vectors_block);
    print_output("Combined:");

    // Clean up
    rspq_block_free(transform_vectors_block);
    free_uncached(matrices);
    free_uncached(output_vectors);
    free_uncached(input_vectors);

    vec_close();

    return 0;
}
