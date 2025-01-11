SUBDIRS = sting

AM_CFLAGS = -Wall

bin_PROGRAMS = doom

header_files = am_map.h d_englsh.h d_event.h d_french.h d_items.h d_main.h \
	d_net.h doomdata.h doomdef.h doomstat.h doomtype.h d_player.h dstrings.h \
	d_textur.h d_think.h d_ticcmd.h f_finale.h f_wipe.h g_game.h hu_lib.h \
	hu_stuff.h i_net.h info.h i_sound.h i_sound_sdl.h i_sound_sb.h i_system.h i_video.h m_argv.h m_bbox.h \
	m_cheat.h m_fixed.h m_menu.h m_misc.h m_random.h m_swap.h p_inter.h \
	p_local.h p_mobj.h p_pspr.h p_saveg.h p_setup.h p_spec.h p_tick.h r_bsp.h \
	r_data.h r_defs.h r_draw.h r_local.h r_main.h r_plane.h r_segs.h r_sky.h \
	r_state.h r_things.h sounds.h s_sound.h st_lib.h st_stuff.h tables.h \
	v_video.h wi_stuff.h w_wad.h z_zone.h i_audio.h i_music.h \
    i_rgb2yuv.h i_cdmus.h \
    i_music_sdl.h i_music_opl.h i_music_midi.h mus2mid.h memio.h opl.h isa.h 

doom_SOURCES = am_map.c d_items.c d_main.c d_net.c doomstat.c \
	dstrings.c f_finale.c f_wipe.c g_game.c hu_lib.c hu_stuff.c i_main.c \
	i_net.c info.c i_sound.c i_sound_sdl.c i_sound_sb.c i_system.c i_video.c m_argv.c m_bbox.c m_cheat.c \
	m_fixed.c m_menu.c m_misc.c m_random.c m_swap.c p_ceilng.c p_doors.c \
	p_enemy.c p_floor.c p_inter.c p_lights.c p_map.c p_maputl.c p_mobj.c \
	p_plats.c p_pspr.c p_saveg.c p_setup.c p_sight.c p_spec.c p_switch.c \
	p_telept.c p_tick.c p_user.c r_bsp.c r_data.c r_draw.c r_main.c r_plane.c \
	r_segs.c r_sky.c r_things.c sounds.c s_sound.c st_lib.c st_stuff.c \
	tables.c v_video.c wi_stuff.c w_wad.c z_zone.c i_audio.c i_music.c \
	i_net_unix.c i_net_sting.c i_rgb2yuv.c i_cdmus.c \
    i_music_sdl.c i_music_opl.c i_music_midi.c mus2mid.c memio.c opl.c md_midi.c  \
	m_fixed_020.S m_fixed_060.S

EXTRA_DIST = $(header_files)
