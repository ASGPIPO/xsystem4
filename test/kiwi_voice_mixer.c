/* Focused Kiwi voice metadata and real mixer gain regression. */
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
	for (int i = 0; lib->functions[i].name; i++)
		if (!strcmp(lib->functions[i].name, name)) {
			lib->functions[i].fun = fun;
			return;
		}
	assert(!"unknown HLL export");
}
static int failures;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL line%d: %s\n", __LINE__, #expr); failures++; } } while (0)

static float sample_group(int group)
{
	SF_INFO info = { .samplerate = 44100, .channels = 2,
		.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT };
	FILE *tmp = tmpfile();
	assert(tmp);
	SNDFILE *file = sf_open_fd(fileno(tmp), SFM_WRITE, &info, false);
	assert(file);
	float input[CHUNK_SIZE * 2];
	for (unsigned i = 0; i < sizeof(input) / sizeof(*input); i++)
		input[i] = .5f;
	assert(sf_writef_float(file, input, CHUNK_SIZE) == CHUNK_SIZE);
	sf_close(file);
	rewind(tmp);
	struct channel *ch = xcalloc(1, sizeof(*ch));
	ch->file = sf_open_fd(fileno(tmp), SFM_READ, &ch->info, false);
	assert(ch->file && init_channel(ch));
	assert(channel_set_group(ch, group));
	assert(channel_set_loop_count(ch, 0) && channel_play(ch));
	float output[CHUNK_SIZE * 2];
	/* Drain cached hierarchy buffers before comparing steady PCM. */
	for (int i = 0; i < 5; i++)
		audio_callback(NULL, (Uint8 *)output, sizeof(output));
	float value = output[100];
	channel_close(ch);
	fclose(tmp);
	/* Drain the stopped channel's remaining cached samples. */
	for (int i = 0; i < 5; i++)
		audio_callback(NULL, (Uint8 *)output, sizeof(output));
	return value;
}

int main(int argc, char **argv)
{
	assert(argc == 2 || argc == 3);
	if (argc == 3) {
		ain = ain_new(12, 0);
		int lib = ain_add_library(ain, "KiwiSoundEngine");
		ain->libraries[lib].nr_functions = 1;
		ain->libraries[lib].functions = xcalloc(1, sizeof(struct ain_hll_function));
		ain->libraries[lib].functions[0].name = xstrdup("Prepare");
		if (!strcmp(argv[2], "explicit")) {
			config.mixer_nr_channels = 1;
			config.mixer_channels = xcalloc(1, sizeof(char *));
			config.mixer_channels[0] = xstrdup("Custom");
			config.mixer_volumes = xcalloc(1, sizeof(int));
			config.mixer_volumes[0] = 73;
		}
		KiwiSoundEngine_PreLink();
		if (!strcmp(argv[2], "explicit"))
			assert(config.mixer_nr_channels == 1 && config.mixer_volumes[0] == 73);
		else
			assert(config.mixer_nr_channels == 6);
		puts("PASS existing six-group and explicit mixer defaults preserved");
		return 0;
	}
	setvbuf(stdout, NULL, _IONBF, 0);
	config.game_dir = xstrdup(argv[1]);
	config.ain_filename = xstrdup("Rance03.ain");
	char *path = gamedir_path(config.ain_filename);
	int error;
	ain = ain_open(path, &error);
	assert(ain);
	free(path);
	KiwiSoundEngine_PreLink();
	asset_manager_init();
	KiwiSoundEngine_ModuleInit();
	SDL_PauseAudioDevice(audio_device, 1);
	printf("real game mixer count=%u\n", config.mixer_nr_channels);
	int volume = -1;
	CHECK(mixer_get_volume(6, &volume) && volume == 100);
	CHECK(mixer_get_volume(7, &volume) && volume == 100);
	CHECK(mixer_set_volume(6, 50) && mixer_get_volume(6, &volume) && volume == 50);
	CHECK(mixer_set_volume(7, 25) && mixer_get_volume(7, &volume) && volume == 25);
	CHECK(!mixer_get_volume(-1, &volume) && !mixer_set_volume(8, 50));

	/* The installed Voice archive uses the legacy map name. No voice text is printed. */
	struct archive_data *data = asset_get_by_name(ASSET_VOICE, "timemap_sound", NULL);
	assert(data);
	struct ex *metadata = ex_read(data->data, data->size);
	archive_free_data(data);
	struct ex_table *table = ex_get_table(metadata, "SoundInfo");
	assert(table && table->nr_columns == 4);
	unsigned counts[2] = {0};
	struct string *names[2] = {0};
	for (unsigned r = 0; r < table->nr_rows; r++) {
		int g = table->rows[r][1].i;
		assert(g == 6 || g == 7);
		counts[g-6]++;
		if (!names[g-6])
			names[g-6] = string_ref(table->rows[r][0].s);
	}
	printf("actual Voice archive groups6=%u groups7=%u\n", counts[0], counts[1]);
	for (int i = 0; i < 2; i++) {
		CHECK(KiwiSoundEngine_GetGroupNumFromFile(names[i]) == i + 6);
		bool prepared = KiwiSoundEngine_Prepare(12, names[i], false);
		CHECK(prepared);
		if (prepared) {
			CHECK(KiwiSoundEngine_GetGroupNum(12) == i + 6);
			CHECK(KiwiSoundEngine_Play(12));
			float output[CHUNK_SIZE * 2];
			float peak = 0;
			for (int n = 0; n < 16; n++) {
				audio_callback(NULL, (Uint8 *)output, sizeof(output));
				for (unsigned s = 0; s < sizeof(output)/sizeof(*output); s++)
					peak = fmaxf(peak, fabsf(output[s]));
			}
			CHECK(peak > 0);
			printf("actual prepared voice group=%d decoded PCM peak=%f\n",
				KiwiSoundEngine_GetGroupNum(12), peak);
			CHECK(KiwiSoundEngine_Unprepare(12));
			for (int n = 0; n < 5; n++)
				audio_callback(NULL, (Uint8 *)output, sizeof(output));
		}
		free_string(names[i]);
	}
	ex_free(metadata);
	if (config.mixer_nr_channels >= 8) {
		CHECK(mixers[6].parent == &mixers[3] && mixers[7].parent == &mixers[3]);
		CHECK(mixers[3].parent == master);
		CHECK(mixer_set_volume(3, 50) && mixer_set_volume(0, 50));
		float male = sample_group(6), female = sample_group(7), music = sample_group(1);
		printf("real PCM gains male=%f female=%f music=%f\n", male, female, music);
		CHECK(fabsf(male - .0625f) < 1e-5);
		CHECK(fabsf(female - .03125f) < 1e-5);
		CHECK(fabsf(music - .25f) < 1e-5);
		CHECK(mixer_set_mute(3, 1));
		CHECK(sample_group(6) == 0 && sample_group(7) == 0);
		CHECK(fabsf(sample_group(1) - .25f) < 1e-5);
		CHECK(mixer_set_mute(3, 0) && mixer_set_mute(6, 1));
		CHECK(sample_group(6) == 0 && fabsf(sample_group(7) - .03125f) < 1e-5);
		CHECK(mixer_set_mute(0, 1) && sample_group(7) == 0 && sample_group(1) == 0);
	}
	KiwiSoundEngine_ModuleFini();
	SDL_Quit();
	ain_free(ain);
	printf("voice regression failures=%d\n", failures);
	return failures ? 1 : 0;
}
