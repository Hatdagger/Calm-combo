package com.hatdagger.calmcombo;

import android.app.Activity;
import android.content.res.AssetFileDescriptor;
import android.media.AudioAttributes;
import android.media.MediaPlayer;
import android.media.SoundPool;
import android.os.Bundle;
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
    private int track = 0;
    private boolean resumed = false;
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
        if (pool != null && id >= 0 && id < SFX.length && sfxId[id] != 0) pool.play(sfxId[id], 0.7f, 0.7f, 1, 0, 1f);
    }

    @Override
    public void setMusic(int t) {
        if (t == 0) {
            if (player != null) player.pause();
            return;
        }
        if (t == track && player != null) {
            if (resumed) player.start();
            return;
        }
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
            if (resumed) p.start();
        } catch (IOException | RuntimeException e) {
            player = null;                       /* no music file: play on in silence */
        }
    }

    @Override
    public void quit() { finish(); }

    @Override
    protected void onResume() {
        super.onResume();
        resumed = true;
        hideBars();
        if (view != null) { view.forceMusic(); view.start(); }
    }

    @Override
    protected void onPause() {
        resumed = false;
        if (view != null) view.stop();
        if (player != null && player.isPlaying()) player.pause();
        super.onPause();
    }

    @Override
    protected void onDestroy() {
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
