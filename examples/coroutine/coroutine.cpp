#include <libdragon.h>
#include "coroutine.h"

void worker(void* arg) {
    u_int32_t a = 42;
    u_int32_t b = 100;
    float c = 4.0f;
    float c2 = 4.0f;
    for(int i=0; i<5; ++i) 
    {
        printf("Hello from coroutine: %f %f\n", c, c2);
        corot_yield();
    
        printf("Hello from coroutine 2: %ld\n", a + b);
        corot_yield();
        ++a;
        b *= 2;
        c += 1;
        c2 +=2;
    }

    corot_sleep(TICKS_FROM_MS(1000));
}

int main()
{
  debug_init_isviewer();
  debug_init_usblog();
  console_init();
  console_set_render_mode(RENDER_MANUAL);
  float test = 1.0f;

  auto co = corot_create(worker, NULL, 4096);

  while (1) {
    console_clear();

    if(co)corot_resume(co);
    wait_ms(100);

    printf("Iteration: %f\n", test); test += 1;
    printf("Done?: %d\n", corot_finished(co));
    if(co && corot_finished(co)) {
      corot_destroy(co);
      co = nullptr;
    }
    console_render();
  }
}
