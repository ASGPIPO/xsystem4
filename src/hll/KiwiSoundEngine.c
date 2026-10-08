/* Copyright (C) 2021 Nunuhara Cabbage <nunuhara@haniwa.technology>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL.h>

#include "system4.h"
#include "system4/ain.h"
#include "system4/archive.h"
#include "system4/ex.h"
#include "system4/string.h"

#include "vm.h"
#include "hll.h"
#include "asset_manager.h"
#include "audio.h"
#include "id_pool.h"
#include "mixer.h"
#include "xsystem4.h"

HLL_WARN_UNIMPLEMENTED( , void, KiwiSoundEngine, SetGlobalFocus, possibly_unused int nNum);

//int KiwiSoundEngine_Sound_GetGroupNum(int nCh);
//static bool KiwiSoundEngine_Sound_PrepareFromFile(int ch, struct string *filename);

static int KiwiSoundEngine_GetMasterGroup(void)
{
	return 0;
}

static int KiwiSoundEngine_GetBGMGroup(void)
{
	return 0;
}

static int KiwiSoundEngine_GetSEGroup(void)
{
	return 0;
}

static int KiwiSoundEngine_GetVoiceGroup(void)
{
	return 0;
}

static int KiwiSoundEngine_GetGimicSEGroup(void)
{
	return 0;
}

static int KiwiSoundEngine_GetBackVoiceGroup(void)
{
	return 0;
}

static bool rance9_bgm_exists(int num)
{
	char name[16];
	snprintf(name, sizeof(name), "%05d", num);
	return asset_exists_by_name(ASSET_BGM, name, NULL);
}

static int rance9_bgm_prepare(int ch, int num)
{
	char name[16];
	snprintf(name, sizeof(name), "%05d", num);
	struct archive_data *dfile = asset_get_by_name(ASSET_BGM, name, NULL);
	if (!dfile)
		return 0;
	return bgm_prepare_from_archive_data(ch, num, dfile);
}

static bool rance9_wav_exists(int num)
{
	char name[16];
	snprintf(name, sizeof(name), "%05d", num);
	return asset_exists_by_name(ASSET_SOUND, name, NULL);
}

static int rance9_wav_prepare(int ch, int num)
{
	char name[16];
	snprintf(name, sizeof(name), "%05d", num);
	struct archive_data *dfile = asset_get_by_name(ASSET_SOUND, name, NULL);
	if (!dfile)
		return 0;
	return wav_prepare_from_archive_data(ch, num, dfile);
}


/* Kiwi v2 uses one ID namespace for music, effects and voices. */
struct kiwi_sound {
	struct channel *channel;
	int group;
};

struct kiwi_se_voice {
	struct channel *channel;
	struct kiwi_se_voice *next;
};

struct kiwi_se {
	int play_span;
	bool multi_play;
	bool has_played;
	Uint32 last_play;
	struct kiwi_se_voice *voices;
};

static struct id_pool sounds;
static struct id_pool sound_effects;
static struct ex *sound_metadata;
static struct ex *voice_metadata;
static bool kiwi_initialized;

static void KiwiSoundEngine_ModuleInit(void)
{
	if (kiwi_initialized)
		return;
	id_pool_init(&sounds, 0);
	id_pool_init(&sound_effects, 0);
	audio_init();
	kiwi_initialized = true;
}

static void close_se_voices(struct kiwi_se *se, bool all)
{
	struct kiwi_se_voice **p = &se->voices;
	while (*p) {
		struct kiwi_se_voice *v = *p;
		if (all || !channel_is_playing(v->channel)) {
			*p = v->next;
			channel_close(v->channel);
			free(v);
		} else {
			p = &v->next;
		}
	}
}

static void KiwiSoundEngine_ModuleFini(void)
{
	if (!kiwi_initialized)
		return;
	for (int id = id_pool_get_first(&sounds); id >= 0; id = id_pool_get_next(&sounds, id)) {
		struct kiwi_sound *sound = id_pool_get(&sounds, id);
		channel_close(sound->channel);
		free(sound);
	}
	for (int id = id_pool_get_first(&sound_effects); id >= 0; id = id_pool_get_next(&sound_effects, id)) {
		struct kiwi_se *se = id_pool_get(&sound_effects, id);
		close_se_voices(se, true);
		free(se);
	}
	id_pool_delete(&sounds);
	id_pool_delete(&sound_effects);
	if (sound_metadata)
		ex_free(sound_metadata);
	if (voice_metadata)
		ex_free(voice_metadata);
	sound_metadata = voice_metadata = NULL;
	kiwi_initialized = false;
}

static struct archive_data *find_sound(const char *name, enum asset_type *type_out)
{
	static const enum asset_type types[] = { ASSET_SOUND, ASSET_BGM, ASSET_VOICE };
	for (unsigned i = 0; i < sizeof(types) / sizeof(*types); i++) {
		struct archive_data *data = asset_get_by_name(types[i], name, NULL);
		if (data) {
			*type_out = types[i];
			return data;
		}
	}
	return NULL;
}

static struct ex_value *find_sound_metadata(enum asset_type type, const char *name)
{
	struct ex **metadata = type == ASSET_VOICE ? &voice_metadata : &sound_metadata;
	if (!*metadata) {
		const char *map = type == ASSET_VOICE ? "timemap_voice2" : "timemap_sound2";
		struct archive_data *data = asset_get_by_name(type, map, NULL);
		if (!data)
			return NULL;
		*metadata = ex_read(data->data, data->size);
		archive_free_data(data);
	}
	struct ex_table *table = ex_get_table(*metadata, "SoundInfo");
	if (!table || table->nr_columns < 4)
		return NULL;
	char *basename = archive_basename(name);
	struct ex_value *row = NULL;
	for (unsigned i = 0; i < table->nr_rows; i++) {
		struct ex_value *candidate = table->rows[i];
		if (candidate[0].type != EX_STRING || candidate[1].type != EX_INT
				|| candidate[2].type != EX_INT || candidate[3].type != EX_INT)
			continue;
		char *key = archive_basename(candidate[0].s->text);
		bool match = !strcmp(basename, key);
		free(key);
		if (match) {
			row = candidate;
			break;
		}
	}
	free(basename);
	return row;
}

static int sound_group(enum asset_type type, struct ex_value *metadata)
{
	if (metadata)
		return metadata[1].i;
	return type == ASSET_VOICE ? 3 : type == ASSET_BGM ? 1 : 2;
}

static struct channel *open_sound(const char *name, int *group_out)
{
	enum asset_type type;
	struct archive_data *data = find_sound(name, &type);
	if (!data)
		return NULL;
	struct ex_value *metadata = find_sound_metadata(type, name);
	int group = sound_group(type, metadata);
	struct channel *ch = channel_open_archive_data(data, type, AUDIO_NO_METADATA);
	if (!ch)
		return NULL;
	if (!channel_set_group(ch, group)) {
		channel_close(ch);
		return NULL;
	}
	if (metadata) {
		int length = channel_get_sample_length(ch);
		int start = max(0, min(length, metadata[2].i));
		int end = max(0, min(length, metadata[3].i));
		if (start < end) {
			channel_set_loop_start_pos(ch, start);
			channel_set_loop_end_pos(ch, end);
		}
	}
	if (group_out)
		*group_out = group;
	return ch;
}

static bool KiwiSoundEngine_Prepare(int id, struct string *name, possibly_unused bool streaming)
{
	if (id < 0)
		return false;
	int group;
	struct channel *ch = open_sound(name->text, &group);
	if (!ch)
		return false;
	struct kiwi_sound *sound = xmalloc(sizeof(*sound));
	*sound = (struct kiwi_sound) { .channel = ch, .group = group };
	struct kiwi_sound *old = id_pool_set(&sounds, id, sound);
	if (old) {
		channel_close(old->channel);
		free(old);
	}
	return true;
}

static bool KiwiSoundEngine_Unprepare(int id)
{
	struct kiwi_sound *sound = id_pool_release(&sounds, id);
	if (!sound)
		return false;
	channel_close(sound->channel);
	free(sound);
	return true;
}

#define KIWI_CHANNEL_WRAPPER(name, ret, expression, fallback) \
	static ret KiwiSoundEngine_##name(int id) { \
		struct kiwi_sound *sound = id_pool_get(&sounds, id); \
		return sound ? (expression) : (fallback); \
	}

KIWI_CHANNEL_WRAPPER(Play, bool, channel_play(sound->channel), false)
KIWI_CHANNEL_WRAPPER(Stop, bool, channel_stop(sound->channel), false)
KIWI_CHANNEL_WRAPPER(IsPlay, bool, channel_is_playing(sound->channel), false)
KIWI_CHANNEL_WRAPPER(GetLoopCount, int, channel_get_loop_count(sound->channel), 0)
KIWI_CHANNEL_WRAPPER(Pause, bool, channel_pause(sound->channel), false)
KIWI_CHANNEL_WRAPPER(Restart, bool, channel_restart(sound->channel), false)
KIWI_CHANNEL_WRAPPER(IsPause, bool, channel_is_paused(sound->channel), false)
KIWI_CHANNEL_WRAPPER(StopFade, bool, channel_stop_fade(sound->channel), false)
KIWI_CHANNEL_WRAPPER(IsFade, bool, channel_is_fading(sound->channel), false)
KIWI_CHANNEL_WRAPPER(GetPos, int, channel_get_pos(sound->channel), 0)
KIWI_CHANNEL_WRAPPER(GetLength, int, channel_get_length(sound->channel), 0)
KIWI_CHANNEL_WRAPPER(GetGroupNum, int, sound->group, -1)
#undef KIWI_CHANNEL_WRAPPER

static bool KiwiSoundEngine_SetLoopCount(int id, int count)
{
	struct kiwi_sound *sound = id_pool_get(&sounds, id);
	return sound && count >= 0 && channel_set_loop_count(sound->channel, count);
}

static bool KiwiSoundEngine_Seek(int id, int pos)
{
	struct kiwi_sound *sound = id_pool_get(&sounds, id);
	return sound && pos >= 0 && channel_seek(sound->channel, pos);
}

static bool KiwiSoundEngine_Fade(int id, int time, float volume, bool stop, int fade_type)
{
	struct kiwi_sound *sound = id_pool_get(&sounds, id);
	if (!sound || time < 0 || !isfinite(volume))
		return false;
	if (fade_type != 0)
		WARNING("KiwiSoundEngine: fade curve %d uses linear interpolation", fade_type);
	return channel_fade(sound->channel, time, lroundf(max(0.0f, min(1.0f, volume)) * 100.0f), stop);
}

static bool KiwiSoundEngine_IsExistID(int id)
{
	return id_pool_get(&sounds, id) != NULL;
}

static int KiwiSoundEngine_GetFreeID(int min_id, int max_id)
{
	if (min_id < 0 || max_id < min_id)
		return -1;
	for (int id = min_id;; id++) {
		if (!KiwiSoundEngine_IsExistID(id))
			return id;
		if (id == max_id)
			return -1;
	}
}

static bool KiwiSoundEngine_IsExistFile(struct string *name)
{
	return asset_exists_by_name(ASSET_SOUND, name->text, NULL)
		|| asset_exists_by_name(ASSET_BGM, name->text, NULL)
		|| asset_exists_by_name(ASSET_VOICE, name->text, NULL);
}

static int KiwiSoundEngine_GetLengthFromFile(struct string *name)
{
	struct channel *ch = open_sound(name->text, NULL);
	if (!ch)
		return 0;
	int length = channel_get_length(ch);
	channel_close(ch);
	return length;
}

static int KiwiSoundEngine_GetGroupNumFromFile(struct string *name)
{
	static const enum asset_type types[] = { ASSET_SOUND, ASSET_BGM, ASSET_VOICE };
	for (unsigned i = 0; i < sizeof(types) / sizeof(*types); i++) {
		if (asset_exists_by_name(types[i], name->text, NULL))
			return sound_group(types[i], find_sound_metadata(types[i], name->text));
	}
	return -1;
}

static float KiwiSoundEngine_GetGroupVolume(int group)
{
	int volume;
	return mixer_get_volume(group, &volume) ? volume / 100.0f : 0.0f;
}

static int KiwiSoundEngine_MillisecondsToSamples(int ms, int rate)
{
	return (int64_t)ms * rate / 1000;
}

static int KiwiSoundEngine_GetSoundFileName(int num, struct string **name)
{
	if (num < 0)
		return 0;
	struct archive_data *data = asset_get(ASSET_SOUND, num + 1);
	if (!data)
		return 0;
	free_string(*name);
	*name = make_string(data->name, strlen(data->name));
	archive_free_data(data);
	return 1;
}

static bool KiwiSoundEngine_SetSeParam(int id, int play_span, bool multi_play)
{
	if (id < 0 || play_span < 0)
		return false;
	struct kiwi_se *se = id_pool_get(&sound_effects, id);
	if (!se) {
		se = xcalloc(1, sizeof(*se));
		id_pool_set(&sound_effects, id, se);
	}
	se->play_span = play_span;
	se->multi_play = multi_play;
	return true;
}

static bool KiwiSoundEngine_IsExistSeID(int id)
{
	return id_pool_get(&sound_effects, id) != NULL;
}

static int KiwiSoundEngine_GetFreeSeID(int min_id, int max_id)
{
	if (min_id < 0 || max_id < min_id)
		return -1;
	for (int id = min_id;; id++) {
		if (!KiwiSoundEngine_IsExistSeID(id))
			return id;
		if (id == max_id)
			return -1;
	}
}

static bool KiwiSoundEngine_PlaySe(int id, struct string *name)
{
	if (id < 0)
		return false;
	struct kiwi_se *se = id_pool_get(&sound_effects, id);
	if (!se) {
		KiwiSoundEngine_SetSeParam(id, 0, true);
		se = id_pool_get(&sound_effects, id);
	}
	close_se_voices(se, false);
	Uint32 now = SDL_GetTicks();
	if (se->has_played && now - se->last_play < (Uint32)se->play_span)
		return false;
	struct channel *ch = open_sound(name->text, NULL);
	if (!ch)
		return false;
	if (!se->multi_play)
		close_se_voices(se, true);
	if (!channel_play(ch)) {
		channel_close(ch);
		return false;
	}
	struct kiwi_se_voice *v = xmalloc(sizeof(*v));
	*v = (struct kiwi_se_voice) { .channel = ch, .next = se->voices };
	se->voices = v;
	se->has_played = true;
	se->last_play = now;
	return true;
}

static bool KiwiSoundEngine_StopSe(int id)
{
	struct kiwi_se *se = id_pool_get(&sound_effects, id);
	if (!se)
		return false;
	close_se_voices(se, true);
	return true;
}

static bool KiwiSoundEngine_IsPlaySe(int id)
{
	struct kiwi_se *se = id_pool_get(&sound_effects, id);
	if (!se)
		return false;
	close_se_voices(se, false);
	return se->voices != NULL;
}

static void KiwiSoundEngine_PreLink(void);

HLL_LIBRARY(KiwiSoundEngine,
	    HLL_EXPORT(_ModuleInit, KiwiSoundEngine_ModuleInit),
	    HLL_EXPORT(_ModuleFini, KiwiSoundEngine_ModuleFini),
	    HLL_EXPORT(GetFreeSeID, KiwiSoundEngine_GetFreeSeID),
	    HLL_EXPORT(IsExistSeID, KiwiSoundEngine_IsExistSeID),
	    HLL_EXPORT(SetSeParam, KiwiSoundEngine_SetSeParam),
	    HLL_EXPORT(PlaySe, KiwiSoundEngine_PlaySe),
	    HLL_EXPORT(StopSe, KiwiSoundEngine_StopSe),
	    HLL_EXPORT(IsPlaySe, KiwiSoundEngine_IsPlaySe),
	    HLL_EXPORT(GetFreeID, KiwiSoundEngine_GetFreeID),
	    HLL_EXPORT(IsExistID, KiwiSoundEngine_IsExistID),
	    HLL_EXPORT(Prepare, KiwiSoundEngine_Prepare),
	    HLL_EXPORT(Unprepare, KiwiSoundEngine_Unprepare),
	    HLL_EXPORT(Play, KiwiSoundEngine_Play),
	    HLL_EXPORT(Stop, KiwiSoundEngine_Stop),
	    HLL_EXPORT(IsPlay, KiwiSoundEngine_IsPlay),
	    HLL_EXPORT(SetLoopCount, KiwiSoundEngine_SetLoopCount),
	    HLL_EXPORT(GetLoopCount, KiwiSoundEngine_GetLoopCount),
	    HLL_EXPORT(Pause, KiwiSoundEngine_Pause),
	    HLL_EXPORT(Restart, KiwiSoundEngine_Restart),
	    HLL_EXPORT(IsPause, KiwiSoundEngine_IsPause),
	    HLL_EXPORT(Fade, KiwiSoundEngine_Fade),
	    HLL_EXPORT(StopFade, KiwiSoundEngine_StopFade),
	    HLL_EXPORT(IsFade, KiwiSoundEngine_IsFade),
	    HLL_EXPORT(Seek, KiwiSoundEngine_Seek),
	    HLL_EXPORT(GetPos, KiwiSoundEngine_GetPos),
	    HLL_EXPORT(GetLength, KiwiSoundEngine_GetLength),
	    HLL_EXPORT(GetGroupNum, KiwiSoundEngine_GetGroupNum),
	    HLL_EXPORT(IsExistFile, KiwiSoundEngine_IsExistFile),
	    HLL_EXPORT(GetLengthFromFile, KiwiSoundEngine_GetLengthFromFile),
	    HLL_EXPORT(GetGroupNumFromFile, KiwiSoundEngine_GetGroupNumFromFile),
	    HLL_EXPORT(GetGroupVolume, KiwiSoundEngine_GetGroupVolume),
	    HLL_EXPORT(MillisecondsToSamples, KiwiSoundEngine_MillisecondsToSamples),
	    HLL_EXPORT(GetSoundFileName, KiwiSoundEngine_GetSoundFileName),
	    HLL_EXPORT(_PreLink, KiwiSoundEngine_PreLink),
	    HLL_EXPORT(SetGlobalFocus, KiwiSoundEngine_SetGlobalFocus),
	    HLL_EXPORT(Music_IsExist, bgm_exists),
	    HLL_EXPORT(Music_Prepare, bgm_prepare),
	    HLL_EXPORT(Music_Unprepare, bgm_unprepare),
	    HLL_EXPORT(Music_Play, bgm_play),
	    HLL_EXPORT(Music_Stop, bgm_stop),
	    HLL_EXPORT(Music_IsPlay, bgm_is_playing),
	    HLL_EXPORT(Music_SetLoopCount, bgm_set_loop_count),
	    HLL_EXPORT(Music_GetLoopCount, bgm_get_loop_count),
	    HLL_EXPORT(Music_SetLoopStartPos, bgm_set_loop_start_pos),
	    HLL_EXPORT(Music_SetLoopEndPos, bgm_set_loop_end_pos),
	    HLL_EXPORT(Music_Fade, bgm_fade),
	    HLL_EXPORT(Music_StopFade, bgm_stop_fade),
	    HLL_EXPORT(Music_IsFade, bgm_is_fading),
	    HLL_EXPORT(Music_Pause, bgm_pause),
	    HLL_EXPORT(Music_Restart, bgm_restart),
	    HLL_EXPORT(Music_IsPause, bgm_is_paused),
	    HLL_EXPORT(Music_GetPos, bgm_get_pos),
	    HLL_EXPORT(Music_GetLength, bgm_get_length),
	    HLL_EXPORT(Music_GetSamplePos, bgm_get_sample_pos),
	    HLL_EXPORT(Music_GetSampleLength, bgm_get_sample_length),
	    HLL_EXPORT(Music_Seek, bgm_seek),
	    //HLL_EXPORT(Music_MillisecondsToSamples, KiwiSoundEngine_Music_MillisecondsToSamples),
	    //HLL_EXPORT(Music_GetFormat, KiwiSoundEngine_Music_GetFormat),
	    HLL_EXPORT(Sound_IsExist, wav_exists),
	    HLL_EXPORT(Sound_Prepare, wav_prepare),
	    HLL_EXPORT(Sound_Unprepare, wav_unprepare),
	    HLL_EXPORT(Sound_Play, wav_play),
	    HLL_EXPORT(Sound_Stop, wav_stop),
	    HLL_EXPORT(Sound_IsPlay, wav_is_playing),
	    HLL_EXPORT(Sound_Fade, wav_fade),
	    HLL_EXPORT(Sound_StopFade, wav_stop_fade),
	    HLL_EXPORT(Sound_IsFade, wav_is_fading),
	    HLL_EXPORT(Sound_GetTimeLength, wav_get_time_length),
	    HLL_TODO_EXPORT(Sound_GetGroupNum, KiwiSoundEngine_Sound_GetGroupNum),
	    HLL_EXPORT(Sound_GetGroupNumFromDataNum, wav_get_group_num_from_data_num),
	    HLL_TODO_EXPORT(Sound_PrepareFromFile, KiwiSoundEngine_Sound_PrepareFromFile),
	    HLL_EXPORT(Sound_GetDataLength, wav_get_data_length),
	    HLL_EXPORT(GetMasterGroup, KiwiSoundEngine_GetMasterGroup),
	    HLL_EXPORT(GetBGMGroup, KiwiSoundEngine_GetBGMGroup),
	    HLL_EXPORT(GetSEGroup, KiwiSoundEngine_GetSEGroup),
	    HLL_EXPORT(GetVoiceGroup, KiwiSoundEngine_GetVoiceGroup),
	    HLL_EXPORT(GetGimicSEGroup, KiwiSoundEngine_GetGimicSEGroup),
	    HLL_EXPORT(GetBackVoiceGroup, KiwiSoundEngine_GetBackVoiceGroup)
	);

static void KiwiSoundEngine_PreLink(void)
{
	int libno = ain_get_library(ain, "KiwiSoundEngine");
	if (libno < 0)
		return;
	if (ain_get_library_function(ain, libno, "Prepare") >= 0 && !config.mixer_nr_channels) {
		static const char *names[] = { "Master", "Music", "Sound", "Voice", "GimicSE", "BackVoice" };
		config.mixer_nr_channels = sizeof(names) / sizeof(*names);
		config.mixer_channels = xcalloc(config.mixer_nr_channels, sizeof(char*));
		config.mixer_volumes = xcalloc(config.mixer_nr_channels, sizeof(int));
		for (unsigned i = 0; i < config.mixer_nr_channels; i++) {
			config.mixer_channels[i] = xstrdup(names[i]);
			config.mixer_volumes[i] = config.default_volume;
		}
	}
	if (ain_get_library_function(ain, libno, "Sound_Seek") >= 0) {
		static_library_replace(&lib_KiwiSoundEngine, "Music_IsExist", rance9_bgm_exists);
		static_library_replace(&lib_KiwiSoundEngine, "Music_Prepare", rance9_bgm_prepare);
		static_library_replace(&lib_KiwiSoundEngine, "Sound_IsExist", rance9_wav_exists);
		static_library_replace(&lib_KiwiSoundEngine, "Sound_Prepare", rance9_wav_prepare);
	}
}
