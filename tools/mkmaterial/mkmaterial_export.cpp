/*
    mkmaterial: convert a MAT INI/JSON file into a binary material database
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.

    For more information, please refer to <http://unlicense.org/>
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "mkmaterial.h"
#include <algorithm>
#include <float.h>
#include "../common/assetcomp.h"
#include "../common/atomic_file.h"
#include "../common/binout.h"
#include "../common/binout.c"
#include "../common/subprocess.h"
#include "../common/utils.h"
#include "../../include/rdpq_macros.h"
#include "../../src/rdpq/rdpq_mat_internal.h"

static uint32_t murmurhash3_32(const void *key, size_t len, uint32_t seed) {
    const uint8_t *data = (const uint8_t*)key;
    uint32_t h = seed, k;
    const uint32_t c1 = 0xcc9e2d51, c2 = 0x1b873593;

    for (size_t i = 0; i + 4 <= len; i += 4) {
        k = (uint32_t)data[i] | (data[i+1]<<8) | (data[i+2]<<16) | (data[i+3]<<24);
        k *= c1; k = (k<<15)|(k>>17); k *= c2;
        h ^= k; h = ((h<<13)|(h>>19))*5+0xe6546b64;
    }

    k = 0;
    switch (len & 3) {
        case 3: k ^= data[len-1]<<16;
        case 2: k ^= data[len-2]<<8;
        case 1: k ^= data[len-3];
                k *= c1; k = (k<<15)|(k>>17); k *= c2; h ^= k;
    }

    h ^= (uint32_t)len;
    h ^= h>>16; h *= 0x85ebca6b;
    h ^= h>>13; h *= 0xc2b2ae35;
    h ^= h>>16;
    return h;
}

void texconvert(Texture &tex)
{
    static char *mksprite = NULL;
    if (!mksprite) asprintf(&mksprite, "%s/bin/mksprite", n64_inst);

    // Read intput file into memory
    verbose("converting %s...\n", tex.name.c_str());
    auto png = slurp(tex.name.c_str());

    // Prepare mksprite command line
    struct subprocess_s subp;
    const char *cmd_addr[32] = {0}; int i = 0;
    cmd_addr[i++] = mksprite;
    cmd_addr[i++] = "--format"; 
    cmd_addr[i++] = tex.fmt.c_str();

    if (tex.mipmap != "NONE") {
        cmd_addr[i++] = "--mipmap";
        cmd_addr[i++] = tex.mipmap.c_str();
    }

    if (tex.dithering != "NONE") {
        cmd_addr[i++] = "--dither";
        cmd_addr[i++] = tex.dithering.c_str();
    }

    char texparms[64];
    snprintf(texparms, sizeof(texparms), "%.2f,%.2f,%d,%d,%.2f,%.2f,%d,%d", 
        tex.s.translate, tex.t.translate, tex.s.scale, tex.t.scale,
        tex.s.repeats, tex.t.repeats, tex.s.mirror, tex.t.mirror);
    cmd_addr[i++] = "--texparms";
    cmd_addr[i++] = texparms;

    cmd_addr[i++] = "--compress";
    cmd_addr[i++] = "0"; // no compression as we need to calculate hash on the bits
    if (flag_verbose >= 2)
        cmd_addr[i++] = "--verbose";

    std::string env1 = "MKSPRITE_INFN="; env1 += tex.name;
    const char *env[] = { env1.c_str(), NULL, };
    
    // Start mksprite
    if (subprocess_create_ex(cmd_addr, subprocess_option_no_window, env, &subp) != 0) {
        fprintf(stderr, "Error: cannot run: %s\n", mksprite);
        exit(1);
    }

    FILE *mksprite_in = subprocess_stdin(&subp);
    fwrite(&png[0], 1, png.size(), mksprite_in);
    fclose(mksprite_in); subp.stdin_file = SUBPROCESS_NULL;

    // Read stdout
    FILE *mksprite_out = subprocess_stdout(&subp);
    std::vector<uint8_t> sprite;
    while (1) {
        uint8_t buf[4096];
        int n = fread(buf, 1, sizeof(buf), mksprite_out);
        if (n == 0) break;
        sprite.insert(sprite.end(), buf, buf + n);
    }    

    // Dump mksprite's stderr. Whatever is printed there (if anything) is useful to see
    forward_to_stderr(subprocess_stderr(&subp), "[mksprite] ");

    // Wait for it to finish
    int retcode;
    subprocess_join(&subp, &retcode);
    if (retcode != 0) {
        fprintf(stderr, "Error: mksprite failed with return code %d\n", retcode);
        exit(1);
    }
    subprocess_destroy(&subp);

    if (retcode != 0) {
        fprintf(stderr, "Error: mksprite failed with return code %d\n", retcode);
        exit(1);
    }

    // Compute murmur32 hash of the sprite
    tex.hash = murmurhash3_32(sprite.data(), sprite.size(), 0x11111111);

    // Create an atomic file to write the sprite
    char *sprite_outfn;
    asprintf(&sprite_outfn, "%s/%08x.sprite", flag_texdb_path, tex.hash);
    verbose("Writing sprite %s...\n", sprite_outfn);

    try {
        AtomicFile af(sprite_outfn);
        if (asset_compress_mem(sprite.data(), sprite.size(), af.stream(), flag_compress, 0, NULL) < 0) {
            throw std::runtime_error("Error compressing sprite data");
        }
        af.commit();
    } catch (const std::exception &e) {
        fprintf(stderr, "Error: %s\n", e.what());
        exit(1);
    }

    free(sprite_outfn);
}

void normalize_combiner(Combiner &cc)
{
    for (size_t ch = 0; ch < 2; ch++) {
        for (auto &uniform : cc.full.channels[ch].uniforms) {
            if (!cc.registers[uniform.id].is_set) continue;
            auto& v = cc.registers[uniform.id].value;
            if (ch == combexpr::RGB) {
                uniform.set({v[0], v[1], v[2]});
            } else if (ch == combexpr::ALPHA) {
                uniform.set(v[3]);
            }
        }
    }
}

void mat_convert(Material &mat)
{
    if (mat.tex[0]) texconvert(mat.tex[0]);
    if (mat.tex[1]) texconvert(mat.tex[1]);
    normalize_combiner(mat.cc);
}

void Material::write(FILE *f)
{
    auto uniforms = cc.full.rdp_uniforms();
    auto has_uni = [&](combexpr::UniformId id) -> bool {
        return uniforms.find(id) != uniforms.end();
    };

    assert(name.size() <= 256);
    w8(f, name.size());
    fwrite(name.c_str(), 1, name.size(), f);

    uint16_t flags = MATFLAG_COMBINER; // always write the combiner
    if (tex[0] || tex[1])       flags |= MATFLAG_TEXTURE;
    if (bl.mode >= 0)           flags |= MATFLAG_BLENDER;
    if (rm.antialias >= 0)      flags |= MATFLAG_RMO_AA;
    if (rm.fog >= 0)            flags |= MATFLAG_RMO_FOG;
    if (rm.dither[0] >= 0)      flags |= MATFLAG_RMO_DITHERING;
    if (rm.dither[1] >= 0)      flags |= MATFLAG_RMO_DITHERING;
    if (rm.filtering >= 0)      flags |= MATFLAG_RMO_FILTERING;
    if (rm.zmode >= 0)          flags |= MATFLAG_RMO_ZMODE;
    if (rm.z_override >= 0)     flags |= MATFLAG_RMO_ZPRIM;
    if (rm.perspective >= 0)    flags |= MATFLAG_RMO_PERSP;
    if (rm.alpha_compare >= 0)  flags |= MATFLAG_RMO_ACMP;
    if (has_uni(combexpr::UNIFORM_K4K5))          flags |= MATFLAG_UNIFORM_K4K5;
    if (has_uni(combexpr::UNIFORM_CHROMAKEY))     flags |= MATFLAG_UNIFORM_CHROMAKEY;
    if (has_uni(combexpr::UNIFORM_PRIM_LOD_FRAC)) flags |= MATFLAG_UNIFORM_PRIMLODFRAC;
    if (has_uni(combexpr::UNIFORM_PRIM))          flags |= MATFLAG_UNIFORM_PRIM;
    if (has_uni(combexpr::UNIFORM_ENV))           flags |= MATFLAG_UNIFORM_ENV;

    w16(f, flags);
    int ext_off_pos = ftell(f);
    w8_placeholderf(f, "%s.ext_offset", name.c_str());
    
    if (flags & MATFLAG_TEXTURE) {
        w32(f, tex[0].hash);
        w32(f, tex[1].hash);
    }
    if (flags & MATFLAG_COMBINER) {
        uint64_t cmd = cc.to_rdpq_mode_arg();
        w64(f, cmd);
    }
    if (flags & MATFLAG_BLENDER) {
        uint32_t cmd = bl.to_rdpq_mode_arg();
        w32(f, cmd);
    }
    if (flags & MATFLAG_RMO_AA) {
        w8(f, rm.antialias);
    }
    if (flags & MATFLAG_RMO_FOG) {
        if (rm.fog.to_str() == "none") {
            w32(f, 0);
        } else if (rm.fog.to_str() == "standard") {
            w32(f, RDPQ_FOG_STANDARD);
        } else {
            assert(0);
        }
    }
    if (flags & MATFLAG_RMO_DITHERING) {
        w8(f, rm.dither[0]);
        w8(f, rm.dither[1]);
    }
    if (flags & MATFLAG_RMO_FILTERING) {
        w8(f, rm.filtering);
    }
    if (flags & MATFLAG_RMO_ZMODE) {
        w8(f, rm.zmode);
    }
    if (flags & MATFLAG_RMO_ZPRIM) {
        w16(f, rm.z_override);
        w16(f, rm.deltaz_override);
    }
    if (flags & MATFLAG_RMO_PERSP) {
        w8(f, rm.perspective);
    }
    if (flags & MATFLAG_RMO_ACMP) {
        w8(f, rm.alpha_compare);
    }
    if (flags & MATFLAG_UNIFORM_K4K5) {
        w16(f, uniforms[combexpr::UNIFORM_K4K5]);
    }
    if (flags & MATFLAG_UNIFORM_CHROMAKEY) {
        w16(f, uniforms[combexpr::UNIFORM_CHROMAKEY]);
    }
    if (flags & MATFLAG_UNIFORM_PRIMLODFRAC) {
        w8(f, uniforms[combexpr::UNIFORM_PRIM_LOD_FRAC]);
    }
    if (flags & MATFLAG_UNIFORM_PRIM) {
        w32(f, uniforms[combexpr::UNIFORM_PRIM]);
    }
    if (flags & MATFLAG_UNIFORM_ENV) {
        w32(f, uniforms[combexpr::UNIFORM_ENV]);
    }
    w8(f, 0xAB); // end of material

    if (ext.size() == 0) {
        placeholder_set_offset(f, 0, "%s.ext_offset", name.c_str());
        return;
    }
    placeholder_set_offset(f, ftell(f) - ext_off_pos, "%s.ext_offset", name.c_str());

    // Sort extensions by hash key
    std::sort(ext.begin(), ext.end(), [](const Extension &a, const Extension &b) {
        return a.hash < b.hash;
    });

    w8(f, ext.size());
    for (const auto &e : ext) {
        w16(f, e.hash);
    }
    for (const auto &e : ext) {
        uint8_t type = e.type;
        if (type == 0 && e.value == "true")
            type |= 0x8;
        w8(f, type);
        w8_placeholderf(f, "%s.offset_%s", name.c_str(), e.name.c_str());
    }
    uint32_t ext_pos = ftell(f);
    for (const auto &e : ext) {
        placeholder_set_offset(f, ftell(f) - ext_pos, "%s.offset_%s", name.c_str(), e.name.c_str());
        if (e.type.to_str() == "bool") {
        } else if (e.type.to_str() == "int") {
            w32(f, parse_int(e.value, INT32_MIN, INT32_MAX));
        } else if (e.type.to_str() == "string") {
            fwrite(e.value.c_str(), 1, e.value.size()+1, f);
        } else if (e.type.to_str() == "float") {
            wf32(f, parse_float(e.value, -FLT_MAX, FLT_MAX));
        } else {
            fprintf(stderr, "Error: unknown extension type '%s' (%d) for extension '%s'\n", e.type.to_str().c_str(), (int)e.type, e.name.c_str());
            exit(1);
        }
    }
}

void mat_writedb(FILE *f, std::vector<Material> &materials)
{
    // Write header
    fwrite("MDB", 1, 3, f);
    w8(f, 1); // version
    w16(f, materials.size()); // num_materials
    w16(f, 0); // flags

    // Write material name length and byte size. This makes for a very
    // compact representation that's very efficient to query linearly at
    // runtime.
    for (auto& mat : materials) {
        w8(f, mat.name.size());
        w8_placeholderf(f, "size.%s", mat.name.c_str());
    }

    // Write materials
    for (auto& mat : materials) {
        int pos = ftell(f);
        mat.write(f);
        int size = ftell(f) - pos;
        placeholder_set_offset(f, size, "size.%s", mat.name.c_str());
    }
}
