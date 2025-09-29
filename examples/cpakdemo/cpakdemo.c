#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <libdragon.h>

// Buffer to save pak content for CopyToRAM/CopyFromRAM operation
#define MAX_BANKS 62
#define BANK_SIZE 32768
static uint8_t pak_content[MAX_BANKS * BANK_SIZE];

enum program_state_type {
    STATE_PRINT_CONNECT_MSG,
    STATE_WAIT_CONNECT,
    STATE_PRINT_PAK_OVERVIEW,
    STATE_WAIT_ACTION,
    STATE_FSCK,
    STATE_FSCK_FIX,
    STATE_FORMAT,
    STATE_FORMAT_ERASE,
    STATE_CREATE_FILE,
    STATE_DELETE_FILE,
    STATE_COPY_TO_RAM,
    STATE_COPY_FROM_RAM,
    STATE_CORRUPT
};

void print_working_controller_message(joypad_port_t port) {
    printf("Working with controller %d\n", port + 1);
}

void print_actions() {
    printf("Start:List A:Check B:CreateFile R:CopyToRAM\n"
           "Z+A:Fix Z+B:DeleteFile Z+R:CopyFromRAM\n"
           "Z+C-Up:Format Z+C-Down:Erase Z+L:Corrupt\n");
}

enum program_state_type state_wait_action(joypad_buttons_t btn) {
    // FIXME: properly handle non pressed button
    if (btn.z) {
        // risky actions can only be done with Z held
        if (btn.a) {
            return STATE_FSCK_FIX;
        } else if (btn.b) {
            return STATE_DELETE_FILE;
        } else if (btn.r) {
            return STATE_COPY_FROM_RAM;
        } else if (btn.l) {
            return STATE_CORRUPT;
        } else if (btn.c_up) {
            return STATE_FORMAT;
        } else if (btn.c_down) {
            return STATE_FORMAT_ERASE;
        }
    } else {
        // regular actions
        if (btn.a) {
            return STATE_FSCK;
        } else if (btn.b) {
            return STATE_CREATE_FILE;
        } else if (btn.r) {
            return STATE_COPY_TO_RAM;
        } else if (btn.start) {
            return STATE_PRINT_PAK_OVERVIEW;
        }
    }

    return STATE_WAIT_ACTION;
}

enum program_state_type state_print_connect_message(joypad_port_t port)
{
    console_clear();
    print_working_controller_message(port);
    printf("Please connect joypad %d and insert a controller pak into it.\n", port + 1);
    console_render();

    return STATE_WAIT_CONNECT;
}

enum program_state_type state_wait_connect(joypad_port_t port)
{
    if (joypad_is_connected(port) && joypad_get_accessory_type(port) == JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK) {
        return STATE_PRINT_PAK_OVERVIEW;
    }

    return STATE_WAIT_CONNECT;
}

enum program_state_type state_print_pak_overview(joypad_port_t port, const char* prefix)
{
    console_clear();
    print_working_controller_message(port);

    // Reading the serial can be done before mounting the fs
    uint8_t serial[24];
    memset(serial, 0, 24);
    int err = cpakfs_get_serial(port, serial);
    if (err < 0) {
        printf("Unable to read serial (%d)\n", err);
        printf("(%d) %s\n", errno, strerror(errno));
    } else {
        printf("Serial: %08"PRIx32"%08"PRIx32" %08"PRIx32"%08"PRIx32" %08"PRIx32"%08"PRIx32"\n",
            *(const uint32_t*)(serial+0*4),
            *(const uint32_t*)(serial+1*4),
            *(const uint32_t*)(serial+2*4),
            *(const uint32_t*)(serial+3*4),
            *(const uint32_t*)(serial+4*4),
            *(const uint32_t*)(serial+5*4));
    }

    err = cpakfs_mount(port, prefix);
    if (err < 0) {
        printf("Unable to mount cpak filesystem (%d)\n", err);
        printf("(%d) %s\n", errno, strerror(errno));
        print_actions();
        console_render();
        return STATE_WAIT_ACTION;
    }

    cpakfs_stats_t stats;
    err = cpakfs_get_stats(port, &stats);
    if (err < 0) {
        printf("Unable to get cpak filesystem stats (%d)\n", err);
        printf("(%d) %s\n", errno, strerror(errno));
    } else {
        printf("pages: %d / %d, notes: %d / %d, banks: %d\n",
            stats.pages.used,
            stats.pages.total,
            stats.notes.used,
            stats.notes.total,
            stats.num_banks);
    }

    int idx = 0;
    dir_t dir;
    if (dir_findfirst(prefix,  &dir) == 0) do {
        char path[255 + 8];
        uint8_t buf[8];
        char buf_str[20];
        FILE* f;

        // read first few bytes of file content and print some hex dump of it
        sprintf(path, "%s%s", prefix, dir.d_name);
        memset(buf, 0, sizeof(buf));
        f = fopen(path, "rb");
        // a filename returned by directory iteration must be reachable by fopen
        assert(f != NULL);

        int nread = fread(buf, 1, sizeof(buf), f);
        char* dst = buf_str;
        for (int i = 0; i < nread; i++) {
            int nbytes = sprintf(dst, "%02x", buf[i]);
            if (nbytes <= 0) {
                break;
            }
            dst += nbytes;
        }
        if (nread >= 8) {
            sprintf(dst, "...");
        }

        fclose(f);

        // should fit on a single line
        printf("%02d - %-28s - %5"PRIi64" - %-19s\n", idx, dir.d_name, dir.d_size, buf_str);

        ++idx;
    } while(dir_findnext(prefix, &dir) == 0);

    for (; idx < 16+1; idx++) {
        printf("\n");
    }

    cpakfs_unmount(port);

    print_actions();
    console_render();
    return STATE_WAIT_ACTION;
}

void report_issue(void *ctx, cpakfs_issue_t issue, cpakfs_issue_level_t level, const char *fmt, ...) {
    int* nerr = (int*)ctx;
    const int limit = 10;

    // Only print the first 10 issues
    if (*nerr >= limit) {
        return;
    }

    const char* level_str;
    switch(level) {
    case CPAKFS_LEVEL_INFO: level_str = "INFO"; break;
    case CPAKFS_LEVEL_WARNING: level_str = "WARN"; break;
    case CPAKFS_LEVEL_ERROR: level_str = "ERROR"; break;
    default: level_str = "???";
    }

    printf("[%s] ", level_str);

    va_list arg;
    va_start(arg, fmt);
    vprintf(fmt, arg);
    va_end(arg);

    printf("\n");

    (*nerr)++;

    if (*nerr >= limit) {
        printf("...\n");
    }
}

enum program_state_type state_fsck(joypad_port_t port, bool fix) {
    console_clear();
    print_working_controller_message(port);
    printf("%s pak\n", (fix ? "Fixing" : "Validating"));
    // give early feedback to user in case operation takes time
    console_render();

    int nerr = 0;
    int err = cpakfs_fsck(port, fix, report_issue, &nerr);
    if (err < 0) {
        printf("Unable to %s file system (%d)\n", (fix ? "fix" : "validate"), err);
        printf("(%d) %s\n", errno, strerror(errno));
    } else {
        printf("%s %d errors !\n", (fix ? "Fixed" : "Found"), err);
    }

    print_actions();
    console_render();
    return STATE_WAIT_ACTION;
}

enum program_state_type state_format(joypad_port_t port, bool erase) {
    console_clear();
    print_working_controller_message(port);
    printf("Formatting pak%s\n", (erase ? " and purge all pages content" : ""));
    // give early feedback to user because operation takes time
    console_render();

    int err = cpakfs_format(port, erase);
    if (err < 0) {
        printf("Unable to format pak(%d)\n", err);
        printf("(%d) %s\n", errno, strerror(errno));
    } else {
        printf("Formatting done!\n");
    }

    print_actions();
    console_render();
    return STATE_WAIT_ACTION;
}

enum program_state_type state_corrupt(joypad_port_t port) {
    console_clear();
    print_working_controller_message(port);
    printf("Corrupting\n");
    console_render();

    // Overwrite the first five pages of first bank to corrupt fs.
    // This matches the behavior of old cpaktest example.
    // But I'm not sure how useful that is...
    int err = 0;
    uint8_t buffer[256];
    memset(buffer, 0xff, sizeof(buffer));

    for (int i = 0; i < 5; i++) {
        printf("Erasing page %d\n", i);
        console_render();
        err = cpak_write(port, 0, i * 256, buffer, 256);
        if (err < 0) {
            printf("Unable to write to pak(%d)\n", err);
            printf("(%d) %s\n", errno, strerror(errno));
            break;
        }
    }

    if (err == 0) {
        printf("Pak has been corrupted\n");
    }

    print_actions();
    console_render();
    return STATE_WAIT_ACTION;
}

enum program_state_type state_copy_to_ram(joypad_port_t port, uint8_t* buffer, size_t* size) {
    console_clear();
    print_working_controller_message(port);
    printf("Copying to RAM\n");

    // First we need to determine how many banks there is
    // We use the bank probing method to avoid relying on fs-level
    // statistics.
    printf("Probing number of banks\n");
    console_render();
    int banks = cpak_probe_banks(port);
    if (banks < 0) {
        printf("Unable to probe number of banks(%d)\n", banks);
        printf("(%d) %s\n", errno, strerror(errno));
        print_actions();
        console_render();
        return STATE_WAIT_ACTION;
    }
    printf("Found %d banks\n", banks);
    console_render();

    // Copy loop done in chunks that don't cross the bank boundary.
    // Assume that when cpak_read succeed the number of bytes read
    // equals the number passed as argument (and not less).
    // This is the case in the current cpak_read implementation but
    // is not documented as such in the public API documentation.
    size_t pak_size = banks * BANK_SIZE;
    uint8_t bank = 0;
    printf("Copying %u bytes\n", pak_size);
    uint64_t t0 = get_ticks_ms();
    while (bank < banks) {
        int nbytes = cpak_read(port, bank, 0, buffer + bank * BANK_SIZE, BANK_SIZE);
        if (nbytes < 0) {
            printf("\n");
            printf("Unable to read pak(%d)\n", nbytes);
            printf("(%d) %s\n", errno, strerror(errno));
            break;
        }
        assert(nbytes == BANK_SIZE);
        bank++;

        // Primitive way of showing progress
        printf(".");
        console_render();
    }
    uint64_t t1 = get_ticks_ms();
    printf("\n");

    *size = bank * BANK_SIZE;

    int64_t duration = t1 - t0;
    printf("%u bytes copied in %"PRIi64" ms: %.2fkb/s\n", bank * BANK_SIZE, duration, (1. * (bank * BANK_SIZE))/duration);

    print_actions();
    console_render();
    return STATE_WAIT_ACTION;
}

enum program_state_type state_copy_from_ram(joypad_port_t port, const uint8_t* buffer, size_t size) {
    console_clear();
    print_working_controller_message(port);
    printf("Copying from RAM\n");

    // Early back off if RAM buffer is empty
    if (size == 0) {
        printf("Nothing to copy from.\nPlease first copy pak to RAM using R button\n");
        print_actions();
        console_render();
        return STATE_WAIT_ACTION;
    }

    // First we need to determine how many banks there is
    // We use the bank probing method to avoid relying on fs-level
    // statistics.
    printf("Probing number of banks\n");
    console_render();
    int banks = cpak_probe_banks(port);
    if (banks < 0) {
        printf("Unable to probe number of banks(%d)\n", banks);
        printf("(%d) %s\n", errno, strerror(errno));
        print_actions();
        console_render();
        return STATE_WAIT_ACTION;
    }
    printf("Found %d banks\n", banks);
    console_render();

    size_t pak_size = banks * BANK_SIZE;

    // Ensure that sizes matches
    if (size != pak_size) {
        printf("Pak size (%u) differs from size in RAM (%u)\n", pak_size, size);
        print_actions();
        console_render();
        return STATE_WAIT_ACTION;
    }

    // Copy loop done in chunks that don't cross the bank boundary.
    // Assume that when cpak_write succeed the number of bytes written
    // equals the number passed as argument (and not less).
    // This is the case in the current cpak_write implementation but
    // is not documented as such in the public API documentation.
    uint8_t bank = 0;
    printf("Copying %u bytes\n", size);
    uint64_t t0 = get_ticks_ms();
    while (bank < banks) {
        int nbytes = cpak_write(port, bank, 0, buffer + bank * BANK_SIZE, BANK_SIZE);
        if (nbytes < 0) {
            printf("\n");
            printf("Unable to write pak(%d)\n", nbytes);
            printf("(%d) %s\n", errno, strerror(errno));
            break;
        }
        assert(nbytes == BANK_SIZE);
        bank++;

        // Primitive way of showing progress
        printf(".");
        console_render();
    }
    uint64_t t1 = get_ticks_ms();
    printf("\n");

    int64_t duration = t1 - t0;
    printf("%u bytes copied in %"PRIi64" ms: %.2fkb/s\n", bank * BANK_SIZE, duration, (1. * (bank * BANK_SIZE))/duration);

    print_actions();
    console_render();
    return STATE_WAIT_ACTION;
}

enum program_state_type state_create_file(joypad_port_t port, const char* prefix, const char* fname) {
    console_clear();
    print_working_controller_message(port);
    printf("Creating file\n");

    bool ok = true;

    int err = cpakfs_mount(port, prefix);
    if (err < 0) {
        ok = false;
        printf("Unable to mount cpak filesystem (%d)\n", err);
        printf("(%d) %s\n", errno, strerror(errno));
        print_actions();
        console_render();
        return STATE_WAIT_ACTION;
    }

    char path[255 + 8];
    sprintf(path, "%s%s", prefix, fname);

    printf("Opening file %s\n", fname);
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        ok = false;
        printf("Unable to open file\n");
        printf("(%d) %s\n", errno, strerror(errno));
    } else {
        const uint8_t content[] = {
            'c', 'p', 'a', 'k', '_', 'd', 'e', 'm', 'o',
        };
        size_t size = sizeof(content);

        printf("Writing content\n");
        int nwritten = fwrite(content, 1, size, f);
        if (nwritten != size) {
            ok = false;
            printf("Unable to write requested amount of bytes: %u != %u\n", nwritten, size);
            printf("(%d) %s\n", errno, strerror(errno));
        }

        err = fclose(f);
        if (err < 0) {
            ok = false;
            printf("Unable to close file (%d)\n", err);
            printf("(%d) %s\n", errno, strerror(errno));
        }
    }

    err = cpakfs_unmount(port);
    if (err < 0) {
        ok = false;
        printf("Unable to unmount filesystem (%d)\n", err);
        printf("(%d) %s\n", errno, strerror(errno));
    }

    if (ok) {
        printf("File %s created\n", fname);
    }

    print_actions();
    console_render();
    return STATE_WAIT_ACTION;
}


enum program_state_type state_delete_file(joypad_port_t port, const char* prefix, const char* fname) {
    console_clear();
    print_working_controller_message(port);
    printf("Deleting file\n");

    bool ok = true;

    int err = cpakfs_mount(port, prefix);
    if (err < 0) {
        ok = false;
        printf("Unable to mount cpak filesystem (%d)\n", err);
        printf("(%d) %s\n", errno, strerror(errno));
        print_actions();
        console_render();
        return STATE_WAIT_ACTION;
    }

    char path[255 + 8];
    sprintf(path, "%s%s", prefix, fname);

    err = remove(path);
    if (err != 0) {
        ok = false;
        printf("Unable to remove file (%d)\n", err);
        printf("(%d) %s\n", errno, strerror(errno));
    }

    err = cpakfs_unmount(port);
    if (err < 0) {
        ok = false;
        printf("Unable to unmount filesystem (%d)\n", err);
        printf("(%d) %s\n", errno, strerror(errno));
    }

    if (ok) {
        printf("File %s deleted\n", fname);
    }

    print_actions();
    console_render();
    return STATE_WAIT_ACTION;
}


int main(void)
{
    // Only handle controller pak from player 1
    const joypad_port_t port = JOYPAD_PORT_1;

    // Recommended prefix is 'cpakN:/' with N being the player number (1..4)
    char prefix[8];
    sprintf(prefix, "cpak%d:/", port + 1);

    // Full name must respect the GAME.PU-filename.ext format
    // To make gamecode match the one from ROM Header (NED\x00)
    // we use the hex encoding of gamecode because NED\x00
    // contains a \x00 which is not useable in C strings.
    const char* fname = "4e454400.XX-TEST.A";

    console_init();
    joypad_init();

    console_set_render_mode(RENDER_MANUAL);
    console_clear();

    size_t pak_content_size = 0;
    enum program_state_type state = STATE_PRINT_CONNECT_MSG;
    while (1)
    {
        joypad_poll();
        joypad_buttons_t btn = joypad_get_buttons(port);

        // Handle joypad / accessory disconnection
        if (state > STATE_WAIT_CONNECT) {
            if (!joypad_is_connected(port) || joypad_get_accessory_type(port) != JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK) {
                // may fail if cpakfs wasn't mounted yet
                cpakfs_unmount(port);
                state = STATE_PRINT_CONNECT_MSG;
            }
        }

        switch(state) {
        case STATE_PRINT_CONNECT_MSG:
            state = state_print_connect_message(port);
            break;

        case STATE_WAIT_CONNECT:
            state = state_wait_connect(port);
            break;

        case STATE_PRINT_PAK_OVERVIEW:
            state = state_print_pak_overview(port, prefix);
            break;

        case STATE_WAIT_ACTION:
            state = state_wait_action(btn);
            break;

        case STATE_FSCK:
        case STATE_FSCK_FIX:
            bool fix = (state == STATE_FSCK_FIX);
            state = state_fsck(port, fix);
            break;

        case STATE_FORMAT:
        case STATE_FORMAT_ERASE:
            bool erase = (state == STATE_FORMAT_ERASE);
            state = state_format(port, erase);
            break;

        case STATE_CORRUPT:
            state = state_corrupt(port);
            break;

        case STATE_COPY_TO_RAM:
            state = state_copy_to_ram(port, pak_content, &pak_content_size);
            break;

        case STATE_COPY_FROM_RAM:
            state = state_copy_from_ram(port, pak_content, pak_content_size);
            break;

        case STATE_CREATE_FILE:
            state = state_create_file(port, prefix, fname);
            break;

        case STATE_DELETE_FILE:
            state = state_delete_file(port, prefix, fname);
            break;

        default:
        }
    }
}
