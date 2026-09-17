#include "pipelines.h"
#include "gl_constants.h"
#include "gl_internal.h"
#include "utils.h"
#include "../magma/magma_internal.h"

extern gl_state_t *state;

DEFINE_RSP_UCODE(rsp_gl_pipeline);
DEFINE_RSP_UCODE(rsp_gl_pipeline_env);
DEFINE_RSP_UCODE(rsp_gl_pipeline_nrm);
DEFINE_RSP_UCODE(rsp_gl_pipeline_env_nrm);

static rsp_ucode_t *pipeline_ucodes[] = {
    &rsp_gl_pipeline,
    &rsp_gl_pipeline_env,
    &rsp_gl_pipeline_nrm,
    &rsp_gl_pipeline_env_nrm
};

static rsp_ucode_t *get_pipeline_ucode(uint32_t features)
{
    return pipeline_ucodes[features];
}

static mg_pipeline_t **create_pipelines(const vertex_layout_t *layout)
{
    mg_pipeline_t **pipelines = calloc(PIPELINE_COUNT, sizeof(mg_pipeline_t*));

    // This will iterate over all possible combinations of features
    for (size_t i = 0; i < PIPELINE_COUNT; i++)
    {
        pipelines[i] = mg_pipeline_create(&(mg_pipeline_parms_t) {
            .vertex_shader_ucode = get_pipeline_ucode(i),
            .vertex_layout = layout->vertex_layout
        });
    }

    return pipelines;
}

static mg_pipeline_t **get_or_create_pipelines(const vertex_layout_t *layout)
{
    uint32_t key = vertex_layout_get_hash(layout);

    mg_pipeline_t **pipelines = hashtable_lookup(&state->pipeline_cache, key);
    if (pipelines == NULL) {
        pipelines = create_pipelines(layout);
        hashtable_insert(&state->pipeline_cache, key, pipelines);
    }

    return pipelines;
}

static void assign_pipelines(mg_pipeline_t **pipelines)
{
    for (size_t i = 0; i < PIPELINE_COUNT; i++)
    {
        uint32_t packed = ((sizeof(gl_pipeline_data_t)*i) << 16) | (ROUND_UP(pipelines[i]->shader_code_size, 8) - 1);
        gl2_write(GL_CMD_SET_PIPELINE, PhysicalAddr(pipelines[i]->shader_code), packed);
    }
}

void update_pipelines_from_layout(const vertex_layout_t *vertex_layout)
{
    mg_pipeline_t **pipelines = get_or_create_pipelines(vertex_layout);
    assign_pipelines(pipelines);
}

static const mg_uniform_t *get_matrices_uniform_from_pipelines(mg_pipeline_t **pipelines)
{
    return mg_pipeline_get_uniform(pipelines[0], GLP_BINDING_MATRICES);
}

const mg_uniform_t *get_matrices_uniform()
{
    if (state->matrices_uniform == NULL) {
        mg_pipeline_t **pipelines = get_or_create_pipelines(&state->begin_end_layout);
        state->matrices_uniform = get_matrices_uniform_from_pipelines(pipelines);
    }
    return state->matrices_uniform;
}
