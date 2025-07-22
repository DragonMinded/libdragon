
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

    if (tex.mipmap != "none") {
        cmd_addr[i++] = "--mipmap";
        cmd_addr[i++] = tex.mipmap.c_str();
    }

    if (tex.dithering != "none") {
        cmd_addr[i++] = "--dither";
        cmd_addr[i++] = tex.dithering.c_str();
    }

    char texparms[64];
    snprintf(texparms, sizeof(texparms), "%.2f,%.2f,%d,%d,%.2f,%.2f,%d,%d", 
        tex.s.translate, tex.t.translate, tex.s.scale, tex.t.scale,
        tex.s.repeats, tex.t.repeats, tex.s.mirror, tex.t.mirror);
    cmd_addr[i++] = "--texparms";
    cmd_addr[i++] = texparms;

    char compress[2] = { '0' + flag_compress, 0 };
    cmd_addr[i++] = "--compress"; // can't compress on stdin/stout anyway
    cmd_addr[i++] = compress;
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
    assert(tex.sprite.empty());
    while (1) {
        uint8_t buf[4096];
        int n = fread(buf, 1, sizeof(buf), mksprite_out);
        if (n == 0) break;
        tex.sprite.insert(tex.sprite.end(), buf, buf + n);
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
}

void mat_convert(Material &mat)
{
    if (mat.tex[0]) texconvert(mat.tex[0]);
    if (mat.tex[1]) texconvert(mat.tex[1]);
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
    
    if (flags & MATFLAG_TEXTURE) {
        w16(f, 0); // texid FIXME
    }
    if (flags & MATFLAG_COMBINER) {
        uint64_t cmd = cc.to_rdpq_mode_arg();
        w64(f, cmd);
    }
    if (flags & MATFLAG_BLENDER) {
        int mode = bl.mode;
        w8(f, mode);
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
}

static uint32_t prime_hash(const char *str, uint32_t prime)
{
    uint32_t hash = 0;
    while(*str) {
        char c = *str++;
        hash = (hash * prime) + c;
    }
    return hash;
}

void mat_writedb(FILE *f, std::map<std::string, Material> &materials)
{
    // Count textures
    int ntextures = 0;
    for (auto& [name, mat] : materials) {
        if (mat.tex[0]) ntextures++;
        if (mat.tex[1]) ntextures++;
    }

    // Prepare hashtable
    const int primes[] = { 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97 };
    int hash_prime = -1;
    std::map<uint32_t, std::string> hashes;
    for (int i=0; i<sizeof(primes)/sizeof(primes[0]); i++) {
        // Check if this prime makes hashes of all material names unique
        bool unique = true;
        hashes.clear();
        for (auto& [name, mat] : materials) {
            uint32_t hash = prime_hash(name.c_str(), primes[i]);
            if (hashes.count(hash)) {
                unique = false;
                break;
            }
            hashes[hash] = name;
        }
        if (unique) {
            verbose("using prime %d for hash table\n", primes[i]);
            hash_prime = primes[i];
            break;
        }
    }
    assert(hash_prime > 0);

    // Write header
    fwrite("MDB", 1, 3, f);
    w8(f, 1); // version
    w32_placeholderf(f, "meta_size");
    w16(f, ntextures); // num_textures
    w16(f, materials.size()); // num_materials
    w16(f, hash_prime); // hash_prime
    w16(f, 0); // flags
    w32_placeholderf(f, "textures");

    // Write hash table
    for (auto& [h, name] : hashes) {  // hash table
        w32(f, h);
        w32_placeholderf(f, "mat.%s", name.c_str());
    }

    // Write textures
    placeholder_set(f, "textures");
    for (int i=0; i<ntextures; i++) {
        w32_placeholderf(f, "tex.%d.offset", i);
        w32_placeholderf(f, "tex.%d.size", i);
    }

    // Write materials
    for (auto& [name, mat] : materials) {
        placeholder_set(f, "mat.%s", name.c_str());
        mat.write(f);
    }

    placeholder_set(f, "meta_size");

    // Write texture data
    int texid = 0;
    for (auto& [name, mat] : materials) {
        for (int i=0; i<2; i++) {
            if (mat.tex[i]) {
                walign(f, 2);
                placeholder_set(f, "tex.%d.offset", texid);
                placeholder_set_offset(f, mat.tex[i].sprite.size(), "tex.%d.size", texid);
                fwrite(&mat.tex[i].sprite[0], 1, mat.tex[i].sprite.size(), f);
            }
        }
    }
}
