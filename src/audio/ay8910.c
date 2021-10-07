#include "ay8910.h"
#include <assert.h>
#include <memory.h>

#define AY8910_TRACE   0

#if AY8910_TRACE
#define tracef(fmt, ...)  debugf(fmt, ##__VA_ARGS__)
#else
#define tracef(fmt, ...)  ({ })
#endif

#if AY8910_CENTER_SILENCE
#define V(f)  ((f) * 0.5f * AY8910_VOLUME_ATTENUATE + 0.5f)
#else
#define V(f)  ((f) * AY8910_VOLUME_ATTENUATE)
#endif

static const float VOL_TABLE[16] = { V(0.0), V(0.002300939285824675), V(0.005554958830034992), V(0.010156837401684337), V(0.01666487649010497), V(0.02586863363340366), V(0.03888471181024493), V(0.05729222609684229), V(0.08332438245052481), V(0.12013941102371954), V(0.17220372373108456), V(0.24583378087747398), V(0.3499624062922039), V(0.4972225205849827), V(0.7054797714144425), V(1.0) };

#undef V

#define SAMPLE_CONV(f)   ((f) * 65535.0f - 32768.0f)

#define OUTS(s) ({ \
	*out++ = (int16_t)SAMPLE_CONV(s); \
	if (AY8910_OUTPUT_STEREO) *out++ = (int16_t)SAMPLE_CONV(s); \
})

#if AY8910_OUTPUT_STEREO
#ifdef N64
	// Store 32-bits at once. This is twice as faster when accessing uncached
	// addresses like the audio buffers.
	#define OUT(sl_, sr_) ({ \
		*(uint32_t*)out = ((uint32_t)(int16_t)(sl_) << 16) | (uint32_t)(int16_t)(sr_); \
		out += 2; \
	})
#else
	#define OUT(sl_, sr_) ({ \
		*out++ = (int16_t)(sl_); \
		*out++ = (int16_t)(sr_); \
	})
#endif
#else
	#define OUT(s_) ({ \
		int16_t s = (int16_t)(s_); \
		*out++ = (int16_t)(s); \
	})
#endif

#if 0
// Reference implementation.
// This implementation processes the whole AY8910 state every sample, so it's
// easy to follow but it's not optimized because it doesn't account for the
// fact that the internal state doesn't change every tick.
int ay8910_gen(AY8910 *ay, int16_t *out, int nsamples) {
	if (ay->ch[0].tone_en && ay->ch[1].tone_en && ay->ch[2].tone_en)
	{
		for (int i=0;i<nsamples;i++)
			OUTS(VOL_TABLE[0]);
		return nsamples;
	}

	#if 0
	{	
		AYChannel *ch0 = &ay->ch[0];
		AYChannel *ch1 = &ay->ch[1];
		AYChannel *ch2 = &ay->ch[2];
		AYNoise *ns = &ay->ns;
		AYEnvelope *env = &ay->env;
		printf("en: %d %d %d\n", !ch0->tone_en, !ch1->tone_en, !ch2->tone_en);
		printf("noise: %d %d %d\n", !ch0->noise_en, !ch1->noise_en, !ch2->noise_en);
		printf("vol: %d %d %d %d\n", ch0->tone_vol, ch1->tone_vol, ch2->tone_vol, env->vol);
		printf("chout: %d %d %d\n", ch0->out, ch1->out, ch2->out);
		printf("period: %d %d %d %d %d\n", ch0->tone_period, ch1->tone_period, ch2->tone_period, ns->period, env->period);
	}
	#endif

	float sample = 0;
	int sample_n = 0;
	for (int i=0; i<nsamples*AY8910_DECIMATE; i++) {
		AYNoise *ns = &ay->ns;
		++ns->count;
		if (ns->count >= ns->period) {
			ns->out ^= ((ns->out ^ (ns->out>>3)) & 1) << 17;
			ns->out >>= 1;
			ns->count -= ns->period;
		}
		AYEnvelope *env = &ay->env;
		if (!env->holding) {
			++env->count;
			if (env->count >= env->period) {
				env->count = 0;

				env->step--;
				if (env->step < 0) {
					if (env->hold) {
						if (env->alternate)
							env->attack ^= 0xF;
						env->holding = 1;
						env->step = 0;
					} else {
						if (env->alternate && env->step&0x10)
							env->attack ^= 0xF;
						env->step &= 0xF;
					}
				}
				env->vol = env->step ^ env->attack;
			}
		}

		for (int c=0; c<3; c++) {
			AYChannel *ch = &ay->ch[c];
			++ch->count;
			if (ch->count >= ch->tone_period) {
				ch->out ^= 1;
				ch->count -= ch->tone_period;
			}

			uint8_t gate = (ch->out | ch->tone_en) & (ns->out | ch->noise_en) & 1;
			uint8_t vol = (ch->tone_vol == 0x10) ? env->vol : ch->tone_vol;
			sample += VOL_TABLE[gate ? vol : 0];
		}
		if (++sample_n == AY8910_DECIMATE) {
			OUTS(sample/(3*AY8910_DECIMATE));
			sample_n = 0;
			sample = 0;
		}
	}
	return nsamples;
}

#else

static uint32_t fastrand() {
	/* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
	static int state = 1;
	uint32_t x = state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	return state = x;
}

static float fastrandf() {
	return fastrand() * 2.3283064365386963e-10f;
}

// Optimized implementation, much faster.
// This implementation is more complex compared to the reference once. It
// inspects the internal state of the AY8910 and decides when the next state
// change is going to happen. Then, it emits a fixed output for all the cycles
// until next state change.
int ay8910_gen(AY8910 *ay, int16_t *out, int nsamples) {
	nsamples *= AY8910_DECIMATE;

	int16_t *iout = out;
	#if AY8910_OUTPUT_STEREO
	float sample_accum_l = 0;
	float sample_accum_r = 0;
	#else
	float sample_accum = 0;
	#endif
	int sample_accum_n = 0;
	AYChannel *ch0 = &ay->ch[0];
	AYChannel *ch1 = &ay->ch[1];
	AYChannel *ch2 = &ay->ch[2];
	AYNoise *ns = &ay->ns;
	AYEnvelope *env = &ay->env;
	int noise = ((!ch0->noise_en)<<0) | ((!ch1->noise_en)<<1) | ((!ch2->noise_en)<<2);
	int envelope = ((ch0->tone_vol == 0x10) || (ch1->tone_vol == 0x10) || (ch2->tone_vol == 0x10));
	if (env->holding) envelope = 0;

	float vol0 = VOL_TABLE[(ch0->tone_vol == 0x10) ? env->vol : ch0->tone_vol];
	float vol1 = VOL_TABLE[(ch1->tone_vol == 0x10) ? env->vol : ch1->tone_vol];
	float vol2 = VOL_TABLE[(ch2->tone_vol == 0x10) ? env->vol : ch2->tone_vol];

	// If the period just changed, the counter might have overflown. Just cap next
	// event to the period.
	if (ch0->count > ch0->tone_period) { ch0->count = 0; ch0->out ^= 1; }
	if (ch1->count > ch1->tone_period) { ch1->count = 0; ch1->out ^= 1; }
	if (ch2->count > ch2->tone_period) { ch2->count = 0; ch2->out ^= 1; }
	if (ns->count > ns->period) ns->count = 0;
	if (env->count > env->period) env->count = 0;

	// Calculate when the state will change for the different components of the PSG.
	// Use a very big number for disabled components, so that it will never become 0
	// before nsamples.
	uint32_t n0 = !ch0->tone_en ? (ch0->tone_period - ch0->count) : 0xFFFFFFFF;
	uint32_t n1 = !ch1->tone_en ? (ch1->tone_period - ch1->count) : 0xFFFFFFFF;
	uint32_t n2 = !ch2->tone_en ? (ch2->tone_period - ch2->count) : 0xFFFFFFFF;
	uint32_t nn = noise ?         (ns->period - ns->count)        : 0xFFFFFFFF;
	uint32_t ne = envelope ?      (env->period - env->count)      : 0xFFFFFFFF;
	// Period == 1 is probably just a mistake, the frequency is too high anyway, ignore them
	if (ch0->tone_period == 1) n0 = 0xFFFFFFFF;
	if (ch1->tone_period == 1) n1 = 0xFFFFFFFF;
	if (ch2->tone_period == 1) n2 = 0xFFFFFFFF;
	if (env->period == 1     ) ne = 0xFFFFFFFF;

	// Very low noise periods are instead very common, and it's just high-frequency
	// noise. To avoid being affected too much by performance, when the noise is
	// lower than the decimation factor we switch to random amplitude modulation
	// to emulate the noise. We call this technique "fastnoise".
	int fastnoise = 0;
	if (AY8910_DECIMATE > 1 && ns->period <= AY8910_DECIMATE) {
		ns->period = AY8910_DECIMATE;
		fastnoise = noise;
	}

	// Periods should never be 0 (they're capped to 1 when they're written)
	assert(ch0->tone_period);
	assert(ch1->tone_period);
	assert(ch2->tone_period);
	assert(ns->period);
	assert(env->period);

	// Now that we have setup the next events in the n* variables, we don't need
	// the current value of the counters anymore. Update them to the final value
	// they will have after processing nsamples, ready for next frame.
	// Handle period==1 (silent) specially, so that we save a division in that case.
	ch0->count += nsamples;
	ch1->count += nsamples;
	ch2->count += nsamples;
	ns->count += nsamples;
	env->count += nsamples;
	if (ch0->tone_period == 1) ch0->count = 0; else ch0->count %= ch0->tone_period;
	if (ch1->tone_period == 1) ch1->count = 0; else ch1->count %= ch1->tone_period;
	if (ch2->tone_period == 1) ch2->count = 0; else ch2->count %= ch2->tone_period;
	if (ns->period == 1) ns->count = 0; else ns->count %= ns->period;
	if (env->period == 1) env->count = 0; else env->count %= env->period;

	// If the chip is completely silent, just early exit
	if (!noise && ch0->tone_en && ch1->tone_en && ch2->tone_en) {
		for (int i=0; i<nsamples/AY8910_DECIMATE; i++)
			OUTS(VOL_TABLE[0]);
		return nsamples/AY8910_DECIMATE;
	}

	#if 0
	printf("en: %d %d %d %d\n", !ch0->tone_en, !ch1->tone_en, !ch2->tone_en, envelope);
	printf("noise: %d %d %d\n", !ch0->noise_en, !ch1->noise_en, !ch2->noise_en);
	printf("vol: %d %d %d %d\n", ch0->tone_vol, ch1->tone_vol, ch2->tone_vol, env->vol);
	printf("chout: %d %d %d\n", ch0->out, ch1->out, ch2->out);
	printf("period: %d %d %d %d %d\n", ch0->tone_period, ch1->tone_period, ch2->tone_period, ns->period, env->period);
	printf("envelope: step:%d att:%d hld:%d holding:%d\n", env->step, env->attack, env->hold, env->holding);
	printf("next: %d %d %d %d %d\n", n0, n1, n2, nn, ne);
	#endif

	int changech = 0x7; // recalc the output of all channels once
	float s0=0, s1=0, s2=0;
	float fn0=0, fn1=0, fn2=0;

	while (nsamples > 0) {
		if (changech & (1<<0)) {
			if (fastnoise & (1<<0)) {
				uint8_t gate = (ch0->out | ch0->tone_en) & 1;
				s0 = !gate ? vol0 : VOL_TABLE[0];
				fn0 = !gate ? (vol0-VOL_TABLE[0]) : 0;
			} else {
				uint8_t gate = (ch0->out | ch0->tone_en) & (ns->out | ch0->noise_en) & 1;
				s0 = !gate ? vol0 : VOL_TABLE[0];
				fn0 = 0;
			}
		}
		if (changech & (1<<1)) {
			if (fastnoise & (1<<1)) {
				uint8_t gate = (ch1->out | ch1->tone_en) & 1;
				s1 = !gate ? vol1 : VOL_TABLE[0];
				fn1 = !gate ? (vol1-VOL_TABLE[0]) : 0;
			} else {
				uint8_t gate = (ch1->out | ch1->tone_en) & (ns->out | ch1->noise_en) & 1;
				s1 = !gate ? vol1 : VOL_TABLE[0];
				fn1 = 0;
			}
		}
		if (changech & (1<<2)) {
			if (fastnoise & (1<<2)) {
				uint8_t gate = (ch2->out | ch2->tone_en) & 1;
				s2 = !gate ? vol2 : VOL_TABLE[0];
				fn2 = !gate ? (vol2-VOL_TABLE[0]) : 0;
			} else {
				uint8_t gate = (ch2->out | ch2->tone_en) & (ns->out | ch2->noise_en) & 1;
				s2 = !gate ? vol2 : VOL_TABLE[0];
				fn2 = 0;
			}
		}
		changech = 0;

		void *update_func = 0;

		// Check which internal component is going to change state first in the future.
		uint32_t next = 0xFFFFFFFF;
		if (n0 < next) { next = n0; update_func = &&update_ch0; }
		if (n1 < next) { next = n1; update_func = &&update_ch1; }
		if (n2 < next) { next = n2; update_func = &&update_ch2; }
		if (nn < next) { next = nn; update_func = &&update_noise; }
		if (ne < next) { next = ne; update_func = &&update_envelope; }
		if (nsamples < next) {
			next = nsamples;
			update_func = 0;
		}

		// Next==0 happens only when two components change state at the same tick.
		// In the case, we run the whole loop twice, and the second time one
		// next will be zero. We don't need to generate samples in this case.
		if (next) {
			// Update 
			nsamples -= next;
			n0 -= next;
			n1 -= next;
			n2 -= next;
			nn -= next;
			ne -= next;

			// Output the current sample value until the next state change.
			#if AY8910_OUTPUT_STEREO
			float samplel = SAMPLE_CONV((s0+s1*0.5f) * (2.f / 3.f));
			float sampler = SAMPLE_CONV((s2+s1*0.5f) * (2.f / 3.f));
			#else
			float sample = SAMPLE_CONV((s0+s1+s2) * (1.f / 3.f));
			#endif

			#if 0
			printf("out %04x %d [%d,%d,%d,%d,%d] (%d)\n", (int)(sample*65535), next, n0,n1,n2,nn,ne, nsamples);
			#endif

			if (AY8910_DECIMATE > 1) {
				// Calculate the fast-noise amplitude (if any). A random amplitude
				// in the range [0..fn] must be subtracted from sample to apply the noise.
				#if AY8910_OUTPUT_STEREO
				float fnl = (fn0+fn1*0.5f) * (2.f / 3.f) * 65535.f;
				float fnr = (fn2+fn1*0.5f) * (2.f / 3.f) * 65535.f;
				#else
				float fn = (fn0+fn1+fn2) * (1.f / 3.f) * 65535.f;
				#endif

				if (sample_accum_n) {
					float fr = fastrandf();
					int sa = AY8910_DECIMATE-sample_accum_n;
					if (sa > next) {
						#if AY8910_OUTPUT_STEREO
						sample_accum_l += (samplel - fnl*fr) * next;
						sample_accum_r += (sampler - fnr*fr) * next;
						#else
						sample_accum += (sample - fn*fr) * next;
						#endif
						sample_accum_n += next;
						goto end_decim;
					} else {					
						#if AY8910_OUTPUT_STEREO
						sample_accum_l += (samplel - fnl*fr) * sa;
						sample_accum_r += (sampler - fnr*fr) * sa;
						OUT(sample_accum_l * (1.f / AY8910_DECIMATE), sample_accum_r * (1.f / AY8910_DECIMATE));
						#else
						sample_accum += (sample - fn*fr) * sa;
						OUT(sample_accum * (1.f / AY8910_DECIMATE));
						#endif
						next -= sa;
						sample_accum_n = 0;
					}
				}

				int nn = next / AY8910_DECIMATE;
				if (fastnoise) {
					for (int i=0; i<nn; i++) {
						float fr = fastrandf();
						#if AY8910_OUTPUT_STEREO
						OUT(samplel - fnl*fr, sampler - fnr*fr);
						#else
						OUT(sample - fn*fr);
						#endif
					}
				} else {
					for (int i=0; i<nn; i++) {
						#if AY8910_OUTPUT_STEREO
						OUT(samplel, sampler);
						#else
						OUT(sample);
						#endif
					}
				}

				next -= nn*AY8910_DECIMATE;
				sample_accum_n = next;
				float fr = fastrandf();
				#if AY8910_OUTPUT_STEREO
				sample_accum_l = (samplel - fnl*fr) * next;
				sample_accum_r = (sampler - fnr*fr) * next;
				#else
				sample_accum = (sample - fn*fr) * next;
				#endif
				end_decim: (void)0;

			} else {
				for (int i=0; i<next; i++) {
					#if AY8910_OUTPUT_STEREO
					OUT(samplel, sampler);
					#else
					OUT(sample);
					#endif
				}
			}
		}

		if (!update_func)
			continue;
		goto *update_func;

		update_ch0: {
			ch0->out ^= 1; changech |= (1<<0); n0 = ch0->tone_period;
			continue;
		}
		update_ch1: {
			ch1->out ^= 1; changech |= (1<<1); n1 = ch1->tone_period;
			continue;
		}
		update_ch2: {
			ch2->out ^= 1; changech |= (1<<2); n2 = ch2->tone_period;
			continue;
		}
		update_noise: {
			ns->out ^= ((ns->out ^ (ns->out>>3)) & 1) << 17;
			ns->out >>= 1;
			changech |= noise;
			nn = ns->period;
			continue;
		}
		update_envelope: {
			if (!env->holding) {			
				env->step--;
				if (env->step < 0) {
					if (env->hold) {
						if (env->alternate)
							env->attack ^= 0xF;
						env->holding = 1;
						env->step = 0;
					} else {
						if (env->alternate && env->step&0x10)
							env->attack ^= 0xF;
						env->step &= 0xF;
					}
				}
				env->vol = env->step ^ env->attack;
				ne = env->period;
			} else {
				// No need to process envelope anymore, as it reached the holding state
				ne = 0xFFFFFFFF;
			}

			float v = VOL_TABLE[env->vol];
			if (ch0->tone_vol == 0x10) { vol0 = v; changech |= (1<<0); }
			if (ch1->tone_vol == 0x10) { vol1 = v; changech |= (1<<1); }
			if (ch2->tone_vol == 0x10) { vol2 = v; changech |= (1<<2); }
			ne = env->period;
			continue;
		}
	}

	assert(nsamples == 0);
	return (out-iout) / (AY8910_OUTPUT_STEREO ? 2 : 1);
}
#endif

void ay8910_reset(AY8910 *ay) {
	memset(ay, 0, sizeof(*ay));
	ay->ns.out = 1;
	ay->ns.period = 1;
	ay->env.period = 1;
	ay->ch[0].tone_en = 1;
	ay->ch[1].tone_en = 1;
	ay->ch[2].tone_en = 1;
	ay->ch[0].noise_en = 1;
	ay->ch[1].noise_en = 1;
	ay->ch[2].noise_en = 1;
	ay->ch[0].tone_period = 1;
	ay->ch[1].tone_period = 1;
	ay->ch[2].tone_period = 1;
}

void ay8910_set_ports(AY8910 *ay, uint8_t (*PortRead)(int), void (*PortWrite)(int, uint8_t)) {
	ay->PortRead = PortRead;
	ay->PortWrite = PortWrite;
}

bool ay8910_is_mute(AY8910* ay) {
	AYChannel *ch0 = &ay->ch[0];
	AYChannel *ch1 = &ay->ch[1];
	AYChannel *ch2 = &ay->ch[2];
	return ch0->tone_en && ch0->noise_en && ch1->tone_en && ch1->noise_en && ch2->tone_en && ch2->noise_en;
}

void ay8910_write_addr(AY8910 *ay, uint8_t addr) {
	ay->addr = addr & 0xF;
}

uint8_t ay8910_read_data(AY8910 *ay) {
	switch (ay->addr) {
	case 14:
		if (ay->PortRead) return ay->PortRead(0);
		return 0xff;
	case 15:
		if (ay->PortRead) return ay->PortRead(1);
		return 0xff;
	default:
		return ay->regs[ay->addr];
	}
}

void ay8910_write_data(AY8910 *ay, uint8_t val) {
	static const uint8_t reg_mask[16] = {
		0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, // tone period
		0x1F, // noise period
		0xFF, // enable
		0x1F, 0x1F, 0x1F, // tone volume
		0xFF, 0xFF, // env period
		0x0F, // env shape
		0xFF, 0xFF, // i/o ports
	};

	if ((val & reg_mask[ay->addr]) != val)
		tracef("ay8910: writing unknown bits: 0x%02x <- %02x\n", ay->addr, val);
	val &= reg_mask[ay->addr];
	ay->regs[ay->addr] = val;

	switch (ay->addr) {
	case 0: case 1: case 2: case 3: case 4: case 5: {
		int ch = ay->addr / 2;		
		ay->ch[ch].tone_period = ay->regs[ch*2] | (((uint16_t)ay->regs[ch*2+1] & 0xF) <<8);
		if (ay->ch[ch].tone_period == 0) ay->ch[ch].tone_period = 1;
		tracef("ay8910: tone %d: period=%04x\n", ch, ay->ch[ch].tone_period);
		break;
	}
	case 6: {
		ay->ns.period = val;
		if (ay->ns.period == 0) ay->ns.period = 1;
		tracef("ay8910: noise period=%02x\n", val);
		break;
	}
	case 7: {
		ay->ch[0].tone_en = (val >> 0) & 1;
		ay->ch[1].tone_en = (val >> 1) & 1;
		ay->ch[2].tone_en = (val >> 2) & 1;
		ay->ch[0].noise_en = (val >> 3) & 1;
		ay->ch[1].noise_en = (val >> 4) & 1;
		ay->ch[2].noise_en = (val >> 5) & 1;
		tracef("ay8910: enable: tone[%d,%d,%d] noise[%d,%d,%d]\n",
			!ay->ch[0].tone_en, !ay->ch[1].tone_en, !ay->ch[2].tone_en,
			!ay->ch[0].noise_en, !ay->ch[1].noise_en, !ay->ch[2].noise_en
		);
		if (val & 0xC0) tracef("ay8910: unimplemented I/O ports configured as output\n");
		break;
	}
	case 8: case 9: case 10: {
		int ch = ay->addr - 8;
		ay->ch[ch].tone_vol = val;
		if (val == 0x10)
			tracef("ay8910: tone %d: vol=envelope\n", ch);
		else
			tracef("ay8910: tone %d: vol=%02x\n", ch, ay->ch[ch].tone_vol);
		break;
	}
	case 11: case 12: {
		ay->env.period = (ay->regs[11] | (((uint16_t)ay->regs[12] & 0xF) << 8));
		ay->env.period *= 2; // envelope clocks at half-rate
		if (ay->env.period == 0) ay->env.period = 1;  // period=0 => half-clock of period 1
		tracef("ay8910: tone 0: env_period=%04x\n", ay->env.period/2);
		break;
	}
	case 13: {
		ay->env.attack = (val & 4) ? 0xF : 0x0;
		if (val & 8) {
			ay->env.hold = val & 1;
			ay->env.alternate = (val & 2) ? 1 : 0;
		} else {
			ay->env.hold = 1;
			ay->env.alternate = ay->env.attack ? 1 : 0;	
		}
		ay->env.step = 0xF;
		ay->env.holding = 0;
		ay->env.vol = ay->env.step ^ ay->env.attack;

		tracef("ay8910: envelope: shape=%x (attack=%x alt=%d hold=%d)\n", val, ay->env.attack, ay->env.alternate, ay->env.hold);
		break;
	}
	default:
		tracef("ay8910: unimplemented register write: 0x%x <- %02x\n", ay->addr, val);
	}
}
