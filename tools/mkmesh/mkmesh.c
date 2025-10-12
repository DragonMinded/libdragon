#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include "../common/binout.c"
#include "../common/binout.h"
#include "../common/utils.h"
#include "../common/assetcomp.h"

#define CGLTF_IMPLEMENTATION
#include "../common/convert_mesh.h"

#include "../src/magma/mgfx_meshdb_internal.h"

int flag_verbose = 0;

int convert_meshdb(const cgltf_data *data, mgfx_meshdb_t *out_meshdb)
{
    memcpy(out_meshdb->magic, MGFX_MESHDB_MAGIC, MGFX_MESHDB_MAGIC_LEN);
    out_meshdb->version = MGFX_MESHDB_VERSION;
    out_meshdb->mesh_count = data->meshes_count;

    for (size_t i = 0; i < data->meshes_count; i++)
    {
        const cgltf_mesh *in_mesh = &data->meshes[i];
        if (flag_verbose) {
            printf("Converting mesh %s\n", in_mesh->name);
        }
        
        mgfx_mesh_entry_t *entry = &out_meshdb->meshes[i];
        entry->name = in_mesh->name;
        entry->mesh = calloc(1, sizeof(mgfx_mesh_t));

        if (convert_mesh(in_mesh, entry->mesh, flag_verbose) != 0) {
            fprintf(stderr, "Error: failed converting mesh %s\n", in_mesh->name);
            return 1;
        }
    }

    return 0;
}

void meshdb_free(mgfx_meshdb_t *meshdb)
{
    for (size_t i = 0; i < meshdb->mesh_count; i++)
    {
        mgfx_mesh_t *mesh = meshdb->meshes[i].mesh;
        if (mesh == NULL) continue;
        mesh_free(mesh);
        free(mesh);
    }
    free(meshdb);
}

void mesh_write(mgfx_mesh_t *mesh, FILE *out)
{
    w32(out, mesh->submesh_count);
    w32_placeholderf(out, "submeshes");
    placeholder_set(out, "submeshes");

    for (size_t i = 0; i < mesh->submesh_count; i++)
    {
        mgfx_submesh_t *submesh = &mesh->submeshes[i];

        w32(out, submesh->vertex_layout.stride);
        w32(out, submesh->vertex_layout.attribute_count);
        w32_placeholderf(out, "vtx_attributes%d", i);

        w32(out, submesh->input_assembly_parms.primitive_topology);
        w8(out, submesh->input_assembly_parms.primitive_restart_enabled);

        walign(out, sizeof(submesh->vertices_count));
        w32(out, submesh->vertices_count);
        w32(out, submesh->indices_count);
        w32_placeholderf(out, "vertices%d", i);
        if (submesh->indices != NULL) {
            w32_placeholderf(out, "indices%d", i);
        } else {
            w32(out, 0);
        }
    }

    for (size_t i = 0; i < mesh->submesh_count; i++)
    {
        mgfx_submesh_t *submesh = &mesh->submeshes[i];
        walign(out, sizeof(uint32_t));
        placeholder_set(out, "vtx_attributes%d", i);
        for (size_t j = 0; j < submesh->vertex_layout.attribute_count; j++)
        {
            mg_vertex_attribute_t *attr = &submesh->vertex_layout.attributes[j];
            w32(out, attr->input);
            w32(out, attr->offset);
        }
    }

    for (size_t i = 0; i < mesh->submesh_count; i++)
    {
        mgfx_submesh_t *submesh = &mesh->submeshes[i];
        size_t vertices_size = submesh->vertex_layout.stride * submesh->vertices_count;
        walign(out, 8);
        placeholder_set(out, "vertices%d", i);
        fwrite(submesh->vertices, vertices_size, 1, out);
    }

    for (size_t i = 0; i < mesh->submesh_count; i++)
    {
        mgfx_submesh_t *submesh = &mesh->submeshes[i];
        if (submesh->indices == NULL) continue;

        walign(out, 8);
        placeholder_set(out, "indices%d", i);
        for (size_t j = 0; j < submesh->indices_count; j++)
        {
            w16(out, submesh->indices[j]);
        }
    }
}

void meshdb_write(mgfx_meshdb_t *meshdb, FILE *out)
{
    w8(out, meshdb->magic[0]);
    w8(out, meshdb->magic[1]);
    w8(out, meshdb->magic[2]);
    w8(out, meshdb->version);
    w32(out, meshdb->mesh_count);

    for (size_t i = 0; i < meshdb->mesh_count; i++)
    {
        w32_placeholderf(out, "meshname%d", i);
        w32_placeholderf(out, "meshptr%d", i);
    }
    
    for (size_t i = 0; i < meshdb->mesh_count; i++)
    {
        mgfx_mesh_t *mesh = meshdb->meshes[i].mesh;
        walign(out, sizeof(mesh->submesh_count));
        placeholder_set(out, "meshptr%d", i);
        mesh_write(mesh, out);
    }

    for (size_t i = 0; i < meshdb->mesh_count; i++)
    {
        placeholder_set(out, "meshname%d", i);
        fwrite(meshdb->meshes[i].name, 1, strlen(meshdb->meshes[i].name) + 1, out);
    }
}

int convert(const char *infn, const char *outfn)
{
    cgltf_options options = {0};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, infn, &data);
    if (result == cgltf_result_file_not_found) {
        fprintf(stderr, "Error: could not find input file: %s\n", infn);
        return 1;
    }
    if (result != cgltf_result_success) {
        fprintf(stderr, "Error: could not parse input file: %s\n", infn);
        return 1;
    }

    if (cgltf_validate(data) != cgltf_result_success) {
        fprintf(stderr, "Error: validation failed\n");
        cgltf_free(data);
        return 1;
    }

    if (data->asset.generator && strstr(data->asset.generator, "Blender") && strstr(data->asset.generator, "v3.4.50")) {
        fprintf(stderr, "Error: Blender version v3.4.1 has buggy glTF export (vertex colors are wrong).\nPlease upgrade Blender and export the model again.\n");
        cgltf_free(data);
        return 1;
    }

    cgltf_load_buffers(&options, data, infn);

    mgfx_meshdb_t *out_meshdb = calloc(1, sizeof(mgfx_meshdb_t) + sizeof(mgfx_mesh_entry_t) * data->meshes_count);

    if (convert_meshdb(data, out_meshdb) != 0) {
        fprintf(stderr, "Error: failed converting mesh\n");
        meshdb_free(out_meshdb);
        cgltf_free(data);
        return 1;
    }

    FILE *out = fopen(outfn, "wb");
    if (!out) {
        fprintf(stderr, "could not open output file: %s\n", outfn);
        meshdb_free(out_meshdb);
        cgltf_free(data);
        return 1;
    }

    meshdb_write(out_meshdb, out);

    fclose(out);
    meshdb_free(out_meshdb);
    cgltf_free(data);
    return 0;
}

void print_args( char * name )
{
    fprintf(stderr, "mkmesh -- Extract and convert meshes from glTF 2.0 files\n\n");
    fprintf(stderr, "Usage: %s [flags] <input files...>\n", name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Command-line flags:\n");
    fprintf(stderr, "   -o/--output <dir>       Specify output directory (default: .)\n");
    fprintf(stderr, "   -c/--compress <level>   Compress output files (default: %d)\n", DEFAULT_COMPRESSION);
    fprintf(stderr, "   -v/--verbose            Verbose output\n");
    fprintf(stderr, "\n");
}

int main(int argc, char *argv[])
{
    char *infn = NULL, *outdir = ".", *outfn = NULL;
    bool error = false;
    int compression = DEFAULT_COMPRESSION;

    if (argc < 2) {
        print_args(argv[0]);
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
                print_args(argv[0]);
                return 0;
            } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
                flag_verbose++;
            } else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--compress")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                char extra;
                if (sscanf(argv[i], "%d%c", &compression, &extra) != 1) {
                    fprintf(stderr, "invalid argument for %s: %s\n", argv[i-1], argv[i]);
                    return 1;
                }
                if (compression < 0 || compression > MAX_COMPRESSION) {
                    fprintf(stderr, "invalid compression level: %d\n", compression);
                    return 1;
                }
            } else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                outdir = argv[i];
            } else {
                fprintf(stderr, "invalid flag: %s\n", argv[i]);
                return 1;
            }
            continue;
        }

        infn = argv[i];
        char *basename = strrchr(infn, '/');
        if (!basename) basename = infn; else basename += 1;
        char* basename_noext = strdup(basename);
        char* ext = strrchr(basename_noext, '.');
        if (ext) *ext = '\0';

        asprintf(&outfn, "%s/%s.mshdb", outdir, basename_noext);
        if (flag_verbose)
            printf("Converting: %s -> %s\n",
                infn, outfn);
        if (convert(infn, outfn) != 0) {
            error = true;
        }
        if (!error) {
            if (compression) {
                struct stat st_decomp = {0}, st_comp = {0};
                stat(outfn, &st_decomp);
                asset_compress(outfn, outfn, compression, 0);
                stat(outfn, &st_comp);
                if (flag_verbose)
                    printf("compressed: %s (%d -> %d, ratio %.1f%%)\n", outfn,
                    (int)st_decomp.st_size, (int)st_comp.st_size, 100.0 * (float)st_comp.st_size / (float)(st_decomp.st_size == 0 ? 1 :st_decomp.st_size));
            }
        }

        free(outfn);
    }

    return error ? 1 : 0;
}
