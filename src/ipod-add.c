// SPDX-License-Identifier: LGPL-2.1-or-later
#include <gpod/itdb.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static void usage(const char *name) {
    fprintf(stderr,
        "Usage: %s [--mount PATH] [--title TEXT] [--artist TEXT]\n"
        "          [--album TEXT] [--music|--audiobook|--video|--photo] FILE\n"
        "\n"
        "Audio: MP3, AAC/M4A, WAV and other Apple-compatible audio.\n"
        "Video: Apple-compatible MP4, M4V or MOV (H.264 or MPEG-4 video).\n"
        "Photo: JPEG, PNG, TIFF, BMP or GIF; --album selects a photo album.\n", name);
}

static int report_error(GError *error, const char *context) {
    fprintf(stderr, "ipod-add: %s: %s\n", context,
            error && error->message ? error->message : "unknown error");
    if (error) g_error_free(error);
    return 1;
}

static gchar *find_mount(void) {
    const gchar *user = g_get_user_name();
    gchar *base = g_build_filename("/media", user, NULL);
    GDir *dir = g_dir_open(base, 0, NULL);
    if (!dir) {
        g_free(base);
        return NULL;
    }
    const gchar *name;
    gchar *found = NULL;
    while ((name = g_dir_read_name(dir))) {
        gchar *candidate = g_build_filename(base, name, NULL);
        gchar *db = g_build_filename(candidate, "iPod_Control", "iTunes",
                                     "iTunesDB", NULL);
        if (g_file_test(db, G_FILE_TEST_IS_REGULAR)) {
            found = candidate;
            g_free(db);
            break;
        }
        g_free(db);
        g_free(candidate);
    }
    g_dir_close(dir);
    g_free(base);
    return found;
}

static gchar *value_from_probe(const gchar *output, const gchar *wanted) {
    gchar **lines = g_strsplit(output ? output : "", "\n", -1);
    gchar *result = NULL;
    for (gint i = 0; lines[i]; i++) {
        gchar *equals = strchr(lines[i], '=');
        if (!equals) continue;
        *equals = '\0';
        const gchar *key = lines[i];
        if (g_ascii_strncasecmp(key, "TAG:", 4) == 0) key += 4;
        if (g_ascii_strcasecmp(key, wanted) == 0) {
            result = g_strdup(equals + 1);
            break;
        }
    }
    g_strfreev(lines);
    return result;
}

static gboolean has_extension(const gchar *path, const gchar *const *extensions) {
    const gchar *dot = strrchr(path, '.');
    if (!dot) return FALSE;
    for (gint i = 0; extensions[i]; i++)
        if (g_ascii_strcasecmp(dot + 1, extensions[i]) == 0) return TRUE;
    return FALSE;
}

int main(int argc, char **argv) {
    gchar *mount = NULL, *title = NULL, *artist = NULL, *album = NULL;
    gboolean force_music = FALSE, force_audiobook = FALSE;
    gboolean force_video = FALSE, force_photo = FALSE;
    const gchar *source = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--mount") == 0 && i + 1 < argc) mount = g_strdup(argv[++i]);
        else if (strcmp(argv[i], "--title") == 0 && i + 1 < argc) title = g_strdup(argv[++i]);
        else if (strcmp(argv[i], "--artist") == 0 && i + 1 < argc) artist = g_strdup(argv[++i]);
        else if (strcmp(argv[i], "--album") == 0 && i + 1 < argc) album = g_strdup(argv[++i]);
        else if (strcmp(argv[i], "--music") == 0) force_music = TRUE;
        else if (strcmp(argv[i], "--audiobook") == 0) force_audiobook = TRUE;
        else if (strcmp(argv[i], "--video") == 0) force_video = TRUE;
        else if (strcmp(argv[i], "--photo") == 0) force_photo = TRUE;
        else if (argv[i][0] == '-') { usage(argv[0]); return 2; }
        else if (!source) source = argv[i];
        else { usage(argv[0]); return 2; }
    }
    if (!source || ((force_music ? 1 : 0) + (force_audiobook ? 1 : 0) +
                    (force_video ? 1 : 0) + (force_photo ? 1 : 0) > 1)) {
        usage(argv[0]);
        return 2;
    }

    struct stat st;
    if (stat(source, &st) != 0) {
        perror("ipod-add: input file");
        return 1;
    }
    if (!mount) mount = find_mount();
    if (!mount) {
        fprintf(stderr, "ipod-add: no mounted iPod was found under /media/%s\n",
                g_get_user_name());
        return 1;
    }

    static const gchar *const image_exts[] = {
        "jpg", "jpeg", "png", "tif", "tiff", "bmp", "gif", NULL
    };
    gboolean image = force_photo ||
                     (!force_music && !force_audiobook && !force_video &&
                      has_extension(source, image_exts));
    if (image) {
        GError *photo_error = NULL;
        Itdb_PhotoDB *photodb = itdb_photodb_parse(mount, &photo_error);
        if (!photodb) {
            if (photo_error) g_error_free(photo_error);
            photo_error = NULL;
            photodb = itdb_photodb_create(mount);
        }
        if (!photodb) {
            g_free(mount);
            fprintf(stderr, "ipod-add: could not open or create the iPod photo database\n");
            return 1;
        }
        Itdb_Artwork *photo = itdb_photodb_add_photo(
            photodb, source, -1, 0, &photo_error);
        if (!photo) {
            itdb_photodb_free(photodb);
            g_free(mount);
            return report_error(photo_error, "could not import the image");
        }
        const gchar *album_name = album && *album ? album : "Imported";
        Itdb_PhotoAlbum *photo_album =
            itdb_photodb_photoalbum_by_name(photodb, (gchar *)album_name);
        if (!photo_album)
            photo_album = itdb_photodb_photoalbum_create(photodb, album_name, -1);
        if (photo_album)
            itdb_photodb_photoalbum_add_photo(photodb, photo_album, photo, -1);
        if (!itdb_photodb_write(photodb, &photo_error)) {
            itdb_photodb_free(photodb);
            g_free(mount);
            return report_error(photo_error, "could not update the Apple photo database");
        }
        printf("Added to Apple Photos album \"%s\": %s\n", album_name, source);
        itdb_photodb_free(photodb);
        g_free(mount); g_free(title); g_free(artist); g_free(album);
        return 0;
    }

    gchar *probe_out = NULL, *probe_err = NULL;
    gint probe_status = 0;
    gchar *probe_argv[] = {
        "ffprobe", "-v", "error", "-show_entries",
        "format=duration:format_tags=title,artist,album,genre:"
        "stream=codec_type,codec_name,width,height",
        "-of", "default=noprint_wrappers=1", (gchar *)source, NULL
    };
    GError *error = NULL;
    if (!g_spawn_sync(NULL, probe_argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                      &probe_out, &probe_err, &probe_status, &error) ||
        probe_status != 0) {
        if (error) g_error_free(error);
        fprintf(stderr, "ipod-add: ffprobe could not read the media file: %s\n",
                probe_err ? probe_err : "unknown error");
        g_free(probe_out); g_free(probe_err); g_free(mount);
        return 1;
    }

    gchar *duration_s = value_from_probe(probe_out, "duration");
    gchar *tag_title = value_from_probe(probe_out, "title");
    gchar *tag_artist = value_from_probe(probe_out, "artist");
    gchar *tag_album = value_from_probe(probe_out, "album");
    gchar *tag_genre = value_from_probe(probe_out, "genre");
    gchar *codec_type = value_from_probe(probe_out, "codec_type");
    gchar *codec_name = value_from_probe(probe_out, "codec_name");
    gboolean probe_has_video =
        strstr(probe_out ? probe_out : "", "codec_type=video") != NULL;
    gboolean probe_has_apple_video =
        strstr(probe_out ? probe_out : "", "codec_name=h264") != NULL ||
        strstr(probe_out ? probe_out : "", "codec_name=mpeg4") != NULL;
    g_free(probe_out); g_free(probe_err);
    gdouble duration = duration_s ? g_ascii_strtod(duration_s, NULL) : 0.0;
    g_free(duration_s);
    gboolean video = force_video ||
                     (!force_music && !force_audiobook && probe_has_video);
    if (video) {
        static const gchar *const video_exts[] = {"mp4", "m4v", "mov", NULL};
        if (!has_extension(source, video_exts) || !probe_has_apple_video) {
            fprintf(stderr,
                "ipod-add: Apple UI requires MP4/M4V/MOV with H.264 or MPEG-4 "
                "video; convert this file first with ffmpeg\n");
            g_free(mount);
            g_free(codec_type); g_free(codec_name);
            return 1;
        }
    }

    if (!title) title = tag_title && *tag_title ? g_strdup(tag_title) : NULL;
    if (!title) {
        gchar *base = g_path_get_basename(source);
        gchar *dot = strrchr(base, '.');
        if (dot) *dot = '\0';
        title = g_strdup(base);
        g_free(base);
    }
    if (!artist) artist = tag_artist && *tag_artist ? g_strdup(tag_artist) : g_strdup("Unknown Artist");
    if (!album) album = tag_album && *tag_album ? g_strdup(tag_album) : g_strdup(title);
    gboolean audiobook = !video &&
        (force_audiobook || (!force_music && duration >= 3600.0));

    Itdb_iTunesDB *db = itdb_parse(mount, &error);
    if (!db) return report_error(error, "could not read the iPod database");
    for (GList *node = db->tracks; node; node = node->next) {
        Itdb_Track *old = node->data;
        if (old->title && old->artist &&
            strcmp(old->title, title) == 0 && strcmp(old->artist, artist) == 0 &&
            old->size == (guint32)st.st_size) {
            fprintf(stderr, "ipod-add: already present: %s — %s\n", title, artist);
            itdb_free(db);
            return 3;
        }
    }

    Itdb_Track *track = itdb_track_new();
    track->title = g_strdup(title);
    track->album = g_strdup(album);
    track->artist = g_strdup(artist);
    track->albumartist = g_strdup(artist);
    track->genre = g_strdup(tag_genre && *tag_genre ? tag_genre :
                            (video ? "Video" : (audiobook ? "Audiobook" : "Music")));
    track->filetype = g_strdup(video ? "MPEG-4 video file" : "MPEG audio file");
    track->size = (guint32)st.st_size;
    track->tracklen = (gint32)(duration * 1000.0 + 0.5);
    track->bitrate = duration > 0 ? (gint32)((st.st_size * 8.0 / duration) / 1000.0 + 0.5) : 0;
    track->time_added = time(NULL);
    track->time_modified = st.st_mtime;
    track->mediatype = video ? ITDB_MEDIATYPE_MOVIE :
                       (audiobook ? ITDB_MEDIATYPE_AUDIOBOOK : ITDB_MEDIATYPE_AUDIO);
    track->movie_flag = video ? 1 : 0;
    track->remember_playback_position = audiobook ? 1 : 0;
    track->skip_when_shuffling = audiobook ? 1 : 0;
    track->mark_unplayed = 1;
    track->visible = 1;
    track->type2 = 1;

    itdb_track_add(db, track, -1);
    Itdb_Playlist *master = itdb_playlist_mpl(db);
    if (!master) {
        itdb_track_remove(track);
        itdb_free(db);
        fprintf(stderr, "ipod-add: iPod database has no master playlist\n");
        return 1;
    }
    itdb_playlist_add_track(master, track, -1);
    if (!itdb_cp_track_to_ipod(track, source, &error)) {
        itdb_track_remove(track);
        itdb_free(db);
        return report_error(error, "could not copy the audio file");
    }
    if (!itdb_write(db, &error)) {
        itdb_free(db);
        return report_error(error, "could not update the Apple database");
    }

    gchar *destination = itdb_filename_on_ipod(track);
    printf("Added to Apple UI as %s: %s — %s\nStored at: %s\n",
           video ? "video" : (audiobook ? "audiobook" : "music"), title, artist,
           destination ? destination : "(unknown)");
    g_free(destination);
    itdb_free(db);
    g_free(mount); g_free(title); g_free(artist); g_free(album);
    g_free(tag_title); g_free(tag_artist); g_free(tag_album); g_free(tag_genre);
    g_free(codec_type); g_free(codec_name);
    return 0;
}
