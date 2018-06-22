/** @brief Syntactic sugar for lwc2 instuctions */
.macro loadVector vt, combined, base
    .set v0, $0
    .set v1, $1
    .set v2, $2
    .set v3, $3
    .set v4, $4
    .set v5, $5
    .set v6, $6
    .set v7, $7
    .set v8, $8
    .set v9, $9
    .set v10, $10
    .set v11, $11
    .set v12, $12
    .set v13, $13
    .set v14, $14
    .set v15, $15
    .set v16, $16
    .set v17, $17
    .set v18, $18
    .set v19, $19
    .set v20, $20
    .set v21, $21
    .set v22, $22
    .set v23, $23
    .set v24, $24
    .set v25, $25
    .set v26, $26
    .set v27, $27
    .set v28, $28
    .set v29, $29
    .set v30, $30
    .set v31, $31
    lwc2 \vt, \combined (\base)
.endm

/** @brief Syntactic sugar for swc2 instuctions */
.macro storeVector vt, combined, base
    swc2 \vt, \combined (\base)
.endm

/** @brief Syntactic sugar for cop2 instuctions */
.macro vectorOp operation
    .set v0, 0x0
    .set v1, 0x1
    .set v2, 0x2
    .set v3, 0x3
    .set v4, 0x4
    .set v5, 0x5
    .set v6, 0x6
    .set v7, 0x7
    .set v8, 0x8
    .set v9, 0x9
    .set v10, 0xA
    .set v11, 0xB
    .set v12, 0xC
    .set v13, 0xD
    .set v14, 0xE
    .set v15, 0xF
    .set v16, 0x10
    .set v17, 0x11
    .set v18, 0x12
    .set v19, 0x13
    .set v20, 0x14
    .set v21, 0x15
    .set v22, 0x16
    .set v23, 0x17
    .set v24, 0x18
    .set v25, 0x19
    .set v26, 0x1A
    .set v27, 0x1B
    .set v28, 0x1C
    .set v29, 0x1D
    .set v30, 0x1E
    .set v31, 0x1F
    cop2 \operation
.endm