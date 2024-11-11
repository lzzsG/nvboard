#include <nvboard.h>
#include <keyboard.h>
#include <stdarg.h>
#include <macro.h>

#define FPS 120

static SDL_Window *main_window = nullptr;
static SDL_Renderer *main_renderer = nullptr;
PinNode pin_array[NR_PINS];

static bool need_redraw = true;
void set_redraw() { need_redraw = true; }

void vga_update();
void kb_update();
void uart_tx_receive();
void uart_rx_send();

void nvboard_update()
{
  extern uint8_t *vga_blank_n_ptr;
  if (*vga_blank_n_ptr)
    vga_update();

  extern bool is_kb_idle;
  if (unlikely(!is_kb_idle))
    kb_update();

  extern int16_t uart_divisor_cnt;
  extern bool is_uart_rx_idle;
  if (unlikely((--uart_divisor_cnt) < 0))
  {
    uart_tx_receive();
    if (unlikely(!is_uart_rx_idle))
      uart_rx_send();
  }

  static uint64_t last = 0;
  static int cpf = 1; // count per frame
  static int cnt = 0;
  if (unlikely((--cnt) < 0))
  {
    uint64_t now = nvboard_get_time();
    uint64_t diff = now - last;
    if (diff == 0)
      return;
    int cpf_new = ((uint64_t)cpf * 1000000) / ((uint64_t)diff * FPS); // adjust cpf
    cnt += cpf_new - cpf;
    cpf = cpf_new;
    if (diff > 1000000 / FPS)
    {
      last = now;
      cnt = cpf;

      void read_event();
      read_event();
      update_components(main_renderer);
      if (need_redraw)
      {
        SDL_RenderPresent(main_renderer);
        need_redraw = false;
      }
    }
  }
}

void nvboard_init(int vga_clk_cycle)
{
  // 初始化 SDL 和 SDL_image
  SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_EVENTS);
  IMG_Init(IMG_INIT_PNG);

  // 计算窗口大小（根据缩放比例调整）
  int window_width = (int)(WINDOW_WIDTH * SCALE);
  int window_height = (int)(WINDOW_HEIGHT * SCALE);

  // 创建窗口和渲染器
  main_window = SDL_CreateWindow("NVBoard " VERSION_STR, 0, 0, window_width, window_height,
                                 SDL_WINDOW_SHOWN | SDL_WINDOW_ALWAYS_ON_TOP);

  main_renderer = SDL_CreateRenderer(main_window, -1,
#ifdef VSYNC
                                     SDL_RENDERER_PRESENTVSYNC |
#endif
#ifdef HARDWARE_ACC
                                         SDL_RENDERER_ACCELERATED |
#else
                                     SDL_RENDERER_SOFTWARE |
#endif
                                         0);

  // 设置全局缩放比例
  SDL_RenderSetScale(main_renderer, SCALE, SCALE);

  SDL_SetRenderDrawColor(main_renderer, 0xff, 0xff, 0xff, 0);
  SDL_RenderClear(main_renderer);

  // 初始化引脚
  for (int i = 0; i < NR_PINS; i++)
  {
    if (pin_array[i].ptr == NULL)
      pin_array[i].ptr = &pin_array[i].data;
  }

  void init_font(SDL_Renderer * renderer);
  init_font(main_renderer);
  init_render(main_renderer);
  init_components(main_renderer);
  init_gui(main_renderer);

  void init_nvboard_timer();
  init_nvboard_timer();

  update_components(main_renderer);

  extern void vga_set_clk_cycle(int cycle);
  vga_set_clk_cycle(vga_clk_cycle);
}

void nvboard_quit()
{
  delete_components();
  SDL_DestroyWindow(main_window);
  SDL_DestroyRenderer(main_renderer);
  IMG_Quit();
  SDL_Quit();
}

void nvboard_bind_pin(void *signal, int len, ...)
{
  assert(len < 64);
  va_list ap;
  va_start(ap, len);
  for (int i = 0; i < len; i++)
  {
    uint16_t pin = va_arg(ap, int);
    pin_array[pin].ptr = signal;
    pin_array[pin].vector_len = len;
    pin_array[pin].bit_offset = len - 1 - i;
  }
  va_end(ap);
}
