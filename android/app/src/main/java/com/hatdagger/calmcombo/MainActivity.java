package com.hatdagger.calmcombo;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.res.AssetFileDescriptor;
import android.media.AudioAttributes;
import android.media.AudioManager;
import android.media.MediaMetadata;
import android.media.MediaPlayer;
import android.media.SoundPool;
import android.media.session.MediaController;
import android.media.session.MediaSessionManager;
import android.media.session.PlaybackState;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.provider.Settings;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.TextView;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.text.Normalizer;
import java.util.List;

/** Calm Combo for Android: full screen, portrait, one board. */
public class MainActivity extends Activity implements GameView.Host {
    /* key ids shared with android/core/calm.h */
    static final int CK_UP = 0, CK_DOWN = 1, CK_LEFT = 2, CK_RIGHT = 3, CK_A = 4, CK_B = 5,
            CK_START = 6, CK_SELECT = 7, CK_X = 8, CK_Y = 9, CK_BACK = 10;

    /* sound file names, in the order of neogeo-cd/src/sound_ids.h */
    static final String[] SFX = { "grab", "no_match", "clear_1", "clear_2", "clear_3", "clear_4",
            "clear_5", "clear_6", "clear_7", "clear_8", "clear_9", "lever", "moves", "stones", "tick", "menu" };

    private static ByteBuffer pak;               /* the art, kept for the life of the app */
    private GameView view;
    private SoundPool pool;
    private final int[] sfxId = new int[SFX.length];
    private MediaPlayer player;
    private int track = 0, wantTrack = 0, failedTrack = -1;
    private boolean resumed = false;

    /* the phone's own music, shown in the bar under the board */
    private AudioManager am;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private MediaController ctl;               /* the other app's player, if we may see it */
    private boolean access;                    /* notification access: song titles available */
    private boolean extPlaying;                /* another app is playing: the game's songs stay quiet */
    private boolean hasFocus, focusBlocked;    /* audio focus for the game's songs */
    private long lastSfx, lastHeard, holdUntil;
    private String mediaLine = "";
    private final boolean[] hat = new boolean[4];

    @Override
    protected void onCreate(Bundle saved) {
        super.onCreate(saved);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        try {
            if (pak == null) pak = loadPak();
        } catch (IOException e) {
            fail("Could not read the game data: " + e.getMessage());
            return;
        }
        int r = GameView.nativeInit(pak, readSave());
        if (r != 0) { fail("The game data is damaged (" + r + ")."); return; }
        am = (AudioManager) getSystemService(AUDIO_SERVICE);
        view = new GameView(this, this);
        setContentView(view);
        view.requestFocus();

        pool = new SoundPool.Builder().setMaxStreams(6).setAudioAttributes(new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_GAME).setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION).build()).build();
        for (int k = 0; k < SFX.length; k++) {
            try (AssetFileDescriptor fd = getAssets().openFd("sfx/" + SFX[k] + ".wav")) {
                sfxId[k] = pool.load(fd, 1);
            } catch (IOException e) {
                sfxId[k] = 0;
            }
        }
    }

    private void fail(String msg) {
        TextView t = new TextView(this);
        t.setText(msg);
        t.setPadding(48, 48, 48, 48);
        setContentView(t);
    }

    private ByteBuffer loadPak() throws IOException {
        try (InputStream in = getAssets().open("calm.pak")) {
            ByteArrayOutputStream out = new ByteArrayOutputStream(8 << 20);
            byte[] buf = new byte[1 << 16];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            byte[] all = out.toByteArray();
            ByteBuffer bb = ByteBuffer.allocateDirect(all.length);
            bb.put(all);
            bb.rewind();
            return bb;
        }
    }

    private File saveFile() { return new File(getFilesDir(), "calm.sav"); }

    private byte[] readSave() {
        File f = saveFile();
        if (!f.exists()) return null;
        try (FileInputStream in = new FileInputStream(f)) {
            byte[] b = new byte[(int) Math.min(f.length(), 256)];
            int n = in.read(b);
            if (n <= 0) return null;
            if (n < b.length) { byte[] c = new byte[n]; System.arraycopy(b, 0, c, 0, n); return c; }
            return b;
        } catch (IOException e) {
            return null;
        }
    }

    @Override
    public void store(byte[] save) {
        File tmp = new File(getFilesDir(), "calm.sav.tmp");
        try (FileOutputStream out = new FileOutputStream(tmp)) {
            out.write(save);
            out.getFD().sync();
        } catch (IOException e) {
            return;
        }
        if (!tmp.renameTo(saveFile())) tmp.delete();
    }

    @Override
    public void playSound(int id) {
        lastSfx = SystemClock.uptimeMillis();
        if (pool != null && id >= 0 && id < SFX.length && sfxId[id] != 0) pool.play(sfxId[id], 0.7f, 0.7f, 1, 0, 1f);
    }

    @Override
    public void setMusic(int t) {
        wantTrack = t;
        applyMusic();
    }

    /* The game's song plays only while the game wants one and nothing else is playing. */
    private void applyMusic() {
        boolean play = resumed && wantTrack != 0 && !extPlaying && !focusBlocked;
        if (!play) {
            if (player != null && player.isPlaying()) player.pause();
            if (!focusBlocked) abandonFocus();   /* a short interruption keeps its claim, so focus comes back */
            return;
        }
        if (player == null || track != wantTrack) {
            if (wantTrack == failedTrack) return;
            loadTrack(wantTrack);
            if (player == null) return;
        }
        if (!player.isPlaying()) {
            requestFocus();
            if (hasFocus) player.start();
        }
    }

    private void loadTrack(int t) {
        if (player != null) { player.release(); player = null; }
        track = t;
        try (AssetFileDescriptor fd = getAssets().openFd("music/track" + t + ".ogg")) {
            MediaPlayer p = new MediaPlayer();
            p.setAudioAttributes(new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build());
            p.setDataSource(fd.getFileDescriptor(), fd.getStartOffset(), fd.getLength());
            p.setLooping(true);
            p.setVolume(0.8f, 0.8f);
            p.prepare();
            player = p;
        } catch (IOException | RuntimeException e) {
            player = null;                       /* no music file: play on in silence */
            failedTrack = t;
        }
    }

    /* ---------- the phone's music: focus, the other player, the bar ---------- */

    private final AudioManager.OnAudioFocusChangeListener focusListener = change -> {
        long now = SystemClock.uptimeMillis();
        if (change == AudioManager.AUDIOFOCUS_GAIN) {
            focusBlocked = false;
        } else if (change == AudioManager.AUDIOFOCUS_LOSS) {
            hasFocus = false;                    /* another app started playing */
            extPlaying = true;
            lastHeard = now;
            holdUntil = now + 1000;
        } else {
            focusBlocked = true;                 /* a call, an alarm, directions: wait for it to end */
        }
        applyMusic();
        pushMedia();
    };

    @SuppressWarnings("deprecation")
    private void requestFocus() {
        if (hasFocus || am == null) return;
        hasFocus = am.requestAudioFocus(focusListener, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN)
                == AudioManager.AUDIOFOCUS_REQUEST_GRANTED;
    }

    @SuppressWarnings("deprecation")
    private void abandonFocus() {
        focusBlocked = false;
        if (!hasFocus || am == null) return;
        am.abandonAudioFocus(focusListener);
        hasFocus = false;
    }

    private final Runnable poll = new Runnable() {
        @Override
        public void run() {
            pollMedia();
            if (resumed) handler.postDelayed(this, 500);
        }
    };

    private static boolean isPlaying(MediaController c) {
        PlaybackState st = c.getPlaybackState();
        if (st == null) return false;
        int s = st.getState();
        return s == PlaybackState.STATE_PLAYING || s == PlaybackState.STATE_BUFFERING;
    }

    /* Look at the other player twice a second: what it plays, and whether it plays. */
    private void pollMedia() {
        long now = SystemClock.uptimeMillis();
        MediaController c = null;
        boolean ok;
        try {
            MediaSessionManager m = (MediaSessionManager) getSystemService(MEDIA_SESSION_SERVICE);
            List<MediaController> list = m.getActiveSessions(new ComponentName(this, MediaListener.class));
            ok = true;
            for (MediaController x : list) {     /* the one playing, else the most recent */
                if (getPackageName().equals(x.getPackageName())) continue;
                if (c == null) c = x;
                if (isPlaying(x)) { c = x; break; }
            }
        } catch (SecurityException | NullPointerException e) {
            ok = false;                          /* no notification access: buttons still work */
        }
        access = ok;
        ctl = c;
        if (now >= holdUntil) {
            if (access) {
                extPlaying = ctl != null && isPlaying(ctl);
            } else if (am != null && (player == null || !player.isPlaying()) && now - lastSfx > 1500) {
                /* without access, listen: any music now that isn't the game's own effects or song */
                if (am.isMusicActive()) { extPlaying = true; lastHeard = now; }
                else if (extPlaying && now - lastHeard > 3000) extPlaying = false;
            }
        }
        mediaLine = "";
        if (ctl != null) {
            MediaMetadata md = ctl.getMetadata();
            if (md != null) {
                String title = ascii(md.getString(MediaMetadata.METADATA_KEY_TITLE));
                if (title.isEmpty()) title = ascii(md.getString(MediaMetadata.METADATA_KEY_DISPLAY_TITLE));
                String artist = ascii(md.getString(MediaMetadata.METADATA_KEY_ARTIST));
                if (artist.isEmpty()) artist = ascii(md.getString(MediaMetadata.METADATA_KEY_ALBUM_ARTIST));
                mediaLine = title.isEmpty() ? artist : artist.isEmpty() ? title : title + " - " + artist;
                if (mediaLine.length() > 96) mediaLine = mediaLine.substring(0, 96);
            }
        }
        applyMusic();
        pushMedia();
    }

    private void pushMedia() {
        if (view == null) return;
        int flags = (extPlaying ? 1 : 0) | (access ? 2 : 0) | (ctl != null ? 4 : 0);
        GameView.nativeMedia(flags, mediaLine);
    }

    /* The game's font is plain ASCII: keep the letters, drop accents and what it can't draw. */
    static String ascii(String s) {
        if (s == null) return "";
        s = s.replace('\u2018', '\'').replace('\u2019', '\'').replace('\u201C', '"').replace('\u201D', '"')
                .replace('\u2013', '-').replace('\u2014', '-').replace("\u2026", "...");
        s = Normalizer.normalize(s, Normalizer.Form.NFD);
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < s.length(); i++) {
            char ch = s.charAt(i);
            if (ch >= 32 && ch < 127) b.append(ch);
            else if (Character.isWhitespace(ch)) b.append(' ');
        }
        return b.toString().replaceAll(" +", " ").trim();
    }

    /* a tap on the bar: previous 0, play/pause 1, next 2, show song titles 3 */
    @Override
    public void mediaAction(int a) {
        long now = SystemClock.uptimeMillis();
        if (a == 3) { askAccess(); return; }
        if (ctl != null) {
            MediaController.TransportControls tc = ctl.getTransportControls();
            if (a == 0) tc.skipToPrevious();
            else if (a == 2) tc.skipToNext();
            else if (isPlaying(ctl)) tc.pause();
            else tc.play();
        } else if (am != null) {
            int code = a == 0 ? KeyEvent.KEYCODE_MEDIA_PREVIOUS : a == 2 ? KeyEvent.KEYCODE_MEDIA_NEXT
                    : KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE;
            am.dispatchMediaKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, code));
            am.dispatchMediaKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, code));
        }
        /* show the change at once; the next looks confirm it */
        extPlaying = a == 1 ? !extPlaying : true;
        lastHeard = now;
        holdUntil = now + 1200;
        applyMusic();
        pushMedia();
    }

    private void askAccess() {
        String flat = new ComponentName(this, MediaListener.class).flattenToString();
        if (Build.VERSION.SDK_INT >= 30) {
            try {
                Intent i = new Intent(Settings.ACTION_NOTIFICATION_LISTENER_DETAIL_SETTINGS);
                i.putExtra(Settings.EXTRA_NOTIFICATION_LISTENER_COMPONENT_NAME, flat);
                startActivity(i);
                return;
            } catch (RuntimeException ignored) {
                /* fall through to the list */
            }
        }
        try {
            startActivity(new Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS));
        } catch (RuntimeException ignored) {
            /* no such screen on this device */
        }
    }

    @Override
    public void quit() { finish(); }

    @Override
    protected void onResume() {
        super.onResume();
        resumed = true;
        hideBars();
        pollMedia();                             /* music already playing on the phone stays on */
        handler.removeCallbacks(poll);
        handler.postDelayed(poll, 500);
        if (view != null) { view.forceMusic(); view.start(); }
    }

    @Override
    protected void onPause() {
        resumed = false;
        if (view != null) view.stop();
        handler.removeCallbacks(poll);
        if (player != null && player.isPlaying()) player.pause();
        abandonFocus();
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        handler.removeCallbacks(poll);
        abandonFocus();
        if (player != null) { player.release(); player = null; }
        if (pool != null) { pool.release(); pool = null; }
        super.onDestroy();
    }

    @Override
    public void onWindowFocusChanged(boolean focus) {
        super.onWindowFocusChanged(focus);
        if (focus) hideBars();
    }

    @SuppressWarnings("deprecation")
    private void hideBars() {
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
    }

    /* ---------- controllers and keyboards ---------- */

    private static int keyOf(int code) {
        switch (code) {
            case KeyEvent.KEYCODE_DPAD_UP: case KeyEvent.KEYCODE_W: return CK_UP;
            case KeyEvent.KEYCODE_DPAD_DOWN: case KeyEvent.KEYCODE_S: return CK_DOWN;
            case KeyEvent.KEYCODE_DPAD_LEFT: case KeyEvent.KEYCODE_A: return CK_LEFT;
            case KeyEvent.KEYCODE_DPAD_RIGHT: case KeyEvent.KEYCODE_D: return CK_RIGHT;
            case KeyEvent.KEYCODE_BUTTON_A: case KeyEvent.KEYCODE_DPAD_CENTER:
            case KeyEvent.KEYCODE_SPACE: case KeyEvent.KEYCODE_F: return CK_A;
            case KeyEvent.KEYCODE_BUTTON_B: case KeyEvent.KEYCODE_G: return CK_B;
            case KeyEvent.KEYCODE_BUTTON_START: case KeyEvent.KEYCODE_ENTER: return CK_START;
            case KeyEvent.KEYCODE_BUTTON_SELECT: case KeyEvent.KEYCODE_ESCAPE: return CK_SELECT;
            case KeyEvent.KEYCODE_BUTTON_X: case KeyEvent.KEYCODE_N: return CK_X;
            case KeyEvent.KEYCODE_BUTTON_Y: case KeyEvent.KEYCODE_M: return CK_Y;
            case KeyEvent.KEYCODE_BACK: return CK_BACK;
            default: return -1;
        }
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent e) {
        int k = keyOf(e.getKeyCode());
        if (k < 0 || view == null) return super.dispatchKeyEvent(e);
        if (e.getAction() == KeyEvent.ACTION_DOWN) { if (e.getRepeatCount() == 0) GameView.nativeKey(k, true); }
        else if (e.getAction() == KeyEvent.ACTION_UP) GameView.nativeKey(k, false);
        return true;
    }

    /* d-pads that report as a hat, and analog sticks, act as the d-pad */
    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent e) {
        if (view == null || (e.getSource() & InputDevice.SOURCE_JOYSTICK) != InputDevice.SOURCE_JOYSTICK
                || e.getAction() != MotionEvent.ACTION_MOVE) return super.dispatchGenericMotionEvent(e);
        float x = e.getAxisValue(MotionEvent.AXIS_HAT_X), y = e.getAxisValue(MotionEvent.AXIS_HAT_Y);
        if (Math.abs(x) < 0.5f) x = e.getAxisValue(MotionEvent.AXIS_X);
        if (Math.abs(y) < 0.5f) y = e.getAxisValue(MotionEvent.AXIS_Y);
        setHat(0, y < -0.5f, CK_UP);
        setHat(1, y > 0.5f, CK_DOWN);
        setHat(2, x < -0.5f, CK_LEFT);
        setHat(3, x > 0.5f, CK_RIGHT);
        return true;
    }

    private void setHat(int i, boolean on, int key) {
        if (hat[i] != on) { hat[i] = on; GameView.nativeKey(key, on); }
    }
}
