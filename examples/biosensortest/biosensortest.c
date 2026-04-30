/**
 * @file biosensortest.c
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief N64 test ROM for Bio Sensor subsystem
 */

#include <string.h>
#include <libdragon.h>

#define HISTOGRAM_WIDTH 60

typedef struct {
    bool buffer[HISTOGRAM_WIDTH];
    int index;
} pulse_history_t;

int main(void)
{
    joypad_buttons_t pressed;
    pulse_history_t history[JOYPAD_PORT_COUNT] = {0};

    timer_init();
    joypad_init();
    debug_init_emulog();

    console_init();
    console_set_render_mode(RENDER_MANUAL);
    console_set_debug(false);

    bio_sensor_init();

    while (1)
    {
        console_clear();
        joypad_poll();

        printf("LibDragon Bio Sensor Subsystem Test ROM\n\n");
        printf("Connect up to 4 controllers with Bio Sensor accessories\n");
        printf("\n");
        printf("Press A to start reading the Bio Sensor\n");
        printf("Press B to stop reading the Bio Sensor\n");
        printf("\n");

        JOYPAD_PORT_FOREACH (port)
        {
            pressed = joypad_get_buttons_pressed(port);

            // Update pulse history
            if (bio_sensor_get_active(port))
            {
                printf("Port %d: ", port + 1);
                printf("BPM: %03d ", bio_sensor_get_bpm(port));

                history[port].buffer[history[port].index] = bio_sensor_get_pulsing(port);
                history[port].index = (history[port].index + 1) % HISTOGRAM_WIDTH;

                if (pressed.b)
                {
                    bio_sensor_read_stop(port);
                    // Clear history when stopping
                    memset(&history[port], 0, sizeof(pulse_history_t));
                }
                printf("Active");
            }
            else if (joypad_get_accessory_type(port) == JOYPAD_ACCESSORY_TYPE_BIO_SENSOR)
            {
                printf("Port %d: BPM: --- ", port + 1);
                if (pressed.a)
                {
                    bio_sensor_read_start(port);
                }
                printf("Stopped; Press A to start");
            }
            else
            {
                printf("Port %d: BPM: --- ", port + 1);
                printf("Bio Sensor not connected");
            }
            printf("\n");

            // Draw histogram
            printf("[");
            for (int i = HISTOGRAM_WIDTH - 1; i >- 0; i--)
            {
                int idx = (history[port].index + i) % HISTOGRAM_WIDTH;
                if (bio_sensor_get_active(port))
                {
                    printf("%c", history[port].buffer[idx] ? '^' : '_');
                }
                else
                {
                    printf("_");
                }
            }
            printf("]\n\n");
        }

        console_render();
    }
}
