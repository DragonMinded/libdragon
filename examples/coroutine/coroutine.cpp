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
        coro_yield();
    
        printf("Hello from coroutine 2: %ld\n", a + b);
        coro_yield();
        ++a;
        b *= 2;
        c += 1;
        c2 +=2;
    }

    coro_sleep(TICKS_FROM_MS(1000));
}

int main()
{
  debug_init_emulog();
  debug_init_usblog();
  console_init();
  console_set_render_mode(RENDER_MANUAL);
  float test = 1.0f;

  auto co = coro_create(worker, NULL, 4096);

  while (1) {
    console_clear();

    if(co)coro_resume(co);
    wait_ms(100);

    printf("Iteration: %f\n", test); test += 1;
    printf("Done?: %d\n", coro_finished(co));
    if(co && coro_finished(co)) {
      coro_destroy(co);
      co = nullptr;
    }
    console_render();
  }
}
