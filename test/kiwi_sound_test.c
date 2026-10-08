
/* Real mixer regression and Kiwi v2 integration probe.
 * Run with an installed Rance03 data directory; no game VM is executed.
 * The test includes the mixer to inspect sample-accurate loop/pause state.
 */
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "system4/file.h"
#include "../src/audio_mixer.c"
#include "../src/hll/KiwiSoundEngine.c"

struct config config = { .default_volume = 100 };
struct ain *ain;

const char *display_utf0(const char *s) { return s; }
char *gamedir_path(const char *s) { return path_join(config.game_dir, s); }

void static_library_replace(struct static_library *lib, const char *name, void *fun)
{
	for (int i = 0; lib->functions[i].name; i++) {
		if (!strcmp(lib->functions[i].name, name)) {
			lib->functions[i].fun = fun;
			return;
		}
	}
	assert(!"unknown HLL export");
}

static void test_stereo_loop(void)
{
	SF_INFO info = { .samplerate = 44100, .channels = 2,
		.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT };
	FILE *tmp = tmpfile();
	assert(tmp);
	SNDFILE *file = sf_open_fd(fileno(tmp), SFM_WRITE, &info, false);
	assert(file);
	float samples[20];
	for (int i = 0; i < 10; i++) {
		samples[2*i] = i / 20.0f;
		samples[2*i+1] = (10+i) / 20.0f;
	}
	assert(sf_writef_float(file, samples, 10) == 10);
	sf_close(file);
	rewind(tmp);
	struct channel *ch = xcalloc(1, sizeof(*ch));
	ch->file = sf_open_fd(fileno(tmp), SFM_READ, &ch->info, false);
	assert(ch->file && init_channel(ch));
	ch->loop_start = 2;
	ch->loop_end = 5;
	ch->loop_count = 2;
	assert(cb_seek(ch, 3));
	float output[8] = {0};
	uint_least32_t read;
	assert(cb_read_frames(ch, output, 4, &read) == STS_STREAM_CONTINUE);
	assert(read == 4);
	const int frames[] = { 3, 4, 2, 3 };
	for (int i = 0; i < 4; i++) {
		assert(fabsf(output[2*i] - samples[2*frames[i]]) < 1e-6);
		assert(fabsf(output[2*i+1] - samples[2*frames[i]+1]) < 1e-6);
	}
	sf_close(ch->file);
	fclose(tmp);
	free(ch);
	puts("PASS stereo loop boundary preserves both channels");
}

static void test_final_buffer(void)
{
	SF_INFO info = { .samplerate = 44100, .channels = 2,
		.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT };
	FILE *tmp = tmpfile();
	assert(tmp);
	SNDFILE *file = sf_open_fd(fileno(tmp), SFM_WRITE, &info, false);
	assert(file);
	float samples[14];
	for (int i = 0; i < 14; i++)
		samples[i] = 0.1f + i / 100.0f;
	sf_writef_float(file, samples, 7);
	sf_close(file);
	rewind(tmp);
	struct channel *ch = xcalloc(1, sizeof(*ch));
	ch->file = sf_open_fd(fileno(tmp), SFM_READ, &ch->info, false);
	assert(ch->file && init_channel(ch));
	// Inspect the actual stream callback with SDL's mixer callback excluded.
	SDL_LockAudioDevice(audio_device);
	ch->voice = sts_mixer_play_stream(&mixers[0].mixer, &ch->stream, 1.0f);
	assert(ch->voice >= 0);
	assert(refill_stream(&ch->stream.sample, ch) == STS_STREAM_CONTINUE);
	assert(ch->stream.sample.length == 14);
	assert(channel_is_playing(ch));
	for (int i = 0; i < 14; i++)
		assert(fabsf(ch->data[i] - samples[i]) < 1e-6);
	int voice = ch->voice;
	assert(refill_stream(&ch->stream.sample, ch) == STS_STREAM_COMPLETE);
	assert(!channel_is_playing(ch));
	sts_mixer_stop_voice(&mixers[0].mixer, voice);
	SDL_UnlockAudioDevice(audio_device);
	sf_close(ch->file);
	fclose(tmp);
	free(ch);
	puts("PASS final partial PCM buffer drains before IsPlay becomes false");
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s /path/to/Rance03\n", argv[0]);
		return 2;
	}
	setvbuf(stdout, NULL, _IONBF, 0);
	assert(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER) == 0);
	config.game_dir = xstrdup(argv[1]);
	config.ain_filename = xstrdup("Rance03.ain");
	char *ain_path = gamedir_path(config.ain_filename);
	int error;
	ain = ain_open(ain_path, &error);
	free(ain_path);
	assert(ain);
	KiwiSoundEngine_PreLink();
	asset_manager_init();
	KiwiSoundEngine_ModuleInit();
	assert(audio_device);

	int library = ain_get_library(ain, "KiwiSoundEngine");
	for (int i = 0; i < ain->libraries[library].nr_functions; i++) {
		const char *name = ain->libraries[library].functions[i].name;
		bool found = false;
		for (int j = 0; lib_KiwiSoundEngine.functions[j].name; j++)
			if (!strcmp(name, lib_KiwiSoundEngine.functions[j].name))
				found = lib_KiwiSoundEngine.functions[j].fun != NULL;
		assert(found);
	}
	puts("PASS all 32 actual AIN Kiwi HLL signatures have callable exports");
	test_stereo_loop();
	test_final_buffer();

	struct archive_data *map = asset_get_by_name(ASSET_SOUND, "timemap_sound2", NULL);
	assert(map);
	struct ex *metadata = ex_read(map->data, map->size);
	archive_free_data(map);
	struct ex_table *table = ex_get_table(metadata, "SoundInfo");
	assert(table && table->nr_rows > 0);
	struct string *music = NULL, *effect = NULL;
	for (unsigned i = 0; i < table->nr_rows; i++) {
		if (!music && table->rows[i][1].i == 1)
			music = string_ref(table->rows[i][0].s);
		if (!effect && table->rows[i][1].i == 2)
			effect = string_ref(table->rows[i][0].s);
	}
	assert(music && effect);
	struct string *missing = make_string("__kiwi_missing_audio__", 22);
	assert(!KiwiSoundEngine_IsExistFile(missing));
	assert(!KiwiSoundEngine_Prepare(99, missing, true));
	assert(!KiwiSoundEngine_Play(99) && !KiwiSoundEngine_IsPlay(99));
	assert(KiwiSoundEngine_IsExistFile(music) && KiwiSoundEngine_IsExistFile(effect));
	assert(KiwiSoundEngine_Prepare(1, music, true));
	assert(KiwiSoundEngine_Prepare(3, effect, false));
	assert(KiwiSoundEngine_GetGroupNum(1) == 1 && KiwiSoundEngine_GetGroupNum(3) == 2);
	assert(KiwiSoundEngine_GetGroupNumFromFile(music) == 1);
	assert(KiwiSoundEngine_GetGroupNumFromFile(effect) == 2);
	assert(KiwiSoundEngine_GetGroupNumFromFile(missing) == -1);
	assert(KiwiSoundEngine_GetFreeID(1, 3) == 2);
	assert(KiwiSoundEngine_GetLengthFromFile(music) == KiwiSoundEngine_GetLength(1));
	struct kiwi_sound *sound = id_pool_get(&sounds, 1);
	int loop_end = sound->channel->loop_end;
	int loop_start = sound->channel->loop_start;
	printf("Real SoundInfo music loop: start=%d end=%d length_ms=%d\n",
		loop_start, loop_end, KiwiSoundEngine_GetLength(1));
	assert(loop_start > 0 && loop_end > loop_start);
	assert(KiwiSoundEngine_SetLoopCount(1, 0));
	assert(KiwiSoundEngine_Seek(1, (int64_t)(loop_end - 4410) * 1000 / 44100));
	uint_least32_t previous_frame = sound->channel->frame;
	assert(KiwiSoundEngine_Play(1));
	assert(KiwiSoundEngine_Play(3));
	Uint32 wait_start = SDL_GetTicks();
	bool wrapped = false;
	while (SDL_GetTicks() - wait_start < 5000) {
		assert(KiwiSoundEngine_IsPlay(1));
		uint_least32_t frame = sound->channel->frame;
		if (frame < previous_frame) {
			assert(frame >= (unsigned)loop_start && frame < (unsigned)loop_end);
			printf("PASS actual Ogg music loop observed: sample=%u -> %u elapsed_ms=%u\n",
				previous_frame, frame, SDL_GetTicks() - wait_start);
			wrapped = true;
			break;
		}
		previous_frame = frame;
		SDL_Delay(10);
	}
	if (!wrapped)
		fprintf(stderr, "Music did not wrap within 5000 ms: sample=%u loop=%d..%d\n",
			previous_frame, loop_start, loop_end);
	assert(wrapped);

	assert(KiwiSoundEngine_Pause(1) && KiwiSoundEngine_IsPause(1));
	int pos = KiwiSoundEngine_GetPos(1);
	SDL_Delay(300);
	assert(KiwiSoundEngine_GetPos(1) == pos);
	assert(KiwiSoundEngine_Restart(1) && !KiwiSoundEngine_IsPause(1));
	SDL_Delay(400);
	assert(KiwiSoundEngine_GetPos(1) > pos);
	puts("PASS pause holds sample position; restart resumes real mixing");
	assert(KiwiSoundEngine_Fade(1, 120, 0.0f, true, 0));
	SDL_Delay(700);
	assert(!KiwiSoundEngine_IsPlay(1));
	assert(KiwiSoundEngine_Stop(3) && KiwiSoundEngine_Unprepare(3));
	puts("PASS timed fade completes and stops music");

	assert(KiwiSoundEngine_SetSeParam(5, 1000, true));
	assert(KiwiSoundEngine_IsExistSeID(5));
	assert(KiwiSoundEngine_PlaySe(5, effect));
	assert(!KiwiSoundEngine_PlaySe(5, effect));
	assert(KiwiSoundEngine_IsPlaySe(5));
	assert(KiwiSoundEngine_StopSe(5) && !KiwiSoundEngine_IsPlaySe(5));
	assert(KiwiSoundEngine_SetSeParam(6, 0, true));
	assert(KiwiSoundEngine_PlaySe(6, effect));
	assert(KiwiSoundEngine_PlaySe(6, effect));
	struct kiwi_se *se = id_pool_get(&sound_effects, 6);
	assert(se->voices && se->voices->next);
	assert(KiwiSoundEngine_GetFreeSeID(5, 8) == 7);
	assert(KiwiSoundEngine_StopSe(6));
	assert(KiwiSoundEngine_SetSeParam(6, 0, false));
	assert(KiwiSoundEngine_PlaySe(6, effect));
	assert(KiwiSoundEngine_PlaySe(6, effect));
	assert(se->voices && !se->voices->next);
	SDL_Delay(400);
	assert(KiwiSoundEngine_StopSe(6));
	puts("PASS SE rate limit, concurrent voices and replacement semantics");
	free_string(missing);
	free_string(music);
	free_string(effect);
	ex_free(metadata);
	KiwiSoundEngine_ModuleFini();
	// SDL callback can continue mixing empty channels until SDL_Quit joins it.
	SDL_Quit();
	ain_free(ain);
	puts("PASS Kiwi real archive/mixer integration");
	return 0;
}

