// Extracted from https://github.com/jca02266/lha
// and simplified for our use case.
// TODO(rasky): still not a proper library, it will call fatal_error + exit(1)
// when something isn't right.

#include "lzh5_compress.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <memory.h>
#include <limits.h>

#undef DEBUG

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif
#ifndef UCHAR_MAX
#define UCHAR_MAX ((1<<(sizeof(unsigned char)*8))-1)
#endif
#ifndef USHRT_MAX
#define USHRT_MAX ((1<<(sizeof(unsigned short)*8))-1)
#endif
#ifndef ULONG_MAX
#define ULONG_MAX ((1<<(sizeof(unsigned long)*8))-1)
#endif

static short bitbuf;
static unsigned char *text;
static unsigned short dicbit;
static unsigned short maxmatch;
static int unpackable;
static off_t origsize, compsize;
static FILE *infile, *outfile;
static unsigned short left[], right[];

static void __attribute__((noreturn, format(printf, 1, 2))) fatal_error(const char *str, ...) {
    va_list va;
    va_start(va, str);
    vfprintf(stderr, str, va);
    va_end(va);
    exit(1);
}

/* ------------------------------------------------------------------------ */
/* LHa for UNIX    Archiver Driver  macro define                            */
/*                                                                          */
/*      Modified                Nobutaka Watazaki                           */
/*                                                                          */
/*  Ver. 1.14   Soruce All chagned              1995.01.14  N.Watazaki      */
/*  Ver. 1.14g  modified                        2000.05.06  T.OKAMOTO       */
/* ------------------------------------------------------------------------ */

#define FALSE           0
#define TRUE            1

#define FILENAME_LENGTH 1024

/* ------------------------------------------------------------------------ */
/* YOUR CUSTOMIZIES                                                         */
/* ------------------------------------------------------------------------ */

#ifndef ARCHIVENAME_EXTENTION
#define ARCHIVENAME_EXTENTION   ".lzh"
#endif
#ifndef BACKUPNAME_EXTENTION
#define BACKUPNAME_EXTENTION    ".bak"
#endif

#define SJIS_FIRST_P(c)         \
  (((unsigned char)(c) >= 0x80 && (unsigned char)(c) < 0xa0) || \
   ((unsigned char)(c) >= 0xe0 && (unsigned char)(c) < 0xfd))
#define SJIS_SECOND_P(c)            \
  (((unsigned char)(c) >= 0x40 && (unsigned char)(c) < 0xfd) && \
    (unsigned char)(c) != 0x7f)

#define X0201_KANA_P(c)\
    (0xa0 < (unsigned char)(c) && (unsigned char)(c) < 0xe0)

/* for filename conversion */
#define NONE 0
#define CODE_EUC 1
#define CODE_SJIS 2
#define CODE_UTF8 3
#define CODE_CAP 4              /* Columbia AppleTalk Program */
#define TO_LOWER 1
#define TO_UPPER 2

#define LZHUFF0_METHOD          "-lh0-"
#define LZHUFF1_METHOD          "-lh1-"
#define LZHUFF2_METHOD          "-lh2-"
#define LZHUFF3_METHOD          "-lh3-"
#define LZHUFF4_METHOD          "-lh4-"
#define LZHUFF5_METHOD          "-lh5-"
#define LZHUFF6_METHOD          "-lh6-"
#define LZHUFF7_METHOD          "-lh7-"
#define LARC_METHOD             "-lzs-"
#define LARC5_METHOD            "-lz5-"
#define LARC4_METHOD            "-lz4-"
#define LZHDIRS_METHOD          "-lhd-"
#define PMARC0_METHOD           "-pm0-"
#define PMARC2_METHOD           "-pm2-"

#define METHOD_TYPE_STORAGE     5

/* Added N.Watazaki ..V */
#define LZHUFF0_METHOD_NUM      0
#define LZHUFF1_METHOD_NUM      1
#define LZHUFF2_METHOD_NUM      2
#define LZHUFF3_METHOD_NUM      3
#define LZHUFF4_METHOD_NUM      4
#define LZHUFF5_METHOD_NUM      5
#define LZHUFF6_METHOD_NUM      6
#define LZHUFF7_METHOD_NUM      7
#define LARC_METHOD_NUM         8
#define LARC5_METHOD_NUM        9
#define LARC4_METHOD_NUM        10
#define LZHDIRS_METHOD_NUM      11
#define PMARC0_METHOD_NUM       12
#define PMARC2_METHOD_NUM       13
/* Added N.Watazaki ..^ */

#define LZHUFF0_DICBIT           0      /* no compress */
#define LZHUFF1_DICBIT          12      /* 2^12 =  4KB sliding dictionary */
#define LZHUFF2_DICBIT          13      /* 2^13 =  8KB sliding dictionary */
#define LZHUFF3_DICBIT          13      /* 2^13 =  8KB sliding dictionary */
#define LZHUFF4_DICBIT          12      /* 2^12 =  4KB sliding dictionary */
#define LZHUFF5_DICBIT          13      /* 2^13 =  8KB sliding dictionary */
#define LZHUFF6_DICBIT          15      /* 2^15 = 32KB sliding dictionary */
#define LZHUFF7_DICBIT          16      /* 2^16 = 64KB sliding dictionary */
#define LARC_DICBIT             11      /* 2^11 =  2KB sliding dictionary */
#define LARC5_DICBIT            12      /* 2^12 =  4KB sliding dictionary */
#define LARC4_DICBIT             0      /* no compress */
#define PMARC0_DICBIT            0      /* no compress */
#define PMARC2_DICBIT           13      /* 2^13 =  8KB sliding dictionary */

#ifdef SUPPORT_LH7
#define MAX_DICBIT          LZHUFF7_DICBIT      /* lh7 use 16bits */
#endif
#ifndef SUPPORT_LH7
#define MAX_DICBIT          LZHUFF6_DICBIT      /* lh6 use 15bits */
#endif

#define MAX_DICSIZ          (1L << MAX_DICBIT)

#define EXTEND_GENERIC          0
#define EXTEND_UNIX             'U'
#define EXTEND_MSDOS            'M'
#define EXTEND_MACOS            'm'
#define EXTEND_OS9              '9'
#define EXTEND_OS2              '2'
#define EXTEND_OS68K            'K'
#define EXTEND_OS386            '3' /* OS-9000??? */
#define EXTEND_HUMAN            'H'
#define EXTEND_CPM              'C'
#define EXTEND_FLEX             'F'
#define EXTEND_RUNSER           'R'
#define EXTEND_AMIGA            'A'

/* this OS type is not official */

#define EXTEND_TOWNSOS          'T'
#define EXTEND_XOSK             'X' /* OS-9 for X68000 (?) */
#define EXTEND_JAVA             'J'

/*---------------------------------------------------------------------------*/

#define GENERIC_ATTRIBUTE               0x20
#define GENERIC_DIRECTORY_ATTRIBUTE     0x10

#define CURRENT_UNIX_MINOR_VERSION      0x00

#define LHA_PATHSEP             0xff    /* path separator of the
                                           filename in lha header.
                                           it should compare with
                                           `unsigned char' or `int',
                                           that is not '\xff', but 0xff. */

#define OSK_RW_RW_RW            0000033
#define OSK_FILE_REGULAR        0000000
#define OSK_DIRECTORY_PERM      0000200
#define OSK_SHARED_PERM         0000100
#define OSK_OTHER_EXEC_PERM     0000040
#define OSK_OTHER_WRITE_PERM    0000020
#define OSK_OTHER_READ_PERM     0000010
#define OSK_OWNER_EXEC_PERM     0000004
#define OSK_OWNER_WRITE_PERM    0000002
#define OSK_OWNER_READ_PERM     0000001

#define UNIX_FILE_TYPEMASK      0170000
#define UNIX_FILE_REGULAR       0100000
#define UNIX_FILE_DIRECTORY     0040000
#define UNIX_FILE_SYMLINK       0120000
#define UNIX_SETUID             0004000
#define UNIX_SETGID             0002000
#define UNIX_STICKYBIT          0001000
#define UNIX_OWNER_READ_PERM    0000400
#define UNIX_OWNER_WRITE_PERM   0000200
#define UNIX_OWNER_EXEC_PERM    0000100
#define UNIX_GROUP_READ_PERM    0000040
#define UNIX_GROUP_WRITE_PERM   0000020
#define UNIX_GROUP_EXEC_PERM    0000010
#define UNIX_OTHER_READ_PERM    0000004
#define UNIX_OTHER_WRITE_PERM   0000002
#define UNIX_OTHER_EXEC_PERM    0000001
#define UNIX_RW_RW_RW           0000666

#define LZHEADER_STORAGE        4096

/* ------------------------------------------------------------------------ */
/*  FILE Attribute                                                          */
/* ------------------------------------------------------------------------ */
#define is_directory(statp)     (((statp)->st_mode & S_IFMT) == S_IFDIR)
#define is_symlink(statp)       (((statp)->st_mode & S_IFMT) == S_IFLNK)
#define is_regularfile(statp)   (((statp)->st_mode & S_IFMT) == S_IFREG)

#if 1 /* assume that fopen() will accepts "b" as binary mode on all systems. */
#define WRITE_BINARY    "wb"
#define READ_BINARY     "rb"
#else
#define WRITE_BINARY    "w"
#define READ_BINARY     "r"
#endif

/* ------------------------------------------------------------------------ */
/* Individual macro define                                                  */
/* ------------------------------------------------------------------------ */

#ifndef MIN
#define MIN(a,b) ((a) <= (b) ? (a) : (b))
#endif

/* bitio.c */
#define peekbits(n)     (bitbuf >> (sizeof(bitbuf)*8 - (n)))

/* crcio.c */
#define CRCPOLY         0xA001      /* CRC-16 (x^16+x^15+x^2+1) */
#define INITIALIZE_CRC(crc) ((crc) = 0)
#define UPDATE_CRC(crc, c) \
 (crctable[((crc) ^ (unsigned char)(c)) & 0xFF] ^ ((crc) >> CHAR_BIT))

/* dhuf.c */
#define N_CHAR      (256 + 60 - THRESHOLD + 1)
#define TREESIZE_C  (N_CHAR * 2)
#define TREESIZE_P  (128 * 2)
#define TREESIZE    (TREESIZE_C + TREESIZE_P)
#define ROOT_C      0
#define ROOT_P      TREESIZE_C

/* huf.c */
#define USHRT_BIT           16  /* (CHAR_BIT * sizeof(ushort)) */
#define NP          (MAX_DICBIT + 1)
#define NT          (USHRT_BIT + 3)
#define NC          (UCHAR_MAX + MAXMATCH + 2 - THRESHOLD)

#define PBIT        5       /* smallest integer such that (1 << PBIT) > * NP */
#define TBIT        5       /* smallest integer such that (1 << TBIT) > * NT */
#define CBIT        9       /* smallest integer such that (1 << CBIT) > * NC */

/*      #if NT > NP #define NPT NT #else #define NPT NP #endif  */
#define NPT         0x80

/* larc.c */
#define MAGIC0      18
#define MAGIC5      19

/* lharc.c */
#define CMD_UNKNOWN 0
#define CMD_EXTRACT 1
#define CMD_ADD     2
#define CMD_LIST    3
#define CMD_DELETE  4

#define STREQU(a,b) (((a)[0] == (b)[0]) ? (strcmp ((a),(b)) == 0) : FALSE)

/* shuf.c */
#define N1          286             /* alphabet size */
#define N2          (2 * N1 - 1)    /* # of nodes in Huffman tree */
#define EXTRABITS   8               /* >= log2(F-THRESHOLD+258-N1) */
#define BUFBITS     16              /* >= log2(MAXBUF) */
#define LENFIELD    4               /* bit size of length field for tree output */

/* util.c */
#define BUFFERSIZE  2048

/* slide.c */
/*
#define PERCOLATE  1
#define NIL        0
#define HASH(p, c) ((p) + ((c) << hash1) + hash2)
*/

/* slide.c */
#define MAXMATCH            256 /* formerly F (not more than UCHAR_MAX + 1) */
#define THRESHOLD           3   /* choose optimal value */

/* ------------------------------------------------------------------------ */
/* LHa for UNIX                                                             */
/*              crcio.c -- crc input / output                               */
/*                                                                          */
/*      Modified                Nobutaka Watazaki                           */
/*                                                                          */
/*  Ver. 1.14   Source All chagned              1995.01.14  N.Watazaki      */
/* ------------------------------------------------------------------------ */

static unsigned int crctable[UCHAR_MAX + 1];

/* ------------------------------------------------------------------------ */
static void
make_crctable( /* void */ )
{
    unsigned int    i, j, r;

    for (i = 0; i <= UCHAR_MAX; i++) {
        r = i;
        for (j = 0; j < CHAR_BIT; j++)
            if (r & 1)
                r = (r >> 1) ^ CRCPOLY;
            else
                r >>= 1;
        crctable[i] = r;
    }
}

/* ------------------------------------------------------------------------ */
static unsigned int
calccrc(crc, p, n)
    unsigned int crc;
    char  *p;
    unsigned int    n;
{
    while (n-- > 0)
        crc = UPDATE_CRC(crc, *p++);
    return crc;
}

/* ------------------------------------------------------------------------ */
static int
fread_crc(crcp, p, n, fp)
    unsigned int *crcp;
    void *p;
    int  n;
    FILE *fp;
{
    // if (text_mode)
    //     n = fread_txt(p, n, fp);
    // else
    n = fread(p, 1, n, fp);

    *crcp = calccrc(*crcp, p, n);
#ifdef NEED_INCREMENTAL_INDICATOR
    put_indicator(n);
#endif
    return n;
}

/* ------------------------------------------------------------------------ */
/* LHa for UNIX                                                             */
/*              bitio.c -- bit stream                                       */
/*                                                                          */
/*      Modified                Nobutaka Watazaki                           */
/*                                                                          */
/*  Ver. 1.14   Source All chagned              1995.01.14  N.Watazaki      */
/*              Separated from crcio.c          2002.10.26  Koji Arai       */
/* ------------------------------------------------------------------------ */

static unsigned char subbitbuf, bitcount;

void
fillbuf(n)          /* Shift bitbuf n bits left, read n bits */
    unsigned char   n;
{
    while (n > bitcount) {
        n -= bitcount;
        bitbuf = (bitbuf << bitcount) + (subbitbuf >> (CHAR_BIT - bitcount));
        if (compsize != 0) {
            compsize--;
            int c = getc(infile);
            if (c == EOF) {
                fatal_error("cannot read stream");
            }
            subbitbuf = (unsigned char)c;
        }
        else
            subbitbuf = 0;
        bitcount = CHAR_BIT;
    }
    bitcount -= n;
    bitbuf = (bitbuf << n) + (subbitbuf >> (CHAR_BIT - n));
    subbitbuf <<= n;
}

unsigned short
getbits(n)
    unsigned char   n;
{
    unsigned short  x;

    x = bitbuf >> (2 * CHAR_BIT - n);
    fillbuf(n);
    return x;
}

void
putcode(n, x)           /* Write leftmost n bits of x */
    unsigned char   n;
    unsigned short  x;
{
    while (n >= bitcount) {
        n -= bitcount;
        subbitbuf += x >> (USHRT_BIT - bitcount);
        x <<= bitcount;
        if (compsize < origsize) {
            if (fwrite(&subbitbuf, 1, 1, outfile) == 0) {
                fatal_error("Write error in bitio.c(putcode)");
            }
            compsize++;
        }
        else
            unpackable = 1;
        subbitbuf = 0;
        bitcount = CHAR_BIT;
    }
    subbitbuf += x >> (USHRT_BIT - bitcount);
    bitcount -= n;
}

static void
putbits(n, x)           /* Write rightmost n bits of x */
    unsigned char   n;
    unsigned short  x;
{
    x <<= USHRT_BIT - n;
    putcode(n, x);
}

static void
init_putbits( /* void */ )
{
    bitcount = CHAR_BIT;
    subbitbuf = 0;
}

/* ------------------------------------------------------------------------ */
/* LHa for UNIX                                                             */
/*              maketree.c -- make Huffman tree                             */
/*                                                                          */
/*      Modified                Nobutaka Watazaki                           */
/*                                                                          */
/*  Ver. 1.14   Source All chagned              1995.01.14  N.Watazaki      */
/* ------------------------------------------------------------------------ */

static void
make_code(nchar, bitlen, code, leaf_num)
    int            nchar;
    unsigned char  *bitlen;
    unsigned short *code;       /* table */
    unsigned short *leaf_num;
{
    unsigned short  weight[17]; /* 0x10000ul >> bitlen */
    unsigned short  start[17];  /* start code */
    unsigned short  total;
    int i;
    int c;

    total = 0;
    for (i = 1; i <= 16; i++) {
        start[i] = total;
        weight[i] = 1 << (16 - i);
        total += weight[i] * leaf_num[i];
    }
    for (c = 0; c < nchar; c++) {
        i = bitlen[c];
        code[c] = start[i];
        start[i] += weight[i];
    }
}

static void
count_leaf(node, nchar, leaf_num, depth) /* call with node = root */
    int node;
    int nchar;
    unsigned short leaf_num[];
    int depth;
{
    if (node < nchar)
        leaf_num[depth < 16 ? depth : 16]++;
    else {
        count_leaf(left[node], nchar, leaf_num, depth + 1);
        count_leaf(right[node], nchar, leaf_num, depth + 1);
    }
}

static void
make_len(nchar, bitlen, sort, leaf_num)
    int nchar;
    unsigned char *bitlen;
    unsigned short *sort;       /* sorted characters */
    unsigned short *leaf_num;
{
    int i, k;
    unsigned int cum;

    cum = 0;
    for (i = 16; i > 0; i--) {
        cum += leaf_num[i] << (16 - i);
    }
#if (UINT_MAX != 0xffff)
    cum &= 0xffff;
#endif
    /* adjust len */
    if (cum) {
        leaf_num[16] -= cum; /* always leaf_num[16] > cum */
        do {
            for (i = 15; i > 0; i--) {
                if (leaf_num[i]) {
                    leaf_num[i]--;
                    leaf_num[i + 1] += 2;
                    break;
                }
            }
        } while (--cum);
    }
    /* make len */
    for (i = 16; i > 0; i--) {
        k = leaf_num[i];
        while (k > 0) {
            bitlen[*sort++] = i;
            k--;
        }
    }
}

/* priority queue; send i-th entry down heap */
static void
downheap(i, heap, heapsize, freq)
    int i;
    short *heap;
    size_t heapsize;
    unsigned short *freq;
{
    short j, k;

    k = heap[i];
    while ((j = 2 * i) <= heapsize) {
        if (j < heapsize && freq[heap[j]] > freq[heap[j + 1]])
            j++;
        if (freq[k] <= freq[heap[j]])
            break;
        heap[i] = heap[j];
        i = j;
    }
    heap[i] = k;
}

/* make tree, calculate bitlen[], return root */
static short
make_tree(nchar, freq, bitlen, code)
    int             nchar;
    unsigned short  *freq;
    unsigned char   *bitlen;
    unsigned short  *code;
{
    short i, j, avail, root;
    unsigned short *sort;

    short heap[NC + 1];       /* NC >= nchar */
    size_t heapsize;

    avail = nchar;
    heapsize = 0;
    heap[1] = 0;
    for (i = 0; i < nchar; i++) {
        bitlen[i] = 0;
        if (freq[i])
            heap[++heapsize] = i;
    }
    if (heapsize < 2) {
        code[heap[1]] = 0;
        return heap[1];
    }

    /* make priority queue */
    for (i = heapsize / 2; i >= 1; i--)
        downheap(i, heap, heapsize, freq);

    /* make huffman tree */
    sort = code;
    do {            /* while queue has at least two entries */
        i = heap[1];    /* take out least-freq entry */
        if (i < nchar)
            *sort++ = i;
        heap[1] = heap[heapsize--];
        downheap(1, heap, heapsize, freq);
        j = heap[1];    /* next least-freq entry */
        if (j < nchar)
            *sort++ = j;
        root = avail++;    /* generate new node */
        freq[root] = freq[i] + freq[j];
        heap[1] = root;
        downheap(1, heap, heapsize, freq);    /* put into queue */
        left[root] = i;
        right[root] = j;
    } while (heapsize > 1);

    {
        unsigned short leaf_num[17];

        /* make leaf_num */
        memset(leaf_num, 0, sizeof(leaf_num));
        count_leaf(root, nchar, leaf_num, 0);

        /* make bitlen */
        make_len(nchar, bitlen, code, leaf_num);

        /* make code table */
        make_code(nchar, bitlen, code, leaf_num);
    }

    return root;
}


/* ------------------------------------------------------------------------ */
/* LHa for UNIX                                                             */
/*              huf.c -- new static Huffman                                 */
/*                                                                          */
/*      Modified                Nobutaka Watazaki                           */
/*                                                                          */
/*  Ver. 1.14   Source All chagned              1995.01.14  N.Watazaki      */
/*  Ver. 1.14i  Support LH7 & Bug Fixed         2000.10. 6  t.okamoto       */
/* ------------------------------------------------------------------------ */

#include <stdlib.h>

/* ------------------------------------------------------------------------ */
static unsigned short left[2 * NC - 1], right[2 * NC - 1];

static unsigned short c_code[NC];      /* encode */
static unsigned short pt_code[NPT];    /* encode */

// static unsigned short c_table[4096];   /* decode */
// static unsigned short pt_table[256];   /* decode */

static unsigned short c_freq[2 * NC - 1]; /* encode */
static unsigned short p_freq[2 * NP - 1]; /* encode */
static unsigned short t_freq[2 * NT - 1]; /* encode */

static unsigned char  c_len[NC];
static unsigned char  pt_len[NPT];

static unsigned char *buf;      /* encode */
static unsigned int bufsiz;     /* encode */
// static unsigned short blocksize; /* decode */
static unsigned short output_pos, output_mask; /* encode */

static int pbit;
static int np;
/* ------------------------------------------------------------------------ */
/*                              Encording                                   */
/* ------------------------------------------------------------------------ */
static void
count_t_freq(/*void*/)
{
    short           i, k, n, count;

    for (i = 0; i < NT; i++)
        t_freq[i] = 0;
    n = NC;
    while (n > 0 && c_len[n - 1] == 0)
        n--;
    i = 0;
    while (i < n) {
        k = c_len[i++];
        if (k == 0) {
            count = 1;
            while (i < n && c_len[i] == 0) {
                i++;
                count++;
            }
            if (count <= 2)
                t_freq[0] += count;
            else if (count <= 18)
                t_freq[1]++;
            else if (count == 19) {
                t_freq[0]++;
                t_freq[1]++;
            }
            else
                t_freq[2]++;
        } else
            t_freq[k + 2]++;
    }
}

/* ------------------------------------------------------------------------ */
static void
write_pt_len(n, nbit, i_special)
    short           n;
    short           nbit;
    short           i_special;
{
    short           i, k;

    while (n > 0 && pt_len[n - 1] == 0)
        n--;
    putbits(nbit, n);
    i = 0;
    while (i < n) {
        k = pt_len[i++];
        if (k <= 6)
            putbits(3, k);
        else
            /* k=7 -> 1110  k=8 -> 11110  k=9 -> 111110 ... */
            putbits(k - 3, USHRT_MAX << 1);
        if (i == i_special) {
            while (i < 6 && pt_len[i] == 0)
                i++;
            putbits(2, i - 3);
        }
    }
}

/* ------------------------------------------------------------------------ */
static void
write_c_len(/*void*/)
{
    short           i, k, n, count;

    n = NC;
    while (n > 0 && c_len[n - 1] == 0)
        n--;
    putbits(CBIT, n);
    i = 0;
    while (i < n) {
        k = c_len[i++];
        if (k == 0) {
            count = 1;
            while (i < n && c_len[i] == 0) {
                i++;
                count++;
            }
            if (count <= 2) {
                for (k = 0; k < count; k++)
                    putcode(pt_len[0], pt_code[0]);
            }
            else if (count <= 18) {
                putcode(pt_len[1], pt_code[1]);
                putbits(4, count - 3);
            }
            else if (count == 19) {
                putcode(pt_len[0], pt_code[0]);
                putcode(pt_len[1], pt_code[1]);
                putbits(4, 15);
            }
            else {
                putcode(pt_len[2], pt_code[2]);
                putbits(CBIT, count - 20);
            }
        }
        else
            putcode(pt_len[k + 2], pt_code[k + 2]);
    }
}

/* ------------------------------------------------------------------------ */
static void
encode_c(c)
    short           c;
{
    putcode(c_len[c], c_code[c]);
}

/* ------------------------------------------------------------------------ */
static void
encode_p(p)
    unsigned short  p;
{
    unsigned short  c, q;

    c = 0;
    q = p;
    while (q) {
        q >>= 1;
        c++;
    }
    putcode(pt_len[c], pt_code[c]);
    if (c > 1)
        putbits(c - 1, p);
}

/* ------------------------------------------------------------------------ */
static void
send_block( /* void */ )
{
    unsigned char   flags=0;
    unsigned short  i, k, root, pos, size;

    root = make_tree(NC, c_freq, c_len, c_code);
    size = c_freq[root];
    putbits(16, size);
    if (root >= NC) {
        count_t_freq();
        root = make_tree(NT, t_freq, pt_len, pt_code);
        if (root >= NT) {
            write_pt_len(NT, TBIT, 3);
        } else {
            putbits(TBIT, 0);
            putbits(TBIT, root);
        }
        write_c_len();
    } else {
        putbits(TBIT, 0);
        putbits(TBIT, 0);
        putbits(CBIT, 0);
        putbits(CBIT, root);
    }
    root = make_tree(np, p_freq, pt_len, pt_code);
    if (root >= np) {
        write_pt_len(np, pbit, -1);
    }
    else {
        putbits(pbit, 0);
        putbits(pbit, root);
    }
    pos = 0;
    for (i = 0; i < size; i++) {
        if (i % CHAR_BIT == 0)
            flags = buf[pos++];
        else
            flags <<= 1;
        if (flags & (1 << (CHAR_BIT - 1))) {
            encode_c(buf[pos++] + (1 << CHAR_BIT));
            k = buf[pos++] << CHAR_BIT;
            k += buf[pos++];
            encode_p(k);
        } else
            encode_c(buf[pos++]);
        if (unpackable)
            return;
    }
    for (i = 0; i < NC; i++)
        c_freq[i] = 0;
    for (i = 0; i < np; i++)
        p_freq[i] = 0;
}

/* ------------------------------------------------------------------------ */
/* lh4, 5, 6, 7 */
static void
output_st1(c, p)
    unsigned short  c;
    unsigned short  p;
{
    static unsigned short cpos;

    output_mask >>= 1;
    if (output_mask == 0) {
        output_mask = 1 << (CHAR_BIT - 1);
        if (output_pos >= bufsiz - 3 * CHAR_BIT) {
            send_block();
            if (unpackable)
                return;
            output_pos = 0;
        }
        cpos = output_pos++;
        buf[cpos] = 0;
    }
    buf[output_pos++] = (unsigned char) c;
    c_freq[c]++;
    if (c >= (1 << CHAR_BIT)) {
        buf[cpos] |= output_mask;
        buf[output_pos++] = (unsigned char) (p >> CHAR_BIT);
        buf[output_pos++] = (unsigned char) p;
        c = 0;
        while (p) {
            p >>= 1;
            c++;
        }
        p_freq[c]++;
    }
}

/* ------------------------------------------------------------------------ */
static unsigned char  *
alloc_buf( /* void */ )
{
    bufsiz = 16 * 1024 *2;  /* 65408U; */ /* t.okamoto */
    while ((buf = (unsigned char *) malloc(bufsiz)) == NULL) {
        bufsiz = (bufsiz / 10) * 9;
        if (bufsiz < 4 * 1024)
            fatal_error("Not enough memory");
    }
    return buf;
}

/* ------------------------------------------------------------------------ */
/* lh4, 5, 6, 7 */
void
encode_start_st1( void )
{
    int             i;

    switch (dicbit) {
    case LZHUFF4_DICBIT:
    case LZHUFF5_DICBIT: pbit = 4; np = LZHUFF5_DICBIT + 1; break;
    case LZHUFF6_DICBIT: pbit = 5; np = LZHUFF6_DICBIT + 1; break;
    case LZHUFF7_DICBIT: pbit = 5; np = LZHUFF7_DICBIT + 1; break;
    default:
        fatal_error("Cannot use %d bytes dictionary", 1 << dicbit);
    }

    for (i = 0; i < NC; i++)
        c_freq[i] = 0;
    for (i = 0; i < np; i++)
        p_freq[i] = 0;
    output_pos = output_mask = 0;
    init_putbits();
//    init_code_cache();
    buf[0] = 0;
}

/* ------------------------------------------------------------------------ */
/* lh4, 5, 6, 7 */
void
encode_end_st1( /* void */ )
{
    if (!unpackable) {
        send_block();
        putbits(CHAR_BIT - 1, 0);   /* flush remaining bits */
    }
    free(buf); buf=0;
}



/* ------------------------------------------------------------------------ */
/* LHa for UNIX                                                             */
/*              slide.c -- sliding dictionary with percolating update       */
/*                                                                          */
/*      Modified                Nobutaka Watazaki                           */
/*                                                                          */
/*  Ver. 1.14d  Exchanging a search algorithm  1997.01.11    T.Okamoto      */
/* ------------------------------------------------------------------------ */

#if 0
#define DEBUG 1
#endif

#ifdef DEBUG
FILE *fout = NULL;
static int noslide = 1;
#endif

/* variables for hash */
struct hash {
    unsigned int pos;
    int too_flag;               /* if 1, matching candidate is too many */
} *hash;
static unsigned int *prev;      /* previous posiion associated with hash */

/* hash function: it represents 3 letters from `pos' on `text' */
#define INIT_HASH(pos) \
        ((( (text[(pos)] << 5) \
           ^ text[(pos) + 1]  ) << 5) \
           ^ text[(pos) + 2]         ) & (unsigned)(HSHSIZ - 1);
#define NEXT_HASH(hash,pos) \
        (((hash) << 5) \
           ^ text[(pos) + 2]         ) & (unsigned)(HSHSIZ - 1);

#define TXTSIZ (MAX_DICSIZ * 2L + MAXMATCH)
#define HSHSIZ (((unsigned long)1) <<15)
#define NIL 0
#define LIMIT 0x100             /* limit of hash chain */

static unsigned int txtsiz;
static unsigned long dicsiz;
static unsigned int remain;

struct matchdata {
    int len;
    unsigned int off;
};

static int
encode_alloc(method)
    int method;
{
    switch (method) {
    // case LZHUFF1_METHOD_NUM:
    //     encode_set = encode_define[0];
    //     maxmatch = 60;
    //     dicbit = LZHUFF1_DICBIT;    /* 12 bits  Changed N.Watazaki */
    //     break;
    case LZHUFF5_METHOD_NUM:
        // encode_set = encode_define[1];
        maxmatch = MAXMATCH;
        dicbit = LZHUFF5_DICBIT;    /* 13 bits */
        break;
    case LZHUFF6_METHOD_NUM:
        // encode_set = encode_define[1];
        maxmatch = MAXMATCH;
        dicbit = LZHUFF6_DICBIT;    /* 15 bits */
        break;
    case LZHUFF7_METHOD_NUM:
        // encode_set = encode_define[1];
        maxmatch = MAXMATCH;
        dicbit = LZHUFF7_DICBIT;    /* 16 bits */
        break;
    default:
        fatal_error("unknown method %d", method);
    }

    dicsiz = (((unsigned long)1) << dicbit);
    txtsiz = dicsiz*2+maxmatch;

    if (hash) return method;

    alloc_buf();

    hash = (struct hash*)malloc(HSHSIZ * sizeof(struct hash));
    prev = (unsigned int*)malloc(MAX_DICSIZ * sizeof(unsigned int));
    text = (unsigned char*)malloc(TXTSIZ);

    return method;
}

static void
init_slide()
{
    unsigned int i;

    for (i = 0; i < HSHSIZ; i++) {
        hash[i].pos = NIL;
        hash[i].too_flag = 0;
    }
}

/* update dictionary */
static void
update_dict(pos, crc)
    unsigned int *pos;
    unsigned int *crc;
{
    unsigned int i, j;
    long n;

    memmove(&text[0], &text[dicsiz], txtsiz - dicsiz);

    n = fread_crc(crc, &text[txtsiz - dicsiz], dicsiz, infile);

    remain += n;

    *pos -= dicsiz;
    for (i = 0; i < HSHSIZ; i++) {
        j = hash[i].pos;
        hash[i].pos = (j > dicsiz) ? j - dicsiz : NIL;
        hash[i].too_flag = 0;
    }
    for (i = 0; i < dicsiz; i++) {
        j = prev[i];
        prev[i] = (j > dicsiz) ? j - dicsiz : NIL;
    }
}

/* associate position with token */
static void
insert_hash(token, pos)
    unsigned int token;
    unsigned int pos;
{
    prev[pos & (dicsiz - 1)] = hash[token].pos; /* chain the previous pos. */
    hash[token].pos = pos;
}

static void
search_dict_1(token, pos, off, max, m)
    unsigned int token;
    unsigned int pos;
    unsigned int off;
    unsigned int max;           /* max. length of matching string */
    struct matchdata *m;
{
    unsigned int chain = 0;
    unsigned int scan_pos = hash[token].pos;
    int scan_beg = scan_pos - off;
    int scan_end = pos - dicsiz;
    unsigned int len;

    while (scan_beg > scan_end) {
        chain++;

        if (text[scan_beg + m->len] == text[pos + m->len]) {
            {
                /* collate token */
                unsigned char *a = &text[scan_beg];
                unsigned char *b = &text[pos];

                for (len = 0; len < max && *a++ == *b++; len++);
            }

            if (len > m->len) {
                m->off = pos - scan_beg;
                m->len = len;
                if (m->len == max)
                    break;

#ifdef DEBUG
                if (noslide) {
                    if (pos - m->off < dicsiz) {
                        printf("matchpos=%u scan_pos=%u dicsiz=%u\n",
                               pos - m->off, scan_pos, dicsiz);
                    }
                }
#endif
            }
        }
        scan_pos = prev[scan_pos & (dicsiz - 1)];
        scan_beg = scan_pos - off;
    }

    if (chain >= LIMIT)
        hash[token].too_flag = 1;
}

/* search the longest token matching to current token */
static void
search_dict(token, pos, min, m)
    unsigned int token;         /* search token */
    unsigned int pos;           /* position of token */
    int min;                    /* min. length of matching string */
    struct matchdata *m;
{
    unsigned int off, tok, max;

    if (min < THRESHOLD - 1) min = THRESHOLD - 1;

    max = maxmatch;
    m->off = 0;
    m->len = min;

    off = 0;
    for (tok = token; hash[tok].too_flag && off < maxmatch - THRESHOLD; ) {
        /* If matching position is too many, The search key is
           changed into following token from `off' (for speed). */
        ++off;
        tok = NEXT_HASH(tok, pos+off);
    }
    if (off == maxmatch - THRESHOLD) {
        off = 0;
        tok = token;
    }

    search_dict_1(tok, pos, off, max, m);

    if (off > 0 && m->len < off + 3)
        /* re-search */
        search_dict_1(token, pos, 0, off+2, m);

    if (m->len > remain) m->len = remain;
}

/* slide dictionary */
static void
next_token(token, pos, crc)
    unsigned int *token;
    unsigned int *pos;
    unsigned int *crc;
{
    remain--;
    if (++*pos >= txtsiz - maxmatch) {
        update_dict(pos, crc);
#ifdef DEBUG
        noslide = 0;
#endif
    }
    *token = NEXT_HASH(*token, *pos);
}

void
lzh5_init(int method)
{
    make_crctable();
    encode_alloc(method);
}

void
lzh5_encode(FILE *in, FILE *out, unsigned int *out_crc, unsigned int *out_csize, unsigned int *out_dsize)
{
    unsigned int token, pos, crc;
    off_t count;
    struct matchdata match, last;

#ifdef DEBUG
    if (!fout)
        fout = xfopen("en", "wt");
    fprintf(fout, "[filename: %s]\n", reading_filename);
#endif
    infile = in;
    outfile = out;
    origsize = 999999999; // only used to detect uncompressible input, ignore
    compsize = count = 0L;
    unpackable = 0;

    INITIALIZE_CRC(crc);

    init_slide();

    encode_start_st1();
    memset(text, ' ', TXTSIZ);

    remain = fread_crc(&crc, &text[dicsiz], txtsiz-dicsiz, infile);

    match.len = THRESHOLD - 1;
    match.off = 0;
    if (match.len > remain) match.len = remain;

    pos = dicsiz;
    token = INIT_HASH(pos);
    insert_hash(token, pos);     /* associate token and pos */

    while (remain > 0 && ! unpackable) {
        last = match;

        next_token(&token, &pos, &crc);
        search_dict(token, pos, last.len-1, &match);
        insert_hash(token, pos);

        if (match.len > last.len || last.len < THRESHOLD) {
            /* output a letter */
            output_st1(text[pos - 1], 0);
#ifdef DEBUG
            fprintf(fout, "%u C %02X\n", count, text[pos-1]);
#endif
            count++;
        } else {
            /* output length and offset */
            output_st1(last.len + (256 - THRESHOLD),
                       (last.off-1) & (dicsiz-1) );

#ifdef DEBUG
            {
                int i;
                unsigned char *ptr;
                unsigned int offset = (last.off & (dicsiz-1));

                fprintf(fout, "%u M <%u %u> ",
                        count, last.len, count - offset);

                ptr = &text[pos-1 - offset];
                for (i=0; i < last.len; i++)
                    fprintf(fout, "%02X ", ptr[i]);
                fprintf(fout, "\n");
            }
#endif
            count += last.len;

            --last.len;
            while (--last.len > 0) {
                next_token(&token, &pos, &crc);
                insert_hash(token, pos);
            }
            next_token(&token, &pos, &crc);
            search_dict(token, pos, THRESHOLD - 1, &match);
            insert_hash(token, pos);
        }
    }
    encode_end_st1();

    if (out_csize) *out_csize = compsize;
    if (out_dsize) *out_dsize = count;
    if (out_crc) *out_crc = crc;
}
