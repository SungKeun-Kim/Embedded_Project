// 실제 AVR 바이너리의 Timer1/ADC/ISR 통합 시험. 실행 방법은 test/README 참고.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
#include <sim_avr.h>
#include <sim_io.h>
#include <sim_elf.h>
#include <avr_adc.h>
#include <avr_ioport.h>
#include <avr_timer.h>

// BIN만 읽는 시험이다. Windows용 simavr의 선택적 ELF 의존성을 사용하지 않는다.
// 잘못 ELF 로더를 호출하면 조용히 성공시키지 않고 시험을 즉시 중단한다.
extern "C" int elf_read_firmware(const char *, elf_firmware_t *) { std::abort(); }

struct Probe {
  avr_t *avr;
  uint64_t lastRise = 0, lastZc = 0;
  unsigned count = 0, voltage = 0;
  double period, phase;
  bool high = false;
};

void adcTrigger(avr_irq_t *, uint32_t, void *param) {
  auto &p = *static_cast<Probe *>(param);
  assert(p.avr->cycle >= 40000ULL * 200); // 부팅 안정화 대기 이후에만 입력 제어 시작
  avr_raise_irq(avr_io_getirq(p.avr, AVR_IOCTL_ADC_GETIRQ, ADC_IRQ_ADC3), p.voltage);
}

void gateChanged(avr_irq_t *, uint32_t value, void *param) {
  auto &p = *static_cast<Probe *>(param);
  bool high = value & 1;
  if (high && !p.high) {
    assert(p.avr->cycle >= (40000ULL + 50ULL * 2000) * 200);
    p.lastRise = p.avr->cycle;
    if (p.avr->cycle > 6ULL * p.avr->frequency) {
      ++p.count;
      double delay = double(p.avr->cycle - p.lastZc) / p.period;
      assert(std::abs(delay - p.phase) < 0.012);
    }
  }
  if (!high && p.high && p.avr->cycle > 6ULL * p.avr->frequency) {
    double width = double(p.avr->cycle - p.lastRise) / p.period;
    assert(width > 0.052 && width < 0.067);
    assert(p.avr->cycle - p.lastZc < p.period);
    assert(p.period - (p.avr->cycle - p.lastZc) > 0.05 * p.period);
  }
  p.high = high;
}

void run(const std::vector<uint8_t> &code, unsigned frequency, unsigned hz, unsigned mv,
         unsigned highUs) {
  avr_t *avr = avr_make_mcu_by_name("attiny85");
  assert(avr);
  avr->frequency = frequency;
  assert(avr_init(avr) == 0);
  avr->frequency = frequency;
  avr->fuse[0] = 0xe2;
  avr->fuse[1] = 0xdf;
  avr->fuse[2] = 0xff;
  avr->vcc = avr->avcc = avr->aref = 5000;
  // 배포된 simavr 1.7의 tiny85 Timer1은 CTC1/OCR1C 모드를 누락하여
  // 기본적으로 256*8=2048클럭마다 비교 ISR을 발생시킨다.
  // 이 펌웨어는 OCR1A == OCR1C이므로 CTC TOP을 OCR1A로 연결해 같은 200클럭을 모사한다.
  // 따라서 이 시험은 실제 Timer1 주변장치의 독립 검증을 대체하지 않는다.
  avr_timer_t *timer = nullptr;
  for (avr_io_t *io = avr->io_port; io; io = io->next) {
    if (std::strcmp(io->kind, "timer") == 0) {
      auto *candidate = reinterpret_cast<avr_timer_t *>(io);
      if (candidate->name == '1') timer = candidate;
    }
  }
  assert(timer);
  for (auto &bit : timer->wgm) bit = {};
  timer->wgm[0].reg = 0x50; // TCCR1 데이터 주소
  timer->wgm[0].bit = 7;    // CTC1
  timer->wgm[0].mask = 1;
  timer->wgm_op[1] = {};
  timer->wgm_op[1].kind = avr_timer_wgm_ctc;
  timer->wgm_op[1].top = avr_timer_wgm_reg_ocra;
  avr_loadcode(avr, const_cast<uint8_t *>(code.data()), code.size(), 0);
  Probe p;
  p.avr = avr;
  p.period = double(frequency) / (2 * hz);
  p.phase = double(mv < 1000 ? 138 : 292) / 332;
  p.voltage = mv;
  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 0), gateChanged, &p);
  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_ADC_GETIRQ, ADC_IRQ_OUT_TRIGGER), adcTrigger, &p);
  avr_irq_t *zc = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 2);
  avr_raise_irq(zc, 0);
  unsigned edge = 0;
  uint64_t nextRise = 10000, nextFall = 0;
  const unsigned highCycles = uint64_t(frequency) * highUs / 1000000;
  while (avr->cycle < 7ULL * frequency) {
    if (avr->cycle >= nextRise) {
      p.lastZc = avr->cycle;
      avr_raise_irq(zc, 1);
      nextFall = avr->cycle + highCycles;
      nextRise = 10000 + uint64_t(std::llround(++edge * p.period));
    }
    if (nextFall && avr->cycle >= nextFall) {
      avr_raise_irq(zc, 0);
      nextFall = 0;
    }
    int result = avr_run(avr);
    assert(result != cpu_Crashed && result != cpu_Done);
    if (avr->cycle > 10000) assert(timer->tov_cycles == 200);
  }
  if (!(p.count >= hz * 2 - 1 && p.count <= hz * 2 + 1)) {
    std::fprintf(stderr, "FAIL: count=%u cpu=%u hz=%u mv=%u PC=%x PORTB=%x TCCR1=%x TIMSK=%x\n",
                 p.count, frequency, hz, mv, avr->pc, avr->data[0x38], avr->data[0x50], avr->data[0x59]);
    std::fprintf(stderr, "cycle=%llu actual_frequency=%u input_edges=%u\n",
                 (unsigned long long)avr->cycle, avr->frequency, edge);
    std::abort();
  }
  std::printf("PASS simavr: %uHz, CPU=%uHz, ADC=%umV, ZC=%uus, %u pulses in final second\n",
              hz, frequency, mv, highUs, p.count);
  std::fflush(stdout);
  avr_terminate(avr);
  std::free(avr);
}

int main(int argc, char **argv) {
  assert(argc == 2);
  std::ifstream file(argv[1], std::ios::binary);
  std::vector<uint8_t> code((std::istreambuf_iterator<char>(file)), {});
  assert(!code.empty());
  for (unsigned hz : {50u, 60u})
    for (unsigned frequency : {8000000u, 6800000u, 9200000u})
      for (unsigned mv : {250u, 4300u})
        for (unsigned width : {50u, 100u, 2000u * 60u / hz})
          run(code, frequency, hz, mv, width);
}
